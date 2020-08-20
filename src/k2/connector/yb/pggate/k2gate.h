// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.
//
// The following only applies to changes made to this file as part of YugaByte development.
//
// Portions Copyright (c) YugaByte, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
// in compliance with the License.  You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied.  See the License for the specific language governing permissions and limitations
// under the License.
//
// Copyright(c) 2020 Futurewei Cloud
//
// Permission is hereby granted,
//        free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all copies
// or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS",
// WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//        AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
//        DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
#ifndef CHOGORI_GATE_API_H
#define CHOGORI_GATE_API_H

#include <algorithm>
#include <functional>
#include <thread>
#include <unordered_map>

#include "yb/common/metrics/metrics.h"
#include "yb/common/sys/mem_tracker.h"
#include "yb/entities/expr.h"
#include "yb/pggate/ybc_pg_typedefs.h"
#include "yb/pggate/pg_env.h"
#include "yb/pggate/memctx.h"
#include "yb/pggate/k2session.h"
#include "yb/pggate/k2statement.h"
#include "yb/pggate/k2client.h"

namespace k2 {
namespace gate {

using namespace yb;
using namespace k2::sql;

//--------------------------------------------------------------------------------------------------
// Implements support for CAPI.
class K2GateApiImpl {
  public:
  K2GateApiImpl(const YBCPgTypeEntity *YBCDataTypeTable, int count, YBCPgCallbacks pg_callbacks);
  virtual ~K2GateApiImpl();

 // Initialize ENV within which PGSQL calls will be executed.
  CHECKED_STATUS CreateEnv(PgEnv **pg_env);
  CHECKED_STATUS DestroyEnv(PgEnv *pg_env);

  // Initialize a session to process statements that come from the same client connection.
  // If database_name is empty, a session is created without connecting to any database.
  CHECKED_STATUS InitSession(const PgEnv *pg_env, const string& database_name);

  // YB Memctx: Create, Destroy, and Reset must be "static" because a few contexts are created
  //            before YugaByte environments including PgGate are created and initialized.
  // Create YB Memctx. Each memctx will be associated with a Postgres's MemoryContext.
  static K2Memctx *CreateMemctx();

  // Destroy YB Memctx.
  static CHECKED_STATUS DestroyMemctx(K2Memctx *memctx);

  // Reset YB Memctx.
  static CHECKED_STATUS ResetMemctx(K2Memctx *memctx);

  // Cache statements in YB Memctx. When Memctx is destroyed, the statement is destructed.
  CHECKED_STATUS AddToCurrentMemctx(const K2Statement::ScopedRefPtr &stmt,
                                      K2Statement **handle);

  // Cache table descriptor in YB Memctx. When Memctx is destroyed, the descriptor is destructed.
  CHECKED_STATUS AddToCurrentMemctx(size_t table_desc_id,
                                      const TableInfo::ScopedRefPtr &table_desc);

  // Read table descriptor that was cached in YB Memctx.
  CHECKED_STATUS GetTabledescFromCurrentMemctx(size_t table_desc_id, TableInfo **handle);

  // Invalidate the sessions table cache.
  CHECKED_STATUS InvalidateCache();

  Result<bool> IsInitDbDone();

  Result<uint64_t> GetSharedCatalogVersion();

  // Remove all values and expressions that were bound to the given statement.
  CHECKED_STATUS ClearBinds(K2Statement *handle);

  // Search for type_entity.
  const YBCPgTypeEntity *FindTypeEntity(int type_oid);

  //------------------------------------------------------------------------------------------------
  // Connect database. Switch the connected database to the given "database_name".
  CHECKED_STATUS ConnectDatabase(const char *database_name);

  // Create database.
  CHECKED_STATUS NewCreateDatabase(const char *database_name,
                                   PgOid database_oid,
                                   PgOid source_database_oid,
                                   PgOid next_oid,
                                   K2Statement **handle);

  CHECKED_STATUS ExecCreateDatabase(K2Statement *handle);

  // Drop database.
  CHECKED_STATUS NewDropDatabase(const char *database_name,
                                 PgOid database_oid,
                                 K2Statement **handle);
  CHECKED_STATUS ExecDropDatabase(K2Statement *handle);

  // Reserve oids.
  CHECKED_STATUS ReserveOids(PgOid database_oid,
                             PgOid next_oid,
                             uint32_t count,
                             PgOid *begin_oid,
                             PgOid *end_oid);

  CHECKED_STATUS GetCatalogMasterVersion(uint64_t *version);

  // Load table.
  Result<TableInfo::ScopedRefPtr> LoadTable(const PgObjectId& table_id);

  // Invalidate the cache entry corresponding to table_id from the PgSession table cache.
  void InvalidateTableCache(const PgObjectId& table_id);

  //------------------------------------------------------------------------------------------------
  // Create, alter and drop table.
  CHECKED_STATUS NewCreateTable(const char *database_name,
                                const char *schema_name,
                                const char *table_name,
                                const PgObjectId& table_id,
                                bool is_shared_table,
                                bool if_not_exist,
                                bool add_primary_key,
                                K2Statement **handle);

  CHECKED_STATUS CreateTableAddColumn(K2Statement *handle, const char *attr_name, int attr_num,
                                      const YBCPgTypeEntity *attr_type, bool is_hash,
                                      bool is_range, bool is_desc, bool is_nulls_first);

  CHECKED_STATUS CreateTableAddSplitRow(K2Statement *handle, int num_cols,
                                        YBCPgTypeEntity **types, uint64_t *data);

  CHECKED_STATUS ExecCreateTable(K2Statement *handle);

  CHECKED_STATUS NewDropTable(const PgObjectId& table_id,
                              bool if_exist,
                              K2Statement **handle);

  CHECKED_STATUS ExecDropTable(K2Statement *handle);

  CHECKED_STATUS GetTableDesc(const PgObjectId& table_id,
                              TableInfo **handle);

  CHECKED_STATUS GetColumnInfo(TableInfo* table_desc,
                               int16_t attr_number,
                               bool *is_primary,
                               bool *is_hash);

  CHECKED_STATUS DmlModifiesRow(K2Statement *handle, bool *modifies_row);

  CHECKED_STATUS SetIsSysCatalogVersionChange(K2Statement *handle);

  CHECKED_STATUS SetCatalogCacheVersion(K2Statement *handle, uint64_t catalog_cache_version);

/*
  //------------------------------------------------------------------------------------------------
  // All DML statements
  CHECKED_STATUS DmlAppendTarget(K2Statement *handle, SqlExpr *expr);

  // Binding Columns: Bind column with a value (expression) in a statement.
  // + This API is used to identify the rows you want to operate on. If binding columns are not
  //   there, that means you want to operate on all rows (full scan). You can view this as a
  //   a definitions of an initial rowset or an optimization over full-scan.
  //
  // + There are some restrictions on when BindColumn() can be used.
  //   Case 1: INSERT INTO tab(x) VALUES(x_expr)
  //   - BindColumn() can be used for BOTH primary-key and regular columns.
  //   - This bind-column function is used to bind "x" with "x_expr", and "x_expr" that can contain
  //     bind-variables (placeholders) and constants whose values can be updated for each execution
  //     of the same allocated statement.
  //
  //   Case 2: SELECT / UPDATE / DELETE <WHERE key = "key_expr">
  //   - BindColumn() can only be used for primary-key columns.
  //   - This bind-column function is used to bind the primary column "key" with "key_expr" that can
  //     contain bind-variables (placeholders) and constants whose values can be updated for each
  //     execution of the same allocated statement.
  CHECKED_STATUS DmlBindColumn(YBCPgStatement handle, int attr_num, YBCPgExpr attr_value);
  CHECKED_STATUS DmlBindColumnCondEq(YBCPgStatement handle, int attr_num, YBCPgExpr attr_value);
  CHECKED_STATUS DmlBindColumnCondBetween(YBCPgStatement handle, int attr_num, YBCPgExpr attr_value,
      YBCPgExpr attr_value_end);
  CHECKED_STATUS DmlBindColumnCondIn(YBCPgStatement handle, int attr_num, int n_attr_values,
      YBCPgExpr *attr_value);

  // Binding Tables: Bind the whole table in a statement.  Do not use with BindColumn.
  CHECKED_STATUS DmlBindTable(YBCPgStatement handle);

  // API for SET clause.
  CHECKED_STATUS DmlAssignColumn(YBCPgStatement handle, int attr_num, YBCPgExpr attr_value);

  // This function is to fetch the targets in YBCPgDmlAppendTarget() from the rows that were defined
  // by YBCPgDmlBindColumn().
  CHECKED_STATUS DmlFetch(K2Statement *handle, int32_t natts, uint64_t *values, bool *isnulls,
                          PgSysColumns *syscols, bool *has_data);

  // Utility method that checks stmt type and calls exec insert, update, or delete internally.
  CHECKED_STATUS DmlExecWriteOp(K2Statement *handle, int32_t *rows_affected_count);

  // This function adds a primary column to be used in the construction of the tuple id (ybctid).
  CHECKED_STATUS DmlAddYBTupleIdColumn(K2Statement *handle, int attr_num, uint64_t datum,
                                       bool is_null, const YBCPgTypeEntity *type_entity);


  // This function returns the tuple id (ybctid) of a Postgres tuple.
  CHECKED_STATUS DmlBuildYBTupleId(K2Statement *handle, const PgAttrValueDescriptor *attrs,
                                   int32_t nattrs, uint64_t *ybctid);

  // DB Operations: SET, WHERE, ORDER_BY, GROUP_BY, etc.
  // + The following operations are run by DocDB.
  //   - API for "set_clause" (not yet implemented).
  //
  // + The following operations are run by Postgres layer. An API might be added to move these
  //   operations to DocDB.
  //   - API for "where_expr"
  //   - API for "order_by_expr"
  //   - API for "group_by_expr"

  // Buffer write operations.
  void StartOperationsBuffering();
  CHECKED_STATUS StopOperationsBuffering();
  CHECKED_STATUS ResetOperationsBuffering();
  CHECKED_STATUS FlushBufferedOperations();
  void DropBufferedOperations();

  //------------------------------------------------------------------------------------------------
  // Select.
  CHECKED_STATUS NewSelect(const PgObjectId& table_id,
                           const PgObjectId& index_id,
                           const PgPrepareParameters *prepare_params,
                           K2Statement **handle);

  CHECKED_STATUS SetForwardScan(K2Statement *handle, bool is_forward_scan);

  CHECKED_STATUS ExecSelect(K2Statement *handle, const PgExecParameters *exec_params);


  //------------------------------------------------------------------------------------------------
  // Expressions.
  //------------------------------------------------------------------------------------------------
  // Column reference.
  CHECKED_STATUS NewColumnRef(K2Statement *handle, int attr_num, const PgTypeEntity *type_entity,
                              const PgTypeAttrs *type_attrs, SqlExpr **expr_handle);

  // Constant expressions.
  CHECKED_STATUS NewConstant(YBCPgStatement stmt, const YBCPgTypeEntity *type_entity,
                             uint64_t datum, bool is_null, YBCPgExpr *expr_handle);
  CHECKED_STATUS NewConstantOp(YBCPgStatement stmt, const YBCPgTypeEntity *type_entity,
                             uint64_t datum, bool is_null, YBCPgExpr *expr_handle, bool is_gt);

  // TODO(neil) UpdateConstant should be merged into one.
  // Update constant.
  template<typename value_type>
  CHECKED_STATUS UpdateConstant(SqlExpr *expr, value_type value, bool is_null) {
    if (expr->opcode() != PgExpr::Opcode::PG_EXPR_CONSTANT) {
      // Invalid handle.
      return STATUS(InvalidArgument, "Invalid expression handle for constant");
    }
    down_cast<PgConstant*>(expr)->UpdateConstant(value, is_null);
    return Status::OK();
  }

  CHECKED_STATUS UpdateConstant(SqlExpr *expr, const char *value, bool is_null);

  CHECKED_STATUS UpdateConstant(SqlExpr *expr, const void *value, int64_t bytes, bool is_null);

  // Operators.
  CHECKED_STATUS NewOperator(K2Statement *stmt, const char *opname,
                             const YBCPgTypeEntity *type_entity,
                             SqlExpr **op_handle);
  CHECKED_STATUS OperatorAppendArg(SqlExpr *op_handle, SqlExpr *arg);

  // Foreign key reference caching.
  bool ForeignKeyReferenceExists(YBCPgOid table_id, std::string&& ybctid);
  CHECKED_STATUS CacheForeignKeyReference(YBCPgOid table_id, std::string&& ybctid);
  CHECKED_STATUS DeleteForeignKeyReference(YBCPgOid table_id, std::string&& ybctid);
  void ClearForeignKeyReferenceCache();
*/
  // Sets the specified timeout in the rpc service.
  void SetTimeout(int timeout_ms);

  private:

  K2Client* CreateK2Client();

  // Metrics.
  gscoped_ptr<MetricRegistry> metric_registry_;
  scoped_refptr<MetricEntity> metric_entity_;

  // Memory tracker.
  std::shared_ptr<MemTracker> mem_tracker_;

  // TODO(neil) Map for environments (we should have just one ENV?). Environments should contain
  // all the custom flags the PostgreSQL sets. We ignore them all for now.
  PgEnv::SharedPtr pg_env_;

  // Mapping table of YugaByte and PostgreSQL datatypes.
  std::unordered_map<int, const YBCPgTypeEntity *> type_map_;

  K2Client* k2_client_;

  scoped_refptr<K2Session> k2_session_;

  YBCPgCallbacks pg_callbacks_;
};

}  // namespace gate
}  // namespace k2

#endif //CHOGORI_GATE_API_H