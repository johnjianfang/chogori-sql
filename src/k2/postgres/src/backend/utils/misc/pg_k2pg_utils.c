/*-------------------------------------------------------------------------
 *
 * pg_k2pg_utils.c
 *	  Utilities for YugaByte/PostgreSQL integration that have to be defined on
 *	  the PostgreSQL side.
 *
 * Copyright (c) YugaByte, Inc.
 * Portions Copyright (c) 2021 Futurewei Cloud
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 * IDENTIFICATION
 *	  src/backend/utils/misc/pg_k2pg_utils.c
 *
 *-------------------------------------------------------------------------
 */

#include <sys/types.h>
#include <unistd.h>

#include "postgres.h"
#include "miscadmin.h"
#include "access/sysattr.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "catalog/pg_database.h"
#include "utils/builtins.h"
#include "catalog/pg_type.h"
#include "catalog/catalog.h"
#include "commands/dbcommands.h"

#include "pg_k2pg_utils.h"
#include "catalog/ybctype.h"

#include "common/k2pg_util.h"
#include "pggate/pg_gate_api.h"
#include "common/pg_k2pg_common.h"

#include "utils/resowner_private.h"

#include "fmgr.h"
#include "access/htup.h"
#include "access/htup_details.h"
#include "access/tupdesc.h"

#include "tcop/utility.h"

uint64_t k2pg_catalog_cache_version = K2PG_CATCACHE_VERSION_UNINITIALIZED;

/** These values are lazily initialized based on corresponding environment variables. */
int k2pg_pg_double_write = -1;
int k2pg_disable_pg_locking = -1;

/* Forward declarations */
static void K2PgInstallTxnDdlHook();

bool
IsK2PgEnabled()
{
	/* We do not support Init/Bootstrap processing modes yet. */
	return PgGate_IsK2PgEnabled();
}

void
CheckIsK2PgSupportedRelation(Relation relation)
{
	const char relkind = relation->rd_rel->relkind;
	CheckIsK2PgSupportedRelationByKind(relkind);
}

void
CheckIsK2PgSupportedRelationByKind(char relkind)
{
	if (!(relkind == RELKIND_RELATION || relkind == RELKIND_INDEX ||
		  relkind == RELKIND_VIEW || relkind == RELKIND_SEQUENCE ||
		  relkind == RELKIND_COMPOSITE_TYPE))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
								errmsg("This feature is not supported in K2PG.")));
}

bool
IsK2PgRelation(Relation relation)
{
	if (!IsK2PgEnabled()) return false;

	const char relkind = relation->rd_rel->relkind;

	CheckIsK2PgSupportedRelationByKind(relkind);

	/* Currently only support regular tables and indexes.
	 * Temp tables and views are supported, but they are not K2PG relations. */
	return (relkind == RELKIND_RELATION || relkind == RELKIND_INDEX)
				 && relation->rd_rel->relpersistence != RELPERSISTENCE_TEMP;
}

bool
IsK2PgRelationById(Oid relid)
{
	Relation relation     = RelationIdGetRelation(relid);
	bool     is_supported = IsK2PgRelation(relation);
	RelationClose(relation);
	return is_supported;
}

bool
IsK2PgBackedRelation(Relation relation)
{
	return IsK2PgRelation(relation) ||
		(relation->rd_rel->relkind == RELKIND_VIEW &&
		relation->rd_rel->relpersistence != RELPERSISTENCE_TEMP);
}

bool
K2PgNeedRetryAfterCacheRefresh(ErrorData *edata)
{
	// TODO Inspect error code to distinguish retryable errors.
	return true;
}

AttrNumber K2PgGetFirstLowInvalidAttributeNumber(Relation relation)
{
	return IsK2PgRelation(relation)
	       ? K2PgFirstLowInvalidAttributeNumber
	       : FirstLowInvalidHeapAttributeNumber;
}

AttrNumber K2PgGetFirstLowInvalidAttributeNumberFromOid(Oid relid)
{
	Relation   relation = RelationIdGetRelation(relid);
	AttrNumber attr_num = K2PgGetFirstLowInvalidAttributeNumber(relation);
	RelationClose(relation);
	return attr_num;
}

int K2PgAttnumToBmsIndex(Relation rel, AttrNumber attnum)
{
	return attnum - K2PgGetFirstLowInvalidAttributeNumber(rel);
}

AttrNumber K2PgBmsIndexToAttnum(Relation rel, int idx)
{
	return idx + K2PgGetFirstLowInvalidAttributeNumber(rel);
}


extern bool K2PgRelHasOldRowTriggers(Relation rel, CmdType operation)
{
	TriggerDesc *trigdesc = rel->trigdesc;
	return (trigdesc &&
		((operation == CMD_UPDATE &&
			(trigdesc->trig_update_after_row ||
			trigdesc->trig_update_before_row)) ||
		(operation == CMD_DELETE &&
			(trigdesc->trig_delete_after_row ||
			trigdesc->trig_delete_before_row))));
}

bool
K2PgRelHasSecondaryIndices(Relation relation)
{
	if (!relation->rd_rel->relhasindex)
		return false;

	bool	 has_indices = false;
	List	 *indexlist = RelationGetIndexList(relation);
	ListCell *lc;

	foreach(lc, indexlist)
	{
		if (lfirst_oid(lc) == relation->rd_pkindex)
			continue;
		has_indices = true;
		break;
	}

	list_free(indexlist);

	return has_indices;
}

bool
K2PgTransactionsEnabled()
{
	static int cached_value = -1;
	if (cached_value == -1)
	{
		cached_value = K2PgIsEnvVarTrueWithDefault("K2PG_TRANSACTIONS_ENABLED", true);
	}
	return IsK2PgEnabled() && cached_value;
}

void
K2PgReportFeatureUnsupported(const char *msg)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("%s", msg)));
}


static bool
K2PgShouldReportErrorStatus()
{
	static int cached_value = -1;
	if (cached_value == -1)
	{
		cached_value = K2PgIsEnvVarTrue("K2PG_REPORT_ERROR_STATUS");
	}

	return cached_value;
}

void
HandleK2PgStatus(K2PgStatus status)
{
	if (!status) {
		return;
	}
	/* Copy the message to the current memory context and free the K2PgStatus. */
	const uint32_t pg_err_code = K2PgStatusPgsqlError(status);
	char* msg_buf = DupK2PgStatusMessage(status, pg_err_code == ERRCODE_UNIQUE_VIOLATION);

	if (K2PgShouldReportErrorStatus()) {
		K2PG_LOG_ERROR("HandleK2PgStatus: %s", msg_buf);
	}
	const uint16_t txn_err_code = K2PgStatusTransactionError(status);
	K2PgFreeStatus(status);
	ereport(ERROR,
			(errmsg("%s", msg_buf),
			 errcode(pg_err_code),
			 k2pg_txn_errcode(txn_err_code),
			 errhidecontext(true)));
}

void
HandleK2PgStatusIgnoreNotFound(K2PgStatus status, bool *not_found)
{
	if (!status) {
		return;
	}
	if (K2PgStatusIsNotFound(status)) {
		*not_found = true;
		K2PgFreeStatus(status);
		return;
	}
	*not_found = false;
	HandleK2PgStatus(status);
}

void
HandleK2PgStatusWithOwner(K2PgStatus status,
												K2PgStatement k2pg_stmt,
												ResourceOwner owner)
{
	if (!status)
		return;

	if (k2pg_stmt)
	{
		if (owner != NULL)
		{
			ResourceOwnerForgetK2PgStmt(owner, k2pg_stmt);
		}
	}
	HandleK2PgStatus(status);
}

void
HandleK2PgTableDescStatus(K2PgStatus status, K2PgTableDesc table)
{
	if (!status)
		return;

	HandleK2PgStatus(status);
}

/*
 * Fetches relation's unique constraint name to specified buffer.
 * If relation is not an index and it has primary key the name of primary key index is returned.
 * In other cases, relation name is used.
 */
static void
FetchUniqueConstraintName(Oid relation_id, char* dest, size_t max_size)
{
	// strncat appends source to destination, so destination must be empty.
	dest[0] = 0;
	Relation rel = RelationIdGetRelation(relation_id);

	if (!rel->rd_index && rel->rd_pkindex != InvalidOid)
	{
		Relation pkey = RelationIdGetRelation(rel->rd_pkindex);

		strncat(dest, RelationGetRelationName(pkey), max_size);

		RelationClose(pkey);
	} else
	{
		strncat(dest, RelationGetRelationName(rel), max_size);
	}

	RelationClose(rel);
}

void
K2PgInitPostgresBackend(
	const char *program_name,
	const char *db_name,
	const char *user_name)
{
	HandleK2PgStatus(K2PgInit(program_name, palloc, cstring_to_text_with_len));

	/*
	 * Enable "K2PG mode" for PostgreSQL so that we will initiate a connection
	 * to the K2 platform cluster right away from every backend process. We only

	 * do this if this env variable is set, so we can still run the regular
	 * PostgreSQL "make check".
	 */
	if (K2PgIsEnabledInPostgresEnvVar())
	{
		const K2PgTypeEntity *type_table;
		int count;
		K2PgGetTypeTable(&type_table, &count);
		K2PgCallbacks callbacks;
		callbacks.FetchUniqueConstraintName = &FetchUniqueConstraintName;
		callbacks.GetCurrentYbMemctx = &GetCurrentYbMemctx;
		PgGate_InitPgGate(type_table, count, callbacks);
		K2PgInstallTxnDdlHook();

		/*
		 * For each process, we create one K2PG session for PostgreSQL to use
		 * when accessing K2PG storage.
		 *
		 * TODO: do we really need to DB name / username here?
		 */
    HandleK2PgStatus(PgGate_InitSession(/* pg_env */ NULL, db_name ? db_name : user_name));
	}
}

void
K2PgOnPostgresBackendShutdown()
{
	PgGate_DestroyPgGate();
}

void
K2PgRestartTransaction()
{
	if (!IsK2PgEnabled())
		return;
	HandleK2PgStatus(PgGate_RestartTransaction());
}

void
K2PgCommitTransaction()
{
	if (!IsK2PgEnabled())
		return;

	HandleK2PgStatus(PgGate_CommitTransaction());
}

void
K2PgAbortTransaction()
{
	if (!IsK2PgEnabled())
		return;

	if (K2PgTransactionsEnabled())
		HandleK2PgStatus(PgGate_AbortTransaction());
}

bool
K2PgIsPgLockingEnabled()
{
	return !K2PgTransactionsEnabled();
}

static bool k2pg_preparing_templates = false;
void
K2PgSetPreparingTemplates() {
	k2pg_preparing_templates = true;
}

bool
K2PgIsPreparingTemplates() {
	return k2pg_preparing_templates;
}

const char*
K2PgTypeOidToStr(Oid type_id) {
	switch (type_id) {
		case BOOLOID: return "BOOL";
		case BYTEAOID: return "BYTEA";
		case CHAROID: return "CHAR";
		case NAMEOID: return "NAME";
		case INT8OID: return "INT8";
		case INT2OID: return "INT2";
		case INT2VECTOROID: return "INT2VECTOR";
		case INT4OID: return "INT4";
		case REGPROCOID: return "REGPROC";
		case TEXTOID: return "TEXT";
		case OIDOID: return "OID";
		case TIDOID: return "TID";
		case XIDOID: return "XID";
		case CIDOID: return "CID";
		case OIDVECTOROID: return "OIDVECTOR";
		case JSONOID: return "JSON";
		case XMLOID: return "XML";
		case PGNODETREEOID: return "PGNODETREE";
		case PGNDISTINCTOID: return "PGNDISTINCT";
		case PGDEPENDENCIESOID: return "PGDEPENDENCIES";
		case PGDDLCOMMANDOID: return "PGDDLCOMMAND";
		case POINTOID: return "POINT";
		case LSEGOID: return "LSEG";
		case PATHOID: return "PATH";
		case BOXOID: return "BOX";
		case POLYGONOID: return "POLYGON";
		case LINEOID: return "LINE";
		case FLOAT4OID: return "FLOAT4";
		case FLOAT8OID: return "FLOAT8";
		case ABSTIMEOID: return "ABSTIME";
		case RELTIMEOID: return "RELTIME";
		case TINTERVALOID: return "TINTERVAL";
		case UNKNOWNOID: return "UNKNOWN";
		case CIRCLEOID: return "CIRCLE";
		case CASHOID: return "CASH";
		case MACADDROID: return "MACADDR";
		case INETOID: return "INET";
		case CIDROID: return "CIDR";
		case MACADDR8OID: return "MACADDR8";
		case INT2ARRAYOID: return "INT2ARRAY";
		case INT4ARRAYOID: return "INT4ARRAY";
		case TEXTARRAYOID: return "TEXTARRAY";
		case OIDARRAYOID: return "OIDARRAY";
		case FLOAT4ARRAYOID: return "FLOAT4ARRAY";
		case ACLITEMOID: return "ACLITEM";
		case CSTRINGARRAYOID: return "CSTRINGARRAY";
		case BPCHAROID: return "BPCHAR";
		case VARCHAROID: return "VARCHAR";
		case DATEOID: return "DATE";
		case TIMEOID: return "TIME";
		case TIMESTAMPOID: return "TIMESTAMP";
		case TIMESTAMPTZOID: return "TIMESTAMPTZ";
		case INTERVALOID: return "INTERVAL";
		case TIMETZOID: return "TIMETZ";
		case BITOID: return "BIT";
		case VARBITOID: return "VARBIT";
		case NUMERICOID: return "NUMERIC";
		case REFCURSOROID: return "REFCURSOR";
		case REGPROCEDUREOID: return "REGPROCEDURE";
		case REGOPEROID: return "REGOPER";
		case REGOPERATOROID: return "REGOPERATOR";
		case REGCLASSOID: return "REGCLASS";
		case REGTYPEOID: return "REGTYPE";
		case REGROLEOID: return "REGROLE";
		case REGNAMESPACEOID: return "REGNAMESPACE";
		case REGTYPEARRAYOID: return "REGTYPEARRAY";
		case UUIDOID: return "UUID";
		case LSNOID: return "LSN";
		case TSVECTOROID: return "TSVECTOR";
		case GTSVECTOROID: return "GTSVECTOR";
		case TSQUERYOID: return "TSQUERY";
		case REGCONFIGOID: return "REGCONFIG";
		case REGDICTIONARYOID: return "REGDICTIONARY";
		case JSONBOID: return "JSONB";
		case INT4RANGEOID: return "INT4RANGE";
		case RECORDOID: return "RECORD";
		case RECORDARRAYOID: return "RECORDARRAY";
		case CSTRINGOID: return "CSTRING";
		case ANYOID: return "ANY";
		case ANYARRAYOID: return "ANYARRAY";
		case VOIDOID: return "VOID";
		case TRIGGEROID: return "TRIGGER";
		case EVTTRIGGEROID: return "EVTTRIGGER";
		case LANGUAGE_HANDLEROID: return "LANGUAGE_HANDLER";
		case INTERNALOID: return "INTERNAL";
		case OPAQUEOID: return "OPAQUE";
		case ANYELEMENTOID: return "ANYELEMENT";
		case ANYNONARRAYOID: return "ANYNONARRAY";
		case ANYENUMOID: return "ANYENUM";
		case FDW_HANDLEROID: return "FDW_HANDLER";
		case INDEX_AM_HANDLEROID: return "INDEX_AM_HANDLER";
		case TSM_HANDLEROID: return "TSM_HANDLER";
		case ANYRANGEOID: return "ANYRANGE";
		default: return "user_defined_type";
	}
}

const char*
K2PgDataTypeToStr(K2PgDataType k2pg_type) {
	switch (k2pg_type) {
		case K2SQL_DATA_TYPE_NOT_SUPPORTED: return "NOT_SUPPORTED";
		case K2SQL_DATA_TYPE_UNKNOWN_DATA: return "UNKNOWN_DATA";
		case K2SQL_DATA_TYPE_NULL_VALUE_TYPE: return "NULL_VALUE_TYPE";
		case K2SQL_DATA_TYPE_INT8: return "INT8";
		case K2SQL_DATA_TYPE_INT16: return "INT16";
		case K2SQL_DATA_TYPE_INT32: return "INT32";
		case K2SQL_DATA_TYPE_INT64: return "INT64";
		case K2SQL_DATA_TYPE_STRING: return "STRING";
		case K2SQL_DATA_TYPE_BOOL: return "BOOL";
		case K2SQL_DATA_TYPE_FLOAT: return "FLOAT";
		case K2SQL_DATA_TYPE_DOUBLE: return "DOUBLE";
		case K2SQL_DATA_TYPE_BINARY: return "BINARY";
		case K2SQL_DATA_TYPE_TIMESTAMP: return "TIMESTAMP";
		case K2SQL_DATA_TYPE_DECIMAL: return "DECIMAL";
		case K2SQL_DATA_TYPE_LIST: return "LIST";
		case K2SQL_DATA_TYPE_MAP: return "MAP";
		case K2SQL_DATA_TYPE_SET: return "SET";
		case K2SQL_DATA_TYPE_DATE: return "DATE";
		case K2SQL_DATA_TYPE_TIME: return "TIME";
		case K2SQL_DATA_TYPE_UINT8: return "UINT8";
		case K2SQL_DATA_TYPE_UINT16: return "UINT16";
		case K2SQL_DATA_TYPE_UINT32: return "UINT32";
		case K2SQL_DATA_TYPE_UINT64: return "UINT64";
		default: return "unknown";
	}
}

void
K2PgReportIfK2PgEnabled()
{
	if (K2PgIsEnabledInPostgresEnvVar()) {
		ereport(LOG, (errmsg(
			"K2Pg is ENABLED in PostgreSQL. Transactions are %s.",
			K2PgIsEnvVarTrue("K2PG_TRANSACTIONS_ENABLED") ?
			"enabled" : "disabled")));
	} else {
		ereport(LOG, (errmsg("K2Pg is NOT ENABLED -- "
							"this is a vanilla PostgreSQL server!")));
	}
}

bool
K2PgShouldRestartAllChildrenIfOneCrashes() {
	if (!K2PgIsEnabledInPostgresEnvVar()) {
		ereport(LOG, (errmsg("K2PgShouldRestartAllChildrenIfOneCrashes returning 0, K2PgIsEnabledInPostgresEnvVar is false")));
		return true;
	}
	const char* flag =
		getenv("PG_NO_RESTART_ALL_CHILDREN_ON_CRASH");
	// We will use PostgreSQL's default behavior (restarting all children if one of them crashes)
	// if the flag env variable is not specified
	return !flag;
}

bool
K2PgShouldLogStackTraceOnError()
{
	static int cached_value = -1;
	if (cached_value != -1)
	{
		return cached_value;
	}

	cached_value = K2PgIsEnvVarTrue("K2PG_STACK_TRACE_ON_ERROR");
	return cached_value;
}

const char*
K2PgErrorLevelToString(int elevel) {
	switch (elevel)
	{
		case DEBUG5: return "DEBUG5";
		case DEBUG4: return "DEBUG4";
		case DEBUG3: return "DEBUG3";
		case DEBUG2: return "DEBUG2";
		case DEBUG1: return "DEBUG1";
		case LOG: return "LOG";
		case LOG_SERVER_ONLY: return "LOG_SERVER_ONLY";
		case INFO: return "INFO";
		case WARNING: return "WARNING";
		case ERROR: return "ERROR";
		case FATAL: return "FATAL";
		case PANIC: return "PANIC";
		default: return "UNKNOWN";
	}
}

const char*
K2PgGetDatabaseName(Oid relid)
{
	/*
	 * Hardcode the names for system db since the cache might not
	 * be initialized during initdb (bootstrap mode).
	 * For shared rels (e.g. pg_database) we may not have a database id yet,
	 * so assuming template1 in that case since that's where shared tables are
	 * stored in K2.
	 * TODO Eventually K2PG should switch to using oid's everywhere so
	 * that dbname and schemaname should not be needed at all.
	 */
	if (MyDatabaseId == TemplateDbOid || IsSharedRelation(relid))
		return "template1";
	else
		return get_database_name(MyDatabaseId);
}

const char*
K2PgGetSchemaName(Oid schemaoid)
{
	/*
	 * Hardcode the names for system namespaces since the cache might not
	 * be initialized during initdb (bootstrap mode).
	 * TODO Eventually K2PG should switch to using oid's everywhere so
	 * that dbname and schemaname should not be needed at all.
	 */
	if (IsSystemNamespace(schemaoid))
		return "pg_catalog";
	else if (IsToastNamespace(schemaoid))
		return "pg_toast";
	else
		return get_namespace_name(schemaoid);
}

Oid
K2PgGetDatabaseOid(Relation rel)
{
	return rel->rd_rel->relisshared ? TemplateDbOid : MyDatabaseId;
}

void
K2PgRaiseNotSupported(const char *msg, int issue_no)
{
	K2PgRaiseNotSupportedSignal(msg, issue_no, K2PgUnsupportedFeatureSignalLevel());
}

void
K2PgRaiseNotSupportedSignal(const char *msg, int issue_no, int signal_level)
{
	if (issue_no > 0)
	{
		ereport(signal_level,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("%s", msg),
				 errhint("See https://github.com/futurewei-cloud/chogori-sql/issues/%d. "
						 "Click '+' on the description to raise its priority", issue_no)));
	}
	else
	{
		ereport(signal_level,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("%s", msg),
				 errhint("%s", "")));
	}
}

//------------------------------------------------------------------------------
// Debug utils.

bool k2pg_debug_mode = false;

const char*
K2PgDatumToString(Datum datum, Oid typid)
{
	Oid			typoutput = InvalidOid;
	bool		typisvarlena = false;

	getTypeOutputInfo(typid, &typoutput, &typisvarlena);
	return OidOutputFunctionCall(typoutput, datum);
}

const char*
K2PgHeapTupleToString(HeapTuple tuple, TupleDesc tupleDesc)
{
	Datum attr = (Datum) 0;
	int natts = tupleDesc->natts;
	bool isnull = false;
	StringInfoData buf;
	initStringInfo(&buf);

	appendStringInfoChar(&buf, '(');
	for (int attnum = 1; attnum <= natts; ++attnum) {
		attr = heap_getattr(tuple, attnum, tupleDesc, &isnull);
		if (isnull)
		{
			appendStringInfoString(&buf, "null");
		}
		else
		{
			Oid typid = TupleDescAttr(tupleDesc, attnum - 1)->atttypid;
			appendStringInfoString(&buf, K2PgDatumToString(attr, typid));
		}
		if (attnum != natts) {
			appendStringInfoString(&buf, ", ");
		}
	}
	appendStringInfoChar(&buf, ')');
	return buf.data;
}

bool
K2PgIsInitDbAlreadyDone()
{
	bool done = false;
	HandleK2PgStatus(PgGate_IsInitDbDone(&done));
	return done;
}

/*---------------------------------------------------------------------------*/
/* Transactional DDL support                                                 */
/*---------------------------------------------------------------------------*/

static ProcessUtility_hook_type prev_ProcessUtility = NULL;
static int ddl_nesting_level = 0;

int
K2PgGetDdlNestingLevel()
{
	return ddl_nesting_level;
}

void
K2PgIncrementDdlNestingLevel()
{
	if (ddl_nesting_level == 0)
		PgGate_EnterSeparateDdlTxnMode();
	ddl_nesting_level++;
}

void
K2PgDecrementDdlNestingLevel(bool success)
{
	ddl_nesting_level--;
	if (ddl_nesting_level == 0)
		PgGate_ExitSeparateDdlTxnMode(success);
}

static bool IsTransactionalDdlStatement(NodeTag node_tag) {
	switch (node_tag) {
		// The lists of tags here have been generated using e.g.:
		// cat $( find src/postgres -name "nodes.h" ) | grep "T_Create" | sort | uniq |
		//   sed 's/,//g' | while read s; do echo -e "\t\tcase $s:"; done
		// All T_Create... tags from nodes.h:
		case T_CreateAmStmt:
		case T_CreateCastStmt:
		case T_CreateConversionStmt:
		case T_CreateDomainStmt:
		case T_CreateEnumStmt:
		case T_CreateEventTrigStmt:
		case T_CreateExtensionStmt:
		case T_CreateFdwStmt:
		case T_CreateForeignServerStmt:
		case T_CreateForeignTableStmt:
		case T_CreateFunctionStmt:
		case T_CreateOpClassItem:
		case T_CreateOpClassStmt:
		case T_CreateOpFamilyStmt:
		case T_CreatePLangStmt:
		case T_CreatePolicyStmt:
		case T_CreatePublicationStmt:
		case T_CreateRangeStmt:
		case T_CreateReplicationSlotCmd:
		case T_CreateRoleStmt:
		case T_CreateSchemaStmt:
		case T_CreateSeqStmt:
		case T_CreateStatsStmt:
		case T_CreateStmt:
		case T_CreateSubscriptionStmt:
		case T_CreateTableAsStmt:
		case T_CreateTableSpaceStmt:
		case T_CreateTransformStmt:
		case T_CreateTrigStmt:
		case T_CreateUserMappingStmt:
		case T_CreatedbStmt:
		// All T_Drop... tags from nodes.h:
		case T_DropOwnedStmt:
		case T_DropReplicationSlotCmd:
		case T_DropRoleStmt:
		case T_DropStmt:
		case T_DropSubscriptionStmt:
		case T_DropTableSpaceStmt:
		case T_DropUserMappingStmt:
		case T_DropdbStmt:
		// All T_Alter... tags from nodes.h:
		case T_AlterCollationStmt:
		case T_AlterDatabaseSetStmt:
		case T_AlterDatabaseStmt:
		case T_AlterDefaultPrivilegesStmt:
		case T_AlterDomainStmt:
		case T_AlterEnumStmt:
		case T_AlterEventTrigStmt:
		case T_AlterExtensionContentsStmt:
		case T_AlterExtensionStmt:
		case T_AlterFdwStmt:
		case T_AlterForeignServerStmt:
		case T_AlterFunctionStmt:
		case T_AlterObjectDependsStmt:
		case T_AlterObjectSchemaStmt:
		case T_AlterOpFamilyStmt:
		case T_AlterOperatorStmt:
		case T_AlterOwnerStmt:
		case T_AlterPolicyStmt:
		case T_AlterPublicationStmt:
		case T_AlterRoleSetStmt:
		case T_AlterRoleStmt:
		case T_AlterSeqStmt:
		case T_AlterSubscriptionStmt:
		case T_AlterSystemStmt:
		case T_AlterTSConfigurationStmt:
		case T_AlterTSDictionaryStmt:
		case T_AlterTableCmd:
		case T_AlterTableMoveAllStmt:
		case T_AlterTableSpaceOptionsStmt:
		case T_AlterTableStmt:
		case T_AlterUserMappingStmt:
		case T_AlternativeSubPlan:
		case T_AlternativeSubPlanState:
		// T_Grant...
		case T_GrantStmt:
		case T_GrantRoleStmt:
		// T_Index...
		case T_IndexStmt:
			return true;
		default:
			return false;
	}
}

static void K2PgTxnDdlProcessUtility(
		PlannedStmt *pstmt,
		const char *queryString,
		ProcessUtilityContext context,
		ParamListInfo params,
		QueryEnvironment *queryEnv,
		DestReceiver *dest,
		char *completionTag) {
	Node	   *parsetree = pstmt->utilityStmt;
	NodeTag node_tag = nodeTag(parsetree);

	bool is_txn_ddl = IsTransactionalDdlStatement(node_tag);

	if (is_txn_ddl) {
		K2PgIncrementDdlNestingLevel();
	}
	PG_TRY();
	{
		if (prev_ProcessUtility)
			prev_ProcessUtility(pstmt, queryString,
								context, params, queryEnv,
								dest, completionTag);
		else
			standard_ProcessUtility(pstmt, queryString,
									context, params, queryEnv,
									dest, completionTag);
	}
	PG_CATCH();
	{
		if (is_txn_ddl) {
			K2PgDecrementDdlNestingLevel(/* success */ false);
		}
		PG_RE_THROW();
	}
	PG_END_TRY();
	if (is_txn_ddl) {
		K2PgDecrementDdlNestingLevel(/* success */ true);
	}
}


static void K2PgInstallTxnDdlHook() {
	if (!K2PgIsInitDbModeEnvVarSet()) {
		prev_ProcessUtility = ProcessUtility_hook;
		ProcessUtility_hook = K2PgTxnDdlProcessUtility;
	}
}
