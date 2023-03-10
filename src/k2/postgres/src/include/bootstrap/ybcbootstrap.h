/*--------------------------------------------------------------------------------------------------
 *
 * ybcbootstrap.h
 *	  prototypes for ybcbootstrap.c
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
 * src/include/bootstrap/ybcbootstrap.h
 *
 *--------------------------------------------------------------------------------------------------
 */

#ifndef YBCBOOTSTRAP_H
#define YBCBOOTSTRAP_H

#include "access/htup.h"
#include "catalog/dependency.h"
#include "catalog/objectaddress.h"
#include "nodes/parsenodes.h"
#include "storage/lock.h"
#include "utils/relcache.h"

#include "pggate/pg_gate_api.h"

/*  Database Functions -------------------------------------------------------------------------- */

/*  Table Functions ----------------------------------------------------------------------------- */

extern void K2PgCreateSysCatalogTable(const char *table_name,
                                     Oid table_oid,
                                     TupleDesc tupDecs,
                                     bool is_shared_relation,
                                     IndexStmt *pkey_idx);
extern Oid K2PgExecSysCatalogInsert(Relation rel,
                                   TupleDesc tupleDesc,
                                   HeapTuple tuple);

#endif
