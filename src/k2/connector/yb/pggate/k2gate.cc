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

#include "yb/pggate/k2gate.h"
#include "yb/pggate/k2ddl.h"
#include "yb/pggate/k2dml.h"

namespace k2 {
namespace gate {

using std::make_shared;

K2GateApiImpl::K2GateApiImpl(const YBCPgTypeEntity *YBCDataTypeArray, int count, YBCPgCallbacks callbacks)
    : metric_registry_(new MetricRegistry()),
      metric_entity_(METRIC_ENTITY_server.Instantiate(metric_registry_.get(), "k2.pggate")),
      mem_tracker_(MemTracker::CreateTracker("PostgreSQL")),
      k2_client_(CreateK2Client()),
      pg_callbacks_(callbacks) {
  // Setup type mapping.
  for (int idx = 0; idx < count; idx++) {
    const YBCPgTypeEntity *type_entity = &YBCDataTypeArray[idx];
    type_map_[type_entity->type_oid] = type_entity;
  }
  k2_client_->Init();
}

K2Client* K2GateApiImpl::CreateK2Client() {
  // TODO: add more complex logic to create k2 client, for example, from a pool  
  K2Client* client = new K2Client();
  return client;
}

K2GateApiImpl::~K2GateApiImpl() {
  k2_client_->Shutdown();
}

const YBCPgTypeEntity *K2GateApiImpl::FindTypeEntity(int type_oid) {
  const auto iter = type_map_.find(type_oid);
  if (iter != type_map_.end()) {
    return iter->second;
  }
  return nullptr;
}

CHECKED_STATUS AddColumn(K2CreateTable* pg_stmt, const char *attr_name, int attr_num,
                         const YBCPgTypeEntity *attr_type, bool is_hash, bool is_range,
                         bool is_desc, bool is_nulls_first) {
  using SortingType = ColumnSchema::SortingType;
  SortingType sorting_type = SortingType::kNotSpecified;

  if (!is_hash && is_range) {
    if (is_desc) {
      sorting_type = is_nulls_first ? SortingType::kDescending : SortingType::kDescendingNullsLast;
    } else {
      sorting_type = is_nulls_first ? SortingType::kAscending : SortingType::kAscendingNullsLast;
    }
  }

  return pg_stmt->AddColumn(attr_name, attr_num, attr_type, is_hash, is_range, sorting_type);
}

//--------------------------------------------------------------------------------------------------

Status K2GateApiImpl::CreateEnv(PgEnv **pg_env) {
  *pg_env = pg_env_.get();
  return Status::OK();
}

Status K2GateApiImpl::DestroyEnv(PgEnv *pg_env) {
  pg_env_ = nullptr;
  return Status::OK();
}

//--------------------------------------------------------------------------------------------------

Status K2GateApiImpl::InitSession(const PgEnv *pg_env,
                              const string& database_name) {
  CHECK(!k2_session_);
  auto session = make_scoped_refptr<K2Session>(k2_client_,
                                               database_name,
                                               pg_callbacks_);
  if (!database_name.empty()) {
    RETURN_NOT_OK(session->ConnectDatabase(database_name));
  }

  k2_session_.swap(session);
  return Status::OK();
}

Status K2GateApiImpl::InvalidateCache() {
  k2_session_->InvalidateCache();
  return Status::OK();
}

Result<bool> K2GateApiImpl::IsInitDbDone() {
  return k2_session_->IsInitDbDone();
}

Result<uint64_t> K2GateApiImpl::GetSharedCatalogVersion() {
  return k2_session_->GetSharedCatalogVersion();
}

//--------------------------------------------------------------------------------------------------

K2Memctx *K2GateApiImpl::CreateMemctx() {
  // Postgres will create YB Memctx when it first use the Memctx to allocate YugaByte object.
  return K2Memctx::Create();
}

Status K2GateApiImpl::DestroyMemctx(K2Memctx *memctx) {
  // Postgres will destroy YB Memctx by releasing the pointer.
  return K2Memctx::Destroy(memctx);
}

Status K2GateApiImpl::ResetMemctx(K2Memctx *memctx) {
  // Postgres reset YB Memctx when clearing a context content without clearing its nested context.
  return K2Memctx::Reset(memctx);
}

// TODO(neil) Use Arena in the future.
// - PgStatement should have been declared as derived class of "MCBase".
// - All objects of PgStatement's derived class should be allocated by YbPgMemctx::Arena.
// - We cannot use Arena yet because quite a large number of YugaByte objects are being referenced
//   from other layers.  Those added code violated the original design as they assume ScopedPtr
//   instead of memory pool is being used. This mess should be cleaned up later.
//
// For now, statements is allocated as ScopedPtr and cached in the memory context. The statements
// would then be destructed when the context is destroyed and all other references are also cleared.
Status K2GateApiImpl::AddToCurrentMemctx(const K2Statement::ScopedRefPtr &stmt,
                                       K2Statement **handle) {
  pg_callbacks_.GetCurrentYbMemctx()->Cache(stmt);
  *handle = stmt.get();
  return Status::OK();
}

// TODO(neil) Most like we don't need table_desc. If we do need it, use Arena here.
// - PgTableDesc should have been declared as derived class of "MCBase".
// - PgTableDesc objects should be allocated by YbPgMemctx::Arena.
//
// For now, table_desc is allocated as ScopedPtr and cached in the memory context. The table_desc
// would then be destructed when the context is destroyed.
Status K2GateApiImpl::AddToCurrentMemctx(size_t table_desc_id,
                                       const K2TableDesc::ScopedRefPtr &table_desc) {
  pg_callbacks_.GetCurrentYbMemctx()->Cache(table_desc_id, table_desc);
  return Status::OK();
}

Status K2GateApiImpl::GetTabledescFromCurrentMemctx(size_t table_desc_id, K2TableDesc **handle) {
  pg_callbacks_.GetCurrentYbMemctx()->GetCache(table_desc_id, handle);
  return Status::OK();
}

//--------------------------------------------------------------------------------------------------

Status K2GateApiImpl::ClearBinds(K2Statement *handle) {
  return handle->ClearBinds();
}

//--------------------------------------------------------------------------------------------------

Status K2GateApiImpl::ConnectDatabase(const char *database_name) {
  return k2_session_->ConnectDatabase(database_name);
}

Status K2GateApiImpl::NewCreateDatabase(const char *database_name,
                                    const PgOid database_oid,
                                    const PgOid source_database_oid,
                                    const PgOid next_oid,
                                    K2Statement **handle) {
  auto stmt = make_scoped_refptr<K2CreateDatabase>(k2_session_, database_name, database_oid,
                                                   source_database_oid, next_oid);
  RETURN_NOT_OK(AddToCurrentMemctx(stmt, handle));
  return Status::OK();
}

Status K2GateApiImpl::ExecCreateDatabase(K2Statement *handle) {
  if (!K2Statement::IsValidStmt(handle, StmtOp::STMT_CREATE_DATABASE)) {
    // Invalid handle.
    return STATUS(InvalidArgument, "Invalid statement handle");
  }

  return down_cast<K2CreateDatabase*>(handle)->Exec();
}

Status K2GateApiImpl::NewDropDatabase(const char *database_name,
                                  PgOid database_oid,
                                  K2Statement **handle) {
  auto stmt = make_scoped_refptr<K2DropDatabase>(k2_session_, database_name, database_oid);
  RETURN_NOT_OK(AddToCurrentMemctx(stmt, handle));
  return Status::OK();
}

Status K2GateApiImpl::ExecDropDatabase(K2Statement *handle) {
  if (!K2Statement::IsValidStmt(handle, StmtOp::STMT_DROP_DATABASE)) {
    // Invalid handle.
    return STATUS(InvalidArgument, "Invalid statement handle");
  }
  return down_cast<K2DropDatabase*>(handle)->Exec();
}

Status K2GateApiImpl::ReserveOids(const PgOid database_oid,
                              const PgOid next_oid,
                              const uint32_t count,
                              PgOid *begin_oid,
                              PgOid *end_oid) {
  return k2_session_->ReserveOids(database_oid, next_oid, count, begin_oid, end_oid);
}

Status K2GateApiImpl::GetCatalogMasterVersion(uint64_t *version) {
  return k2_session_->GetCatalogMasterVersion(version);
}

Result<K2TableDesc::ScopedRefPtr> K2GateApiImpl::LoadTable(const PgObjectId& table_id) {
  return k2_session_->LoadTable(table_id);
}

void K2GateApiImpl::InvalidateTableCache(const PgObjectId& table_id) {
  k2_session_->InvalidateTableCache(table_id);
}

//--------------------------------------------------------------------------------------------------

Status K2GateApiImpl::NewCreateTable(const char *database_name,
                                 const char *schema_name,
                                 const char *table_name,
                                 const PgObjectId& table_id,
                                 bool is_shared_table,
                                 bool if_not_exist,
                                 bool add_primary_key,
                                 K2Statement **handle) {
  auto stmt = make_scoped_refptr<K2CreateTable>(
      k2_session_, database_name, schema_name, table_name,
      table_id, is_shared_table, if_not_exist, add_primary_key);
  RETURN_NOT_OK(AddToCurrentMemctx(stmt, handle));
  return Status::OK();
}

Status K2GateApiImpl::CreateTableAddColumn(K2Statement *handle, const char *attr_name, int attr_num,
                                       const YBCPgTypeEntity *attr_type,
                                       bool is_hash, bool is_range,
                                       bool is_desc, bool is_nulls_first) {
  if (!K2Statement::IsValidStmt(handle, StmtOp::STMT_CREATE_TABLE)) {
    // Invalid handle.
    return STATUS(InvalidArgument, "Invalid statement handle");
  }
  return AddColumn(down_cast<K2CreateTable*>(handle), attr_name, attr_num, attr_type,
      is_hash, is_range, is_desc, is_nulls_first);
}

Status K2GateApiImpl::CreateTableAddSplitRow(K2Statement *handle, int num_cols,
                                           YBCPgTypeEntity **types, uint64_t *data) {
  if (!K2Statement::IsValidStmt(handle, StmtOp::STMT_CREATE_TABLE)) {
    // Invalid handle.
    return STATUS(InvalidArgument, "Invalid statement handle");
  }
  return down_cast<K2CreateTable*>(handle)->AddSplitRow(num_cols, types, data);
}

Status K2GateApiImpl::ExecCreateTable(K2Statement *handle) {
  if (!K2Statement::IsValidStmt(handle, StmtOp::STMT_CREATE_TABLE)) {
    // Invalid handle.
    return STATUS(InvalidArgument, "Invalid statement handle");
  }
  return down_cast<K2CreateTable*>(handle)->Exec();
}

Status K2GateApiImpl::NewDropTable(const PgObjectId& table_id,
                               bool if_exist,
                               K2Statement **handle) {
  auto stmt = make_scoped_refptr<K2DropTable>(k2_session_, table_id, if_exist);
  RETURN_NOT_OK(AddToCurrentMemctx(stmt, handle));
  return Status::OK();
}

Status K2GateApiImpl::ExecDropTable(K2Statement *handle) {
  if (!K2Statement::IsValidStmt(handle, StmtOp::STMT_DROP_TABLE)) {
    // Invalid handle.
    return STATUS(InvalidArgument, "Invalid statement handle");
  }
  return down_cast<K2DropTable*>(handle)->Exec();
}

Status K2GateApiImpl::GetTableDesc(const PgObjectId& table_id,
                               K2TableDesc **handle) {
  // First read from memory context.
  size_t hash_id = hash_value(table_id);
  RETURN_NOT_OK(GetTabledescFromCurrentMemctx(hash_id, handle));

  // Read from environment.
  if (*handle == nullptr) {
    auto result = k2_session_->LoadTable(table_id);
    RETURN_NOT_OK(result);
    RETURN_NOT_OK(AddToCurrentMemctx(hash_id, *result));

    *handle = result->get();
  }

  return Status::OK();
}

Status K2GateApiImpl::GetColumnInfo(K2TableDesc* table_desc,
                                int16_t attr_number,
                                bool *is_primary,
                                bool *is_hash) {
  return table_desc->GetColumnInfo(attr_number, is_primary, is_hash);
}

Status K2GateApiImpl::DmlModifiesRow(K2Statement *handle, bool *modifies_row) {
  if (!handle) {
    return STATUS(InvalidArgument, "Invalid statement handle");
  }

  *modifies_row = false;

  switch (handle->stmt_op()) {
    case StmtOp::STMT_UPDATE:
    case StmtOp::STMT_DELETE:
      *modifies_row = true;
      break;
    default:
      break;
  }

  return Status::OK();
}

Status K2GateApiImpl::SetIsSysCatalogVersionChange(K2Statement *handle) {
  if (!handle) {
    return STATUS(InvalidArgument, "Invalid statement handle");
  }

  switch (handle->stmt_op()) {
    case StmtOp::STMT_UPDATE:
    case StmtOp::STMT_DELETE:
    case StmtOp::STMT_INSERT:
      down_cast<K2DmlWrite *>(handle)->SetIsSystemCatalogChange();
      return Status::OK();
    default:
      break;
  }

  return STATUS(InvalidArgument, "Invalid statement handle");
}

Status K2GateApiImpl::SetCatalogCacheVersion(K2Statement *handle, uint64_t catalog_cache_version) {
  if (!handle) {
    return STATUS(InvalidArgument, "Invalid statement handle");
  }

  switch (handle->stmt_op()) {
    case StmtOp::STMT_SELECT:
    case StmtOp::STMT_INSERT:
    case StmtOp::STMT_UPDATE:
    case StmtOp::STMT_DELETE:
      down_cast<K2Dml *>(handle)->SetCatalogCacheVersion(catalog_cache_version);
      return Status::OK();
    default:
      break;
  }

  return STATUS(InvalidArgument, "Invalid statement handle");
}

// Binding -----------------------------------------------------------------------------------------

Status K2GateApiImpl::DmlAppendTarget(K2Statement *handle, PgExpr *target) {
  return down_cast<K2Dml*>(handle)->AppendTarget(target);
}

Status K2GateApiImpl::DmlBindColumn(K2Statement *handle, int attr_num, PgExpr *attr_value) {
  return down_cast<K2Dml*>(handle)->BindColumn(attr_num, attr_value);
}

Status K2GateApiImpl::DmlBindColumnCondEq(K2Statement *handle, int attr_num, PgExpr *attr_value) {
  return down_cast<K2DmlRead*>(handle)->BindColumnCondEq(attr_num, attr_value);
}

Status K2GateApiImpl::DmlBindColumnCondBetween(K2Statement *handle, int attr_num, PgExpr *attr_value,
    PgExpr *attr_value_end) {
  return down_cast<K2DmlRead*>(handle)->BindColumnCondBetween(attr_num, attr_value, attr_value_end);
}

Status K2GateApiImpl::DmlBindColumnCondIn(K2Statement *handle, int attr_num, int n_attr_values,
    PgExpr **attr_values) {
  return down_cast<K2DmlRead*>(handle)->BindColumnCondIn(attr_num, n_attr_values, attr_values);
}

Status K2GateApiImpl::DmlBindTable(K2Statement *handle) {
  return down_cast<K2Dml*>(handle)->BindTable();
}

CHECKED_STATUS K2GateApiImpl::DmlAssignColumn(K2Statement *handle, int attr_num, PgExpr *attr_value) {
  return down_cast<K2Dml*>(handle)->AssignColumn(attr_num, attr_value);
}

Status K2GateApiImpl::DmlFetch(K2Statement *handle, int32_t natts, uint64_t *values, bool *isnulls,
                           PgSysColumns *syscols, bool *has_data) {
  return down_cast<K2Dml*>(handle)->Fetch(natts, values, isnulls, syscols, has_data);
}

Status K2GateApiImpl::DmlBuildYBTupleId(K2Statement *handle, const PgAttrValueDescriptor *attrs,
                                    int32_t nattrs, uint64_t *ybctid) {
  const string id = VERIFY_RESULT(down_cast<K2Dml*>(handle)->BuildYBTupleId(attrs, nattrs));
  const YBCPgTypeEntity *type_entity = FindTypeEntity(kPgByteArrayOid);
  *ybctid = type_entity->yb_to_datum(id.data(), id.size(), nullptr /* type_attrs */);
  return Status::OK();
}

Status K2GateApiImpl::DmlExecWriteOp(K2Statement *handle, int32_t *rows_affected_count) {
  switch (handle->stmt_op()) {
    case StmtOp::STMT_INSERT:
    case StmtOp::STMT_UPDATE:
    case StmtOp::STMT_DELETE:
    case StmtOp::STMT_TRUNCATE:
      {
        auto dml_write = down_cast<K2DmlWrite *>(handle);
        RETURN_NOT_OK(dml_write->Exec(rows_affected_count != nullptr /* force_non_bufferable */));
        if (rows_affected_count) {
          *rows_affected_count = dml_write->GetRowsAffectedCount();
        }
        return Status::OK();
      }
    default:
      break;
  }
  return STATUS(InvalidArgument, "Invalid statement handle");
}

bool K2GateApiImpl::ForeignKeyReferenceExists(YBCPgOid table_id, std::string&& ybctid) {
  return k2_session_->ForeignKeyReferenceExists(table_id, std::move(ybctid));
}

Status K2GateApiImpl::CacheForeignKeyReference(YBCPgOid table_id, std::string&& ybctid) {
  return k2_session_->CacheForeignKeyReference(table_id, std::move(ybctid));
}

Status K2GateApiImpl::DeleteForeignKeyReference(YBCPgOid table_id, std::string&& ybctid) {
  return k2_session_->DeleteForeignKeyReference(table_id, std::move(ybctid));
}

void K2GateApiImpl::ClearForeignKeyReferenceCache() {
  k2_session_->InvalidateForeignKeyReferenceCache();
}

void K2GateApiImpl::SetTimeout(const int timeout_ms) {
  k2_session_->SetTimeout(timeout_ms);
}

// Column references -------------------------------------------------------------------------------

Status K2GateApiImpl::NewColumnRef(K2Statement *stmt, int attr_num, const PgTypeEntity *type_entity,
                               const PgTypeAttrs *type_attrs, PgExpr **expr_handle) {
  if (!stmt) {
    // Invalid handle.
    return STATUS(InvalidArgument, "Invalid statement handle");
  }
  PgColumnRef::SharedPtr colref = make_shared<PgColumnRef>(attr_num, type_entity, type_attrs);
  stmt->AddExpr(colref);

  *expr_handle = colref.get();
  return Status::OK();
}

// Constant ----------------------------------------------------------------------------------------

Status K2GateApiImpl::NewConstant(K2Statement *stmt, const YBCPgTypeEntity *type_entity,
                              uint64_t datum, bool is_null, PgExpr **expr_handle) {
  if (!stmt) {
    // Invalid handle.
    return STATUS(InvalidArgument, "Invalid statement handle");
  }
  PgExpr::SharedPtr pg_const = make_shared<PgConstant>(type_entity, datum, is_null);
  stmt->AddExpr(pg_const);

  *expr_handle = pg_const.get();
  return Status::OK();
}

Status K2GateApiImpl::NewConstantOp(K2Statement *stmt, const YBCPgTypeEntity *type_entity,
                              uint64_t datum, bool is_null, PgExpr **expr_handle, bool is_gt) {
  if (!stmt) {
    // Invalid handle.
    return STATUS(InvalidArgument, "Invalid statement handle");
  }
  PgExpr::SharedPtr pg_const = make_shared<PgConstant>(type_entity, datum, is_null,
      is_gt ? PgExpr::Opcode::PG_EXPR_GT : PgExpr::Opcode::PG_EXPR_LT);
  stmt->AddExpr(pg_const);

  *expr_handle = pg_const.get();
  return Status::OK();
}

// Text constant -----------------------------------------------------------------------------------

Status K2GateApiImpl::UpdateConstant(PgExpr *expr, const char *value, bool is_null) {
  if (expr->opcode() != PgExpr::Opcode::PG_EXPR_CONSTANT) {
    // Invalid handle.
    return STATUS(InvalidArgument, "Invalid expression handle for constant");
  }
  down_cast<PgConstant*>(expr)->UpdateConstant(value, is_null);
  return Status::OK();
}

Status K2GateApiImpl::UpdateConstant(PgExpr *expr, const char *value, int64_t bytes, bool is_null) {
  if (expr->opcode() != PgExpr::Opcode::PG_EXPR_CONSTANT) {
    // Invalid handle.
    return STATUS(InvalidArgument, "Invalid expression handle for constant");
  }
  down_cast<PgConstant*>(expr)->UpdateConstant(value, bytes, is_null);
  return Status::OK();
}

// Text constant -----------------------------------------------------------------------------------

Status K2GateApiImpl::NewOperator(K2Statement *stmt, const char *opname,
                              const YBCPgTypeEntity *type_entity,
                              PgExpr **op_handle) {
  if (!stmt) {
    // Invalid handle.
    return STATUS(InvalidArgument, "Invalid statement handle");
  }
  RETURN_NOT_OK(PgExpr::CheckOperatorName(opname));

  // Create operator.
  PgExpr::SharedPtr pg_op = make_shared<PgOperator>(opname, type_entity);
  stmt->AddExpr(pg_op);

  *op_handle = pg_op.get();
  return Status::OK();
}

Status K2GateApiImpl::OperatorAppendArg(PgExpr *op_handle, PgExpr *arg) {
  if (!op_handle || !arg) {
    // Invalid handle.
    return STATUS(InvalidArgument, "Invalid expression handle");
  }
  down_cast<PgOperator*>(op_handle)->AppendArg(arg);
  return Status::OK();
}

}  // namespace gate
}  // namespace k2
