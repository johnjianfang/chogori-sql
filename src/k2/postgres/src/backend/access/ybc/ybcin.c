/*--------------------------------------------------------------------------------------------------
 *
 * ybcin.c
 *	  Implementation of YugaByte indexes.
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
 * src/backend/access/ybc/ybcin.c
 *
 * TODO: currently this file contains skeleton index access methods. They will be implemented in
 * coming revisions.
 *--------------------------------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"
#include "access/nbtree.h"
#include "access/relscan.h"
#include "access/sysattr.h"
#include "access/ybcam.h"
#include "access/ybcin.h"
#include "catalog/index.h"
#include "catalog/pg_type.h"
#include "commands/ybccmds.h"
#include "utils/rel.h"
#include "executor/ybcModifyTable.h"

/* --------------------------------------------------------------------------------------------- */

/* Working state for ybcinbuild and its callback */
typedef struct
{
	bool	isprimary;		/* are we building a primary index? */
	double	index_tuples;	/* # of tuples inserted into index */
	bool	is_backfill;	/* are we concurrently backfilling an index? */
} K2PgBuildState;

/*
 * LSM handler function: return IndexAmRoutine with access method parameters
 * and callbacks.
 */
Datum
ybcinhandler(PG_FUNCTION_ARGS)
{
	IndexAmRoutine *amroutine = makeNode(IndexAmRoutine);

	amroutine->amstrategies = BTMaxStrategyNumber;
	amroutine->amsupport = BTNProcs;
	amroutine->amcanorder = true;
	amroutine->amcanorderbyop = false;
	amroutine->amcanbackward = true;
	amroutine->amcanunique = true;
	amroutine->amcanmulticol = true;
	amroutine->amoptionalkey = true;
	amroutine->amsearcharray = true;
	amroutine->amsearchnulls = true;
	amroutine->amstorage = false;
	amroutine->amclusterable = true;
	amroutine->ampredlocks = true;
	amroutine->amcanparallel = false; /* TODO: support parallel scan */
	amroutine->amcaninclude = true;
	amroutine->amkeytype = InvalidOid;

	amroutine->ambuild = ybcinbuild;
	amroutine->ambuildempty = ybcinbuildempty;
	amroutine->aminsert = NULL; /* use k2pg_aminsert below instead */
	amroutine->ambulkdelete = ybcinbulkdelete;
	amroutine->amvacuumcleanup = ybcinvacuumcleanup;
	amroutine->amcanreturn = ybcincanreturn;
	amroutine->amcostestimate = ybcincostestimate;
	amroutine->amoptions = ybcinoptions;
	amroutine->amproperty = ybcinproperty;
	amroutine->amvalidate = ybcinvalidate;
	amroutine->ambeginscan = ybcinbeginscan;
	amroutine->amrescan = ybcinrescan;
	amroutine->amgettuple = ybcingettuple;
	amroutine->amgetbitmap = NULL; /* TODO: support bitmap scan */
	amroutine->amendscan = ybcinendscan;
	amroutine->ammarkpos = NULL; /* TODO: support mark/restore pos with ordering */
	amroutine->amrestrpos = NULL;
	amroutine->amestimateparallelscan = NULL; /* TODO: support parallel scan */
	amroutine->aminitparallelscan = NULL;
	amroutine->amparallelrescan = NULL;
	amroutine->k2pg_aminsert = ybcininsert;
	amroutine->k2pg_amdelete = ybcindelete;
	amroutine->k2pg_ambackfill = ybcinbackfill;

	PG_RETURN_POINTER(amroutine);
}

static void
ybcinbuildCallback(Relation index, HeapTuple heapTuple, Datum *values, bool *isnull,
				   bool tupleIsAlive, void *state)
{
	K2PgBuildState  *buildstate = (K2PgBuildState *)state;

	if (!buildstate->isprimary)
		K2PgExecuteInsertIndex(index,
							  values,
							  isnull,
							  heapTuple->t_k2pgctid,
							  buildstate->is_backfill);

	buildstate->index_tuples += 1;
}

IndexBuildResult *
ybcinbuild(Relation heap, Relation index, struct IndexInfo *indexInfo)
{
	K2PgBuildState	buildstate;
	double			heap_tuples = 0;

	/* Do the heap scan */
	buildstate.isprimary = index->rd_index->indisprimary;
	buildstate.index_tuples = 0;
	buildstate.is_backfill = false;
	heap_tuples = IndexBuildHeapScan(heap, index, indexInfo, true, ybcinbuildCallback,
									 &buildstate, NULL);

	/*
	 * Return statistics
	 */
	IndexBuildResult *result = (IndexBuildResult *) palloc(sizeof(IndexBuildResult));
	result->heap_tuples  = heap_tuples;
	result->index_tuples = buildstate.index_tuples;
	return result;
}

IndexBuildResult *
ybcinbackfill(Relation heap,
			  Relation index,
			  struct IndexInfo *indexInfo,
			  uint64_t *read_time,
			  RowBounds *row_bounds)
{
	K2PgBuildState	buildstate;
	double			heap_tuples = 0;

	/* Do the heap scan */
	buildstate.isprimary = index->rd_index->indisprimary;
	buildstate.index_tuples = 0;
	buildstate.is_backfill = true;
	heap_tuples = IndexBackfillHeapRangeScan(heap,
											 index,
											 indexInfo,
											 ybcinbuildCallback,
											 &buildstate,
											 read_time,
											 row_bounds);

	/*
	 * Return statistics
	 */
	IndexBuildResult *result = (IndexBuildResult *) palloc(sizeof(IndexBuildResult));
	result->heap_tuples  = heap_tuples;
	result->index_tuples = buildstate.index_tuples;
	return result;
}

void
ybcinbuildempty(Relation index)
{
	K2PG_LOG_WARNING("Unexpected building of empty unlogged index");
}

bool
ybcininsert(Relation index, Datum *values, bool *isnull, Datum k2pgctid, Relation heap,
			IndexUniqueCheck checkUnique, struct IndexInfo *indexInfo)
{
	if (!index->rd_index->indisprimary)
		K2PgExecuteInsertIndex(index,
							  values,
							  isnull,
							  k2pgctid,
							  false /* is_backfill */);

	return index->rd_index->indisunique ? true : false;
}

void
ybcindelete(Relation index, Datum *values, bool *isnull, Datum k2pgctid, Relation heap,
			struct IndexInfo *indexInfo)
{
	if (!index->rd_index->indisprimary)
		K2PgExecuteDeleteIndex(index, values, isnull, k2pgctid);
}

IndexBulkDeleteResult *
ybcinbulkdelete(IndexVacuumInfo *info, IndexBulkDeleteResult *stats,
				IndexBulkDeleteCallback callback, void *callback_state)
{
	K2PG_LOG_WARNING("Unexpected bulk delete of index via vacuum");
	return NULL;
}

IndexBulkDeleteResult *
ybcinvacuumcleanup(IndexVacuumInfo *info, IndexBulkDeleteResult *stats)
{
	K2PG_LOG_WARNING("Unexpected index cleanup via vacuum");
	return NULL;
}

/* --------------------------------------------------------------------------------------------- */

bool ybcincanreturn(Relation index, int attno)
{
	/*
	 * If "canreturn" is true, Postgres will attempt to perform index-only scan on the indexed
	 * columns and expect us to return the column values as an IndexTuple. This will be the case
	 * for secondary index.
	 *
	 * For indexes which are primary keys, we will return the table row as a HeapTuple instead.
	 * For this reason, we set "canreturn" to false for primary keys.
	 */
	return !index->rd_index->indisprimary;
}

void
ybcincostestimate(struct PlannerInfo *root, struct IndexPath *path, double loop_count,
				  Cost *indexStartupCost, Cost *indexTotalCost, Selectivity *indexSelectivity,
				  double *indexCorrelation, double *indexPages)
{
	camIndexCostEstimate(path, indexSelectivity, indexStartupCost, indexTotalCost);
}

bytea *
ybcinoptions(Datum reloptions, bool validate)
{
	return NULL;
}

bool
ybcinproperty(Oid index_oid, int attno, IndexAMProperty prop, const char *propname,
			  bool *res, bool *isnull)
{
	return false;
}

bool
ybcinvalidate(Oid opclassoid)
{
	return true;
}

/* --------------------------------------------------------------------------------------------- */

IndexScanDesc
ybcinbeginscan(Relation rel, int nkeys, int norderbys)
{
	IndexScanDesc scan;

	/* no order by operators allowed */
	Assert(norderbys == 0);

	/* get the scan */
	scan = RelationGetIndexScan(rel, nkeys, norderbys);
	scan->opaque = NULL;

	return scan;
}

void
ybcinrescan(IndexScanDesc scan, ScanKey scankey, int nscankeys,	ScanKey orderbys, int norderbys)
{
	if (scan->opaque)
	{
		/* For rescan, end the previous scan. */
		ybcinendscan(scan);
		scan->opaque = NULL;
	}

	CamScanDesc camScan = camBeginScan(scan->heapRelation, scan->indexRelation, scan->xs_want_itup,
																	 nscankeys, scankey);
	camScan->index = scan->indexRelation;
	scan->opaque = camScan;
}

/*
 * Processing the following SELECT.
 *   SELECT data FROM heapRelation WHERE rowid IN
 *     ( SELECT rowid FROM indexRelation WHERE key = given_value )
 *
 * TODO(neil) Postgres layer should make just one request for IndexScan.
 *   - Query ROWID from IndexTable using key.
 *   - Query data from Table (relation) using ROWID.
 */
bool
ybcingettuple(IndexScanDesc scan, ScanDirection dir)
{
	Assert(dir == ForwardScanDirection || dir == BackwardScanDirection);
	const bool is_forward_scan = (dir == ForwardScanDirection);

	CamScanDesc ybscan = (CamScanDesc) scan->opaque;
	ybscan->exec_params = scan->k2pg_exec_params;
	if (!is_forward_scan && ybscan->exec_params != NULL && !ybscan->exec_params->limit_use_default) {
		// Ignore limit count for reverse scan since K2 PG cannot push down the limit for reverse scan and
		// rely on PG to process the limit count
		// this only applies if limit_use_default is not true
		ybscan->exec_params->limit_count = -1;
	}
	Assert(PointerIsValid(ybscan));

	/*
	 * IndexScan(SysTable, Index) --> HeapTuple.
	 */
	scan->xs_ctup.t_k2pgctid = 0;
	if (ybscan->prepare_params.index_only_scan)
	{
		IndexTuple tuple = cam_getnext_indextuple(ybscan, is_forward_scan, &scan->xs_recheck);
		if (tuple)
		{
			scan->xs_ctup.t_k2pgctid = tuple->t_k2pgctid;
			scan->xs_itup = tuple;
			scan->xs_itupdesc = RelationGetDescr(scan->indexRelation);
		}
	}
	else
	{
		HeapTuple tuple = cam_getnext_heaptuple(ybscan, is_forward_scan, &scan->xs_recheck);
		if (tuple)
		{
			scan->xs_ctup.t_k2pgctid = tuple->t_k2pgctid;
			scan->xs_hitup = tuple;
			scan->xs_hitupdesc = RelationGetDescr(scan->heapRelation);
		}
	}

	return scan->xs_ctup.t_k2pgctid != 0;
}

void
ybcinendscan(IndexScanDesc scan)
{
	CamScanDesc ybscan = (CamScanDesc)scan->opaque;
	Assert(PointerIsValid(ybscan));
	camEndScan(ybscan);
}
