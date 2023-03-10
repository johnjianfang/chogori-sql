/*--------------------------------------------------------------------------------------------------
 *
 * ybccmds.c
 *        K2PG commands for creating and altering table structures and settings
 *
 * Copyright (c) YugaByte, Inc.
 * Portions Copyright (c) 2021 Futurewei Cloud
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied.  See the License for the specific language governing permissions and limitations
 * under the License.
 *
 * IDENTIFICATION
 *        src/backend/commands/ybccmds.c
 *
 *------------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"
#include "access/sysattr.h"
#include "catalog/catalog.h"
#include "catalog/index.h"
#include "catalog/pg_am.h"
#include "catalog/pg_attribute.h"
#include "catalog/pg_class.h"
#include "catalog/pg_database.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_type.h"
#include "catalog/ybctype.h"
#include "commands/dbcommands.h"
#include "commands/ybccmds.h"

#include "access/htup_details.h"
#include "utils/lsyscache.h"
#include "utils/relcache.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "executor/tuptable.h"
#include "executor/ybcExpr.h"

#include "pggate/pg_gate_api.h"
#include "pg_k2pg_utils.h"

#include "access/nbtree.h"
#include "commands/defrem.h"
#include "nodes/nodeFuncs.h"
#include "parser/parser.h"
#include "parser/parse_coerce.h"
#include "parser/parse_type.h"

/* Utility function to calculate column sorting options */
static void
ColumnSortingOptions(SortByDir dir, SortByNulls nulls, bool* is_desc, bool* is_nulls_first)
{
  if (dir == SORTBY_DESC) {
	/*
	 * From postgres doc NULLS FIRST is the default for DESC order.
	 * So SORTBY_NULLS_DEFAULT is equal to SORTBY_NULLS_FIRST here.
	 */
	*is_desc = true;
	*is_nulls_first = (nulls != SORTBY_NULLS_LAST);
  } else {
	/*
	 * From postgres doc ASC is the default sort order and NULLS LAST is the default for it.
	 * So SORTBY_DEFAULT is equal to SORTBY_ASC and SORTBY_NULLS_DEFAULT is equal
	 * to SORTBY_NULLS_LAST here.
	 */
	*is_desc = false;
	*is_nulls_first = (nulls == SORTBY_NULLS_FIRST);
  }
}

/* -------------------------------------------------------------------------- */
/*  Cluster Functions. */
void
K2InitPGCluster()
{
	HandleK2PgStatus(PgGate_InitPrimaryCluster());
}

void
K2FinishInitDB()
{
	HandleK2PgStatus(PgGate_FinishInitDB());
}

/* -------------------------------------------------------------------------- */
/*  Database Functions. */

void
K2PgCreateDatabase(Oid dboid, const char *dbname, Oid src_dboid, Oid next_oid, bool colocated)
{
	K2PgStatement handle;

	HandleK2PgStatus(PgGate_NewCreateDatabase(dbname,
										  dboid,
										  src_dboid,
										  next_oid,
										  colocated,
										  &handle));
	HandleK2PgStatus(PgGate_ExecCreateDatabase(handle));
}

void
K2PgDropDatabase(Oid dboid, const char *dbname)
{
	K2PgStatement handle;

	HandleK2PgStatus(PgGate_NewDropDatabase(dbname,
										dboid,
										&handle));
	HandleK2PgStatus(PgGate_ExecDropDatabase(handle));
}

void
K2PgReservePgOids(Oid dboid, Oid next_oid, uint32 count, Oid *begin_oid, Oid *end_oid)
{
	HandleK2PgStatus(PgGate_ReserveOids(dboid,
									next_oid,
									count,
									begin_oid,
									end_oid));
}

/* ------------------------------------------------------------------------- */
/*  Table Functions. */

static void CreateTableAddColumn(K2PgStatement handle,
								 Form_pg_attribute att,
								 bool is_hash,
								 bool is_primary,
								 bool is_desc,
								 bool is_nulls_first)
{
	const AttrNumber attnum = att->attnum;
	const K2PgTypeEntity *col_type = K2PgDataTypeFromOidMod(attnum,
															att->atttypid);
	HandleK2PgStatus(PgGate_CreateTableAddColumn(handle,
																					 NameStr(att->attname),
																					 attnum,
																					 col_type,
																					 is_hash,
																					 is_primary,
																					 is_desc,
																					 is_nulls_first));
}

/* Utility function to add columns to the K2PG create statement
 * Columns need to be sent in order first hash columns, then rest of primary
 * key columns, then regular columns.
 */
static void CreateTableAddColumns(K2PgStatement handle,
								  TupleDesc desc,
								  Constraint *primary_key,
								  const bool colocated)
{
	/* Add all key columns first with respect to compound key order */
	ListCell *cell;
	if (primary_key != NULL)
	{
		foreach(cell, primary_key->k2pg_index_params)
		{
			IndexElem *index_elem = (IndexElem *)lfirst(cell);
			bool column_found = false;
			for (int i = 0; i < desc->natts; ++i)
			{
				Form_pg_attribute att = TupleDescAttr(desc, i);
				if (strcmp(NameStr(att->attname), index_elem->name) == 0)
				{
					if (!K2PgDataTypeIsValidForKey(att->atttypid))
						ereport(ERROR,
								(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
								 errmsg("PRIMARY KEY containing column of type"
										" '%s' not yet supported",
										K2PgTypeOidToStr(att->atttypid))));
					SortByDir order = index_elem->ordering;
					/* In K2PG mode, the first column defaults to HASH if it is
					 * not set and its table is not colocated */
					const bool is_first_key =
						cell == list_head(primary_key->k2pg_index_params);
					bool is_hash = (order == SORTBY_HASH ||
									(is_first_key &&
									 order == SORTBY_DEFAULT &&
									 !colocated));
					bool is_desc = false;
					bool is_nulls_first = false;
					ColumnSortingOptions(order,
										 index_elem->nulls_ordering,
										 &is_desc,
										 &is_nulls_first);
					CreateTableAddColumn(handle,
										 att,
										 is_hash,
										 true /* is_primary */,
										 is_desc,
										 is_nulls_first);
					column_found = true;
					break;
				}
			}
			if (!column_found)
				ereport(FATAL,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("Column '%s' not found in table",
								index_elem->name)));
		}
	}

	/* Add all non-key columns */
	for (int i = 0; i < desc->natts; ++i)
	{
		Form_pg_attribute att = TupleDescAttr(desc, i);
		bool is_key = false;
		if (primary_key)
			foreach(cell, primary_key->k2pg_index_params)
			{
				IndexElem *index_elem = (IndexElem *) lfirst(cell);
				if (strcmp(NameStr(att->attname), index_elem->name) == 0)
				{
					is_key = true;
					break;
				}
			}
		if (!is_key)
			CreateTableAddColumn(handle,
								 att,
								 false /* is_hash */,
								 false /* is_primary */,
								 false /* is_desc */,
								 false /* is_nulls_first */);
	}
}

static Datum*
CreateSplitPointDatums(ParseState *pstate,
                       List *split_point,
                       Oid *col_attrtypes,
                       int32 *col_attrtypmods)
{
	Datum *datums = palloc(sizeof(Datum) * list_length(split_point));
	/* Within a split point, go through the splits for each column */
	int split_num = 0;
	ListCell *cell;
	foreach(cell, split_point)
	{
		/* Get the column's split */
		PartitionRangeDatum *split = (PartitionRangeDatum*) lfirst(cell);

		/* If it contains a value, convert that value */
		if (split->kind == PARTITION_RANGE_DATUM_VALUE)
		{
			A_Const *aconst = (A_Const*) split->value;
			Node *value = (Node *) make_const(pstate, &aconst->val, aconst->location);
			value = coerce_to_target_type(pstate,
			                              value,
			                              exprType(value),
			                              col_attrtypes[split_num],
			                              col_attrtypmods[split_num],
			                              COERCION_ASSIGNMENT,
			                              COERCE_IMPLICIT_CAST,
			                              -1);
			if (value == NULL || ((Const*)value)->consttype == 0)
				ereport(ERROR, (errmsg("Type mismatch in split point")));

			split->value = value;
			datums[split_num] = ((Const*)value)->constvalue;
		}
		else
		{
			/*
			 * TODO (george): maybe we'll allow MINVALUE/MAXVALUE in the future,
			 * but for now it is illegal
			 */
			ereport(ERROR, (errmsg("Split points must specify finite values")));
		}
		++split_num;
	}
	return datums;
}

void
K2PgCreateTable(CreateStmt *stmt, char relkind, TupleDesc desc, Oid relationId, Oid pgNamespaceId)
{
	if (relkind != RELKIND_RELATION)
	{
		return;
	}

	if (stmt->relation->relpersistence == RELPERSISTENCE_TEMP)
	{
		return; /* Nothing to do. */
	}

	K2PgStatement handle = NULL;
	ListCell       *listptr;

	char *db_name = get_database_name(MyDatabaseId);
	char *schema_name = stmt->relation->schemaname;
	if (schema_name == NULL)
	{
		schema_name = get_namespace_name(pgNamespaceId);
	}
	if (!IsBootstrapProcessingMode())
		K2PG_LOG_INFO("Creating Table %s.%s.%s",
					 db_name,
					 schema_name,
					 stmt->relation->relname);

	Constraint *primary_key = NULL;

	foreach(listptr, stmt->constraints)
	{
		Constraint *constraint = lfirst(listptr);

		if (constraint->contype == CONSTR_PRIMARY)
		{
			primary_key = constraint;
		}
	}

	/* By default, inherit the colocated option from the database */
	bool colocated = MyDatabaseColocated;

	/* Handle user-supplied colocated reloption */
	ListCell *opt_cell;
	foreach(opt_cell, stmt->options)
	{
		DefElem *def = (DefElem *) lfirst(opt_cell);

		if (strcmp(def->defname, "colocated") == 0)
		{
			bool colocated_relopt = defGetBoolean(def);
			if (MyDatabaseColocated)
				colocated = colocated_relopt;
			else if (colocated_relopt)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot set colocated true on a non-colocated"
								" database")));
			/* The following break is fine because there should only be one
			 * colocated reloption at this point due to checks in
			 * parseRelOptions */
			break;
		}
	}

	HandleK2PgStatus(PgGate_NewCreateTable(db_name,
									   schema_name,
									   stmt->relation->relname,
									   MyDatabaseId,
									   relationId,
									   false, /* is_shared_table */
									   false, /* if_not_exists */
									   primary_key == NULL /* add_primary_key */,
									   colocated,
									   &handle));

	CreateTableAddColumns(handle, desc, primary_key, colocated);

	/* Create the table. */
	HandleK2PgStatus(PgGate_ExecCreateTable(handle));
}

void
K2PgDropTable(Oid relationId)
{
	K2PgStatement	handle = NULL;
	bool			colocated = false;

	/* Determine if table is colocated */
	if (MyDatabaseColocated)
	{
		bool not_found = false;
		HandleK2PgStatusIgnoreNotFound(PgGate_IsTableColocated(MyDatabaseId,
																											 relationId,
																											 &colocated),
																 &not_found);
	}

	/* Create table-level tombstone for colocated tables */
	if (colocated)
	{
		bool not_found = false;
		HandleK2PgStatusIgnoreNotFound(PgGate_NewTruncateColocated(MyDatabaseId,
																													 relationId,
																													 false,
																													 &handle),
																 &not_found);
		/* Since the creation of the handle could return a 'NotFound' error,
		 * execute the statement only if the handle is valid.
		 */
		const bool valid_handle = !not_found;
		if (valid_handle)
		{
			HandleK2PgStatusIgnoreNotFound(PgGate_DmlBindTable(handle), &not_found);
			int rows_affected_count = 0;
			HandleK2PgStatusIgnoreNotFound(PgGate_DmlExecWriteOp(handle, &rows_affected_count), &not_found);
		}
	}

	/* Drop the table */
	{
		bool not_found = false;
		HandleK2PgStatusIgnoreNotFound(PgGate_NewDropTable(MyDatabaseId,
																									 relationId,
																									 false, /* if_exists */
																									 &handle),
																 &not_found);
		const bool valid_handle = !not_found;
		if (valid_handle)
		{
			HandleK2PgStatusIgnoreNotFound(PgGate_ExecDropTable(handle), &not_found);
		}
	}
}

void
K2PgTruncateTable(Relation rel) {
	K2PgStatement	handle;
	Oid				relationId = RelationGetRelid(rel);
	bool			colocated = false;

	/* Determine if table is colocated */
	if (MyDatabaseColocated)
		HandleK2PgStatus(PgGate_IsTableColocated(MyDatabaseId,
											 relationId,
											 &colocated));

	if (colocated)
	{
		/* Create table-level tombstone for colocated tables */
		HandleK2PgStatus(PgGate_NewTruncateColocated(MyDatabaseId,
												 relationId,
												 false,
												 &handle));
		HandleK2PgStatus(PgGate_DmlBindTable(handle));
		int rows_affected_count = 0;
		HandleK2PgStatus(PgGate_DmlExecWriteOp(handle, &rows_affected_count));
	}
	else
	{
		/* Send truncate table RPC to master for non-colocated tables */
		HandleK2PgStatus(PgGate_NewTruncateTable(MyDatabaseId,
											 relationId,
											 &handle));
		HandleK2PgStatus(PgGate_ExecTruncateTable(handle));
	}

	if (!rel->rd_rel->relhasindex)
		return;

	/* Truncate the associated secondary indexes */
	List	 *indexlist = RelationGetIndexList(rel);
	ListCell *lc;

	foreach(lc, indexlist)
	{
		Oid indexId = lfirst_oid(lc);

		if (indexId == rel->rd_pkindex)
			continue;

		/* Determine if table is colocated */
		if (MyDatabaseColocated)
			HandleK2PgStatus(PgGate_IsTableColocated(MyDatabaseId,
												 relationId,
												 &colocated));
		if (colocated)
		{
			/* Create table-level tombstone for colocated tables */
			HandleK2PgStatus(PgGate_NewTruncateColocated(MyDatabaseId,
													 relationId,
													 false,
													 &handle));
			HandleK2PgStatus(PgGate_DmlBindTable(handle));
			int rows_affected_count = 0;
			HandleK2PgStatus(PgGate_DmlExecWriteOp(handle, &rows_affected_count));
		}
		else
		{
			/* Send truncate table RPC to master for non-colocated tables */
			HandleK2PgStatus(PgGate_NewTruncateTable(MyDatabaseId,
												 indexId,
												 &handle));
			HandleK2PgStatus(PgGate_ExecTruncateTable(handle));
		}
	}

	list_free(indexlist);
}

void
K2PgCreateIndex(const char *indexName,
			   IndexInfo *indexInfo,
			   TupleDesc indexTupleDesc,
			   int16 *coloptions,
			   Datum reloptions,
			   Oid indexId,
			   Relation rel,
			   OptSplit *split_options,
			   const bool skip_index_backfill)
{
	char *db_name	  = get_database_name(MyDatabaseId);
	char *schema_name = get_namespace_name(RelationGetNamespace(rel));

	if (!IsBootstrapProcessingMode())
		K2PG_LOG_INFO("Creating index %s.%s.%s",
					 db_name,
					 schema_name,
					 indexName);

	/* Check reloptions. */
	ListCell	*opt_cell;
	foreach(opt_cell, untransformRelOptions(reloptions))
	{
		DefElem *def = (DefElem *) lfirst(opt_cell);

		if (strcmp(def->defname, "colocated") == 0)
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot set option \"%s\" on index",
							def->defname)));
		}
	}

	K2PgStatement handle = NULL;

	HandleK2PgStatus(PgGate_NewCreateIndex(db_name,
									   schema_name,
									   indexName,
									   MyDatabaseId,
									   indexId,
									   RelationGetRelid(rel),
									   rel->rd_rel->relisshared,
									   indexInfo->ii_Unique,
									   skip_index_backfill,
									   false, /* if_not_exists */
									   &handle));

	for (int i = 0; i < indexTupleDesc->natts; i++)
	{
		Form_pg_attribute     att         = TupleDescAttr(indexTupleDesc, i);
		char                  *attname    = NameStr(att->attname);
		AttrNumber            attnum      = att->attnum;
		const K2PgTypeEntity *col_type   = K2PgDataTypeFromOidMod(attnum, att->atttypid);
		const bool            is_key      = (i < indexInfo->ii_NumIndexKeyAttrs);

		if (is_key)
		{
			if (!K2PgDataTypeIsValidForKey(att->atttypid))
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("INDEX on column of type '%s' not yet supported",
								K2PgTypeOidToStr(att->atttypid))));
		}

		const int16 options        = coloptions[i];
		const bool  is_hash        = options & INDOPTION_HASH;
		const bool  is_desc        = options & INDOPTION_DESC;
		const bool  is_nulls_first = options & INDOPTION_NULLS_FIRST;

		HandleK2PgStatus(PgGate_CreateIndexAddColumn(handle,
																						 attname,
																						 attnum,
																						 col_type,
																						 is_hash,
																						 is_key,
																						 is_desc,
																						 is_nulls_first));
	}

	/* Create the index. */
	HandleK2PgStatus(PgGate_ExecCreateIndex(handle));
}

K2PgStatement
K2PgPrepareAlterTable(AlterTableStmt *stmt, Relation rel, Oid relationId)
{
	K2PgStatement handle = NULL;
	HandleK2PgStatus(PgGate_NewAlterTable(MyDatabaseId,
									  relationId,
									  &handle));

	ListCell *lcmd;
	int col = 1;
	bool needsK2PgAlter = false;

	foreach(lcmd, stmt->cmds)
	{
		AlterTableCmd *cmd = (AlterTableCmd *) lfirst(lcmd);
		switch (cmd->subtype)
		{
			case AT_AddColumn:
			{
				ColumnDef* colDef = (ColumnDef *) cmd->def;
				Oid			typeOid;
				int32		typmod;
				HeapTuple	typeTuple;
				int order;

				/* Skip yb alter for IF NOT EXISTS with existing column */
				if (cmd->missing_ok)
				{
					HeapTuple tuple = SearchSysCacheAttName(RelationGetRelid(rel), colDef->colname);
					if (HeapTupleIsValid(tuple)) {
						ReleaseSysCache(tuple);
						break;
					}
				}

				typeTuple = typenameType(NULL, colDef->typeName, &typmod);
				typeOid = HeapTupleGetOid(typeTuple);
				order = RelationGetNumberOfAttributes(rel) + col;
				const K2PgTypeEntity *col_type = K2PgDataTypeFromOidMod(order, typeOid);

				HandleK2PgStatus(PgGate_AlterTableAddColumn(handle, colDef->colname,
																										order, col_type,
																										colDef->is_not_null));
				++col;
				ReleaseSysCache(typeTuple);
				needsK2PgAlter = true;

				break;
			}
			case AT_DropColumn:
			{
				/* Skip yb alter for IF EXISTS with non-existent column */
				if (cmd->missing_ok)
				{
					HeapTuple tuple = SearchSysCacheAttName(RelationGetRelid(rel), cmd->name);
					if (!HeapTupleIsValid(tuple))
						break;
					ReleaseSysCache(tuple);
				}

				HandleK2PgStatus(PgGate_AlterTableDropColumn(handle, cmd->name));
				needsK2PgAlter = true;

				break;
			}

			case AT_AddIndex:
			case AT_AddIndexConstraint:
			{
				IndexStmt *index = (IndexStmt *) cmd->def;
				/* Only allow adding indexes when it is a unique non-primary-key constraint */
				if (!index->unique || index->primary || !index->isconstraint)
				{
					ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							errmsg("This ALTER TABLE command is not yet supported.")));
				}

				break;
			}

			case AT_AddConstraint:
			case AT_DropConstraint:
			case AT_DropOids:
			case AT_EnableTrig:
			case AT_EnableAlwaysTrig:
			case AT_EnableReplicaTrig:
			case AT_EnableTrigAll:
			case AT_EnableTrigUser:
			case AT_DisableTrig:
			case AT_DisableTrigAll:
			case AT_DisableTrigUser:
			case AT_ChangeOwner:
			case AT_ColumnDefault:
			case AT_DropNotNull:
			case AT_SetNotNull:
			case AT_AddIdentity:
			case AT_SetIdentity:
			case AT_DropIdentity:
			case AT_EnableRowSecurity:
			case AT_DisableRowSecurity:
			case AT_ForceRowSecurity:
			case AT_NoForceRowSecurity:
				/* For these cases a K2PG alter isn't required, so we do nothing. */
				break;

			default:
				ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("This ALTER TABLE command is not yet supported.")));
				break;
		}
	}

	if (!needsK2PgAlter)
	{
		return NULL;
	}

	return handle;
}

void
K2PgExecAlterPgTable(K2PgStatement handle, Oid relationId)
{
	if (handle)
	{
		if (IsK2PgRelationById(relationId)) {
			HandleK2PgStatus(PgGate_ExecAlterTable(handle));
		}
	}
}

void
K2PgRename(RenameStmt *stmt, Oid relationId)
{
	K2PgStatement handle = NULL;
	char *db_name	  = get_database_name(MyDatabaseId);

	switch (stmt->renameType)
	{
		case OBJECT_TABLE:
			HandleK2PgStatus(PgGate_NewAlterTable(MyDatabaseId,
											  relationId,
											  &handle));
			HandleK2PgStatus(PgGate_AlterTableRenameTable(handle, db_name, stmt->newname));
			break;

		case OBJECT_COLUMN:
		case OBJECT_ATTRIBUTE:

			HandleK2PgStatus(PgGate_NewAlterTable(MyDatabaseId,
											  relationId,
											  &handle));

			HandleK2PgStatus(PgGate_AlterTableRenameColumn(handle, stmt->subname, stmt->newname));
			break;

		default:
			ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					errmsg("Renaming this object is not yet supported.")));

	}

	K2PgExecAlterPgTable(handle, relationId);
}

void
K2PgDropIndex(Oid relationId)
{
	K2PgStatement	handle;
	bool			colocated = false;

	/* Determine if table is colocated */
	if (MyDatabaseColocated)
	{
		bool not_found = false;
		HandleK2PgStatusIgnoreNotFound(PgGate_IsTableColocated(MyDatabaseId,
																											 relationId,
																											 &colocated),
																 &not_found);
	}

	/* Create table-level tombstone for colocated tables */
	if (colocated)
	{
		bool not_found = false;
		HandleK2PgStatusIgnoreNotFound(PgGate_NewTruncateColocated(MyDatabaseId,
																													 relationId,
																													 false,
																													 &handle),
																 &not_found);
		const bool valid_handle = !not_found;
		if (valid_handle) {
			HandleK2PgStatusIgnoreNotFound(PgGate_DmlBindTable(handle), &not_found);
			int rows_affected_count = 0;
			HandleK2PgStatusIgnoreNotFound(PgGate_DmlExecWriteOp(handle, &rows_affected_count), &not_found);
		}
	}

	/* Drop the index table */
	{
		bool not_found = false;
		HandleK2PgStatusIgnoreNotFound(PgGate_NewDropIndex(MyDatabaseId,
																									 relationId,
																									 false, /* if_exists */
																									 &handle),
																 &not_found);
		const bool valid_handle = !not_found;
		if (valid_handle) {
			HandleK2PgStatusIgnoreNotFound(PgGate_ExecDropIndex(handle), &not_found);
		}
	}
}
