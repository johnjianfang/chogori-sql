// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
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

#include "yb/common/concurrent/ref_counted.h"

#include <regex>
#include <atomic>
#include <mutex>

#include <glog/logging.h>

#include "yb/common/concurrent/thread_collision_warner.h"

namespace yb {

namespace subtle {

RefCountedBase::RefCountedBase()
    : ref_count_(0)
#ifndef NDEBUG
    , in_dtor_(false)
#endif
    {
}

RefCountedBase::~RefCountedBase() {
#ifndef NDEBUG
  DCHECK(in_dtor_) << "RefCounted object deleted without calling Release()";
#endif
}

void RefCountedBase::AddRef() const {
  // TODO(maruel): Add back once it doesn't assert 500 times/sec.
  // Current thread books the critical section "AddRelease" without release it.
  // DFAKE_SCOPED_LOCK_THREAD_LOCKED(add_release_);
#ifndef NDEBUG
  DCHECK(!in_dtor_);
#endif
  ++ref_count_;
}

bool RefCountedBase::Release() const {
  // TODO(maruel): Add back once it doesn't assert 500 times/sec.
  // Current thread books the critical section "AddRelease" without release it.
  // DFAKE_SCOPED_LOCK_THREAD_LOCKED(add_release_);
#ifndef NDEBUG
  DCHECK(!in_dtor_);
#endif
  if (--ref_count_ == 0) {
#ifndef NDEBUG
    in_dtor_ = true;
#endif
    return true;
  }
  return false;
}

bool RefCountedThreadSafeBase::HasOneRef() const {
  return base::RefCountIsOne(
      &const_cast<RefCountedThreadSafeBase*>(this)->ref_count_);
}

RefCountedThreadSafeBase::RefCountedThreadSafeBase() : ref_count_(0) {
#ifndef NDEBUG
  in_dtor_ = false;
#endif
}

RefCountedThreadSafeBase::~RefCountedThreadSafeBase() {
#ifndef NDEBUG
  DCHECK(in_dtor_) << "RefCountedThreadSafe object deleted without "
                      "calling Release()";
#endif
}

void RefCountedThreadSafeBase::AddRef() const {
#ifndef NDEBUG
  DCHECK(!in_dtor_);
#endif
  base::RefCountInc(&ref_count_);
}

bool RefCountedThreadSafeBase::Release() const {
#ifndef NDEBUG
  DCHECK(!in_dtor_);
  DCHECK(!base::RefCountIsZero(&ref_count_));
#endif
  if (!base::RefCountDec(&ref_count_)) {
#ifndef NDEBUG
    in_dtor_ = true;
#endif
    return true;
  }
  return false;
}

#ifndef NDEBUG

bool g_ref_counted_debug_enabled = false;

namespace {

RefCountedDebugFn* g_ref_counted_debug_report_event_fn;
std::regex g_ref_counted_debug_type_name_regex;

}  // anonymous namespace

void InitRefCountedDebugging(const std::string& type_name_regex,
                             RefCountedDebugFn* debug_fn) {
  g_ref_counted_debug_report_event_fn = debug_fn;
  g_ref_counted_debug_enabled = !type_name_regex.empty();
  if (g_ref_counted_debug_enabled) {
    g_ref_counted_debug_type_name_regex = std::regex(type_name_regex);
  }
}

void RefCountedDebugHook(
    const char* type_name,
    const void* this_ptr,
    int32_t current_ref_count,
    int32_t ref_delta) {
  std::match_results<const char*> match;
  if (!std::regex_match(type_name, match, g_ref_counted_debug_type_name_regex)) {
    return;
  }

  (*g_ref_counted_debug_report_event_fn)(type_name, this_ptr, current_ref_count, ref_delta);
}

#else

void InitRefCountedDebugging(const std::string& type_name_regex) {
}

#endif

}  // namespace subtle

}  // namespace yb
