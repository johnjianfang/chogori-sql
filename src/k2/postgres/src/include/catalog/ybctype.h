/*--------------------------------------------------------------------------------------------------
 *
 * ybctype.h
 *	  prototypes for ybctype.c.
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
 * src/include/catalog/ybctype.h
 *
 *--------------------------------------------------------------------------------------------------
 */

#ifndef YBCTYPE_H
#define YBCTYPE_H

#include "access/htup.h"
#include "catalog/dependency.h"
#include "nodes/parsenodes.h"

#include "pggate/pg_gate_typedefs.h"

/*
 * Constants for OIDs of supported Postgres native types (that do not have an
 * already declared constant in Postgres).
 */
#define K2PG_CHARARRAYOID 1002 /* char[] */
#define K2PG_TEXTARRAYOID 1009 /* text[] */
#define K2PG_ACLITEMARRAYOID 1034 /* aclitem[] */

extern const K2PgTypeEntity *K2PgDataTypeFromName(TypeName *typeName);
extern const K2PgTypeEntity *K2PgDataTypeFromOidMod(int attnum, Oid type_id);

/*
 * Returns true if we are allow the given type to be used for key columns such as primary key or
 * indexing key.
 */
bool K2PgDataTypeIsValidForKey(Oid type_id);

/*
 * Array of all type entities.
 */
void K2PgGetTypeTable(const K2PgTypeEntity **type_table, int *count);

#endif
