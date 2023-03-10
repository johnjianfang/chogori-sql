//--------------------------------------------------------------------------------------------------
// Copyright (c) YugaByte, Inc.
// Portions Copyright (c) 2021 Futurewei Cloud
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
//
// Yugabyte extensions to pqcomm that should be accessible to the other parts of the code.
//--------------------------------------------------------------------------------------------------

#ifndef K2PG_PQCOMM_EXTENSIONS_H
#define K2PG_PQCOMM_EXTENSIONS_H

extern void K2PgSaveOutputBufferPosition(bool sending_non_restartable_data);
extern void K2PgRestoreOutputBufferPosition(void);

#endif // K2PG_PQCOMM_EXTENSIONS_H
