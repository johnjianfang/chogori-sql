/*--------------------------------------------------------------------------------------------------
 *
 * ybcModifyTable.h
 *	  prototypes for ybcModifyTable.c
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
 * src/include/executor/ybcModifyTable.h
 *
 *--------------------------------------------------------------------------------------------------
 */

#ifndef YBCMODIFYTABLE_H
#define YBCMODIFYTABLE_H

#include "nodes/execnodes.h"
#include "executor/tuptable.h"

//------------------------------------------------------------------------------
// K2 PG modify table API.

/*
 * Insert data into K2 PG table.
 * This function is equivalent to "heap_insert", but it sends data to K2 platform.
 */
extern Oid K2PgHeapInsert(TupleTableSlot *slot,
												 HeapTuple tuple,
												 EState *estate);

/*
 * Insert a tuple into a K2PG table. Will execute within a distributed
 * transaction if the table is transactional (PSQL default).
 */
extern Oid K2PgExecuteInsert(Relation rel,
                            TupleDesc tupleDesc,
                            HeapTuple tuple);

/*
 * Execute the insert outside of a transaction.
 * Assumes the caller checked that it is safe to do so.
 */
extern Oid K2PgExecuteNonTxnInsert(Relation rel,
								  TupleDesc tupleDesc,
								  HeapTuple tuple);

/*
 * Insert a tuple into the an index's backing K2PG index table.
 */
extern void K2PgExecuteInsertIndex(Relation rel,
								  Datum *values,
								  bool *isnull,
								  Datum k2pgctid,
								  bool is_backfill);

/*
 * Delete a tuple (identified by k2pgctid) from a K2PG table.
 * If this is a single row op we will return false in the case that there was
 * no row to delete. This can occur because we do not first perform a scan if
 * it is a single row op.
 */
extern bool K2PgExecuteDelete(Relation rel,
							 TupleTableSlot *slot,
							 EState *estate,
							 ModifyTableState *mtstate);
/*
 * Delete a tuple (identified by index columns and base table k2pgctid) from an
 * index's backing K2PG index table.
 */
extern void K2PgExecuteDeleteIndex(Relation index,
                                  Datum *values,
                                  bool *isnull,
                                  Datum k2pgctid);

/*
 * Update a row (identified by k2pgctid) in a K2PG table.
 * If this is a single row op we will return false in the case that there was
 * no row to update. This can occur because we do not first perform a scan if
 * it is a single row op.
 */
extern bool K2PgExecuteUpdate(Relation rel,
							 TupleTableSlot *slot,
							 HeapTuple tuple,
							 EState *estate,
							 ModifyTableState *mtstate,
							 Bitmapset *updatedCols);

//------------------------------------------------------------------------------
// System tables modify-table API.
// For system tables we identify rows to update/delete directly by primary key
// and execute them directly (rather than needing to read k2pgctid first).
// TODO This should be used for regular tables whenever possible.

extern void K2PgDeleteSysCatalogTuple(Relation rel, HeapTuple tuple);

extern void K2PgUpdateSysCatalogTuple(Relation rel,
									 HeapTuple oldtuple,
									 HeapTuple tuple);

//------------------------------------------------------------------------------
// Utility methods.

extern bool K2PgIsSingleRowTxnCapableRel(ResultRelInfo *resultRelInfo);

extern Datum K2PgGetPgTupleIdFromSlot(TupleTableSlot *slot);

extern Datum K2PgGetPgTupleIdFromTuple(K2PgStatement pg_stmt,
									  Relation rel,
									  HeapTuple tuple,
									  TupleDesc tupleDesc);

/*
 * Returns if a table has secondary indices.
 */
extern bool K2PgRelInfoHasSecondaryIndices(ResultRelInfo *resultRelInfo);

/*
 * Get primary key columns as bitmap of a table for real and system K2PG columns.
 */
extern Bitmapset *GetFullK2PgTablePrimaryKey(Relation rel);

/*
 * Get primary key columns as bitmap of a table for real columns.
 */
extern Bitmapset *GetK2PgTablePrimaryKey(Relation rel);

#endif							/* YBCMODIFYTABLE_H */
