// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dbgbreakpoint.h"

#include <algorithm>
#include <cctype>

using google_cloud_debugger_portable_pdb::DocumentIndex;
using google_cloud_debugger_portable_pdb::LocalConstantRow;
using google_cloud_debugger_portable_pdb::LocalScopeRow;
using google_cloud_debugger_portable_pdb::LocalVariableRow;
using std::string;
using std::vector;

namespace google_cloud_debugger {

void DbgBreakpoint::Initialize(const DbgBreakpoint &other) {
  set_ = other.set_;
  line_ = other.line_;
  column_ = other.column_;
  file_name_ = other.file_name_;
  id_ = other.id_;
  il_offset_ = other.il_offset_;
  method_def_ = other.method_def_;
  method_token_ = other.method_token_;
  method_name_ = other.method_name_;
  local_variables_ = other.local_variables_;
  local_constants_ = other.local_constants_;
  debug_breakpoint_ = other.debug_breakpoint_;
}

void DbgBreakpoint::Initialize(const string &file_name, const string &id,
                               uint32_t line, uint32_t column) {
  file_name_ = file_name;
  id_ = id;
  std::transform(
      file_name_.begin(), file_name_.end(), file_name_.begin(),
      [](unsigned char c) -> unsigned char { return std::tolower(c); });
  line_ = line;
  column_ = column;
}

HRESULT DbgBreakpoint::GetCorDebugBreakpoint(
    ICorDebugBreakpoint **debug_breakpoint) const {
  if (!debug_breakpoint) {
    return E_INVALIDARG;
  }

  if (debug_breakpoint_) {
    *debug_breakpoint = debug_breakpoint_;
    debug_breakpoint_->AddRef();
    return S_OK;
  }

  return E_FAIL;
}

bool DbgBreakpoint::TrySetBreakpoint(
    const google_cloud_debugger_portable_pdb::PortablePdbFile &pdb_file) {
  uint32_t current_doc_index_index = -1;
  uint32_t best_match_index = current_doc_index_index;
  size_t best_file_name_location_matched = UINT32_MAX;
  vector<google_cloud_debugger_portable_pdb::DocumentIndex> docs =
      pdb_file.GetDocumentIndexTable();

  // Loop through all the documents and try to find the one that
  // best matches the breakpoint's file name.
  for (auto &&document_index : pdb_file.GetDocumentIndexTable()) {
    ++current_doc_index_index;
    string document_name = document_index.GetFilePath();
    // Normalize path separators. The PDB may use either Unix or Windows-style
    // paths, but the Cloud Debugger only uses Unix.
    std::replace(document_name.begin(), document_name.end(), '\\', '/');
    if (document_name.size() < file_name_.size()) {
      continue;
    }

    std::transform(
        document_name.begin(), document_name.end(), document_name.begin(),
        [](unsigned char c) -> unsigned char { return std::tolower(c); });

    size_t file_name_location = document_name.rfind(file_name_);
    // Has to matched from the end.
    if (file_name_location + file_name_.size() != document_name.size()) {
      continue;
    }

    // Try to find the best match.
    // Best match here means the file with the longest path that matches
    // the file name so file_name_location should be as small as possible.
    if (file_name_location < best_file_name_location_matched) {
      // Checks that the line number indeed matches up.
      google_cloud_debugger_portable_pdb::DocumentIndex matched_doc =
          docs[current_doc_index_index];

      // Have to try to find the best method that matches.
      // This is because the breakpoint can be inside method A but
      // method A is inside method B and this means we should use method A
      // to get the local variables instead of method B.
      bool found_breakpoint = false;
      uint32_t best_matched_method_first_line = 0;
      for (auto &&method : matched_doc.GetMethods()) {
        if (method.first_line > line_ || method.last_line < line_) {
          continue;
        }

        // If this method first line is greater than the previous one,
        // this means that this method is inside it.
        if (method.first_line > best_matched_method_first_line) {
          // If this is false, it means no sequence points in the method
          // corresponds to this breakpoint.
          if (TrySetBreakpointInMethod(method)) {
            best_matched_method_first_line = method.first_line;
            found_breakpoint = true;
          }
        }
      }

      if (found_breakpoint) {
        best_file_name_location_matched = file_name_location;
        best_match_index = current_doc_index_index;
      }
    }
  }

  set_ = best_match_index != -1;
  return set_;
}

bool DbgBreakpoint::TrySetBreakpointInMethod(
    const google_cloud_debugger_portable_pdb::MethodInfo &method) {
  for (auto &&sequence_point : method.sequence_points) {
    if (sequence_point.start_line > line_ || sequence_point.end_line < line_) {
      continue;
    }

    il_offset_ = sequence_point.il_offset;
    method_def_ = method.method_def;

    // Loop through all scopes of method to retrieve local variables and
    // constants. We are only interested in scopes that encompasses this
    // sequence point.
    for (auto &&local_scope : method.local_scope) {
      if (local_scope.start_offset > il_offset_ ||
          local_scope.start_offset + local_scope.length < il_offset_) {
        continue;
      }

      local_variables_.clear();
      local_variables_.insert(local_variables_.end(),
                              local_scope.local_variables.begin(),
                              local_scope.local_variables.end());

      local_constants_.clear();
      local_constants_.insert(local_constants_.end(),
                              local_scope.local_constants.begin(),
                              local_scope.local_constants.end());
    }

    return true;
  }

  return false;
}

}  // namespace google_cloud_debugger
