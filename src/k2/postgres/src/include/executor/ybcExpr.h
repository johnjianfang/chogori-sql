/*--------------------------------------------------------------------------------------------------
 * ybcExpr.h
 *	  prototypes for ybcExpr.c
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
 * src/include/executor/ybcExpr.h
 *
 * NOTES:
 *   - For performance reasons, some expressions must be sent to K2 for execution.
 *   - This module constructs expression tree to be sent to K2 PgGate API.
 *--------------------------------------------------------------------------------------------------
 */

#ifndef YBCEXPR_H
#define YBCEXPR_H

#include "pggate/pg_gate_typedefs.h"

#include "pggate/pg_gate_api.h"

// Construct column reference expression.
extern K2PgExpr K2PgNewColumnRef(K2PgStatement k2pg_stmt, int16_t attr_num, int attr_typid,
																 const K2PgTypeAttrs *type_attrs);

// Construct constant expression using the given datatype "type_id" and value "datum".
extern K2PgExpr K2PgNewConstant(K2PgStatement k2pg_stmt, Oid type_id, Datum datum, bool is_null);

// Construct a generic eval_expr call for given a PG Expr and its expected type and attno.
extern K2PgExpr K2PgNewEvalExprCall(K2PgStatement k2pg_stmt, Expr *expr, int32_t attno, int32_t type_id, int32_t type_mod);

#endif							/* YBCEXPR_H */
