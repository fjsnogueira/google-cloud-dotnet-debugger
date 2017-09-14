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

#ifndef DBG_CLASS_FIELD_H_
#define DBG_CLASS_FIELD_H_

#include <memory>
#include <sstream>
#include <vector>

#include "dbgobject.h"
#include "stringstreamwrapper.h"

namespace google_cloud_debugger {
class EvalCoordinator;

// Class that represents a field in a .NET class.
class DbgClassField : public StringStreamWrapper {
 public:
  // Initialize the field names, metadata signature, flags and values.
  // HRESULT will be stored in initialized_hr_.
  void Initialize(mdFieldDef fieldDef, IMetaDataImport *metadata_import,
                  ICorDebugObjectValue *debug_obj_value,
                  ICorDebugClass *debug_class,
                  ICorDebugType *class_type, int depth);

  // Sets the value of variable to the value of this field.
  HRESULT PopulateVariableValue(
      google::cloud::diagnostics::debug::Variable *variable,
      IEvalCoordinator *eval_coordinator);

  std::string GetFieldName() const {
    return ConvertWCharPtrToString(field_name_);
  }

  // Returns the HRESULT when Initialize function is called.
  HRESULT GetInitializeHr() const { return initialized_hr_; }

 private:
  // Token for the class that the field belongs to.
  mdTypeDef class_token_ = 0;

  // Token that represents this field.
  mdFieldDef field_def_ = 0;

  // Flags associated with the field's metadata.
  DWORD field_attributes_ = 0;

  // Pointer to signature meatdata value of the field.
  PCCOR_SIGNATURE signature_metadata_ = 0;

  // Size of signature_metadata_
  ULONG signature_metadata_len_ = 0;

  // A flag specifying the type of the constant that is the default value of the
  // property. This value is from the CorElementType enumeration.
  DWORD default_value_type_flags_ = 0;

  // The size in wide characters of default_value_ if default_value_type_flags
  // is ELEMENT_TYPE_STRING. Otherwise, it is not relevant.
  UVCP_CONSTANT default_value_ = 0;

  // The size in wide characters of default_value_ if default_value_type_flags
  // is ELEMENT_TYPE_STRING. Otherwise, it is not relevant.
  ULONG default_value_len_ = 0;

  // Name of the field. We don't use wstring because GetFieldProps expect
  // a WCHAR array.
  std::vector<WCHAR> field_name_;

  // Value of the field.
  std::unique_ptr<DbgObject> field_value_;

  // True if this is a static field.
  BOOL is_static_field_ = FALSE;

  // The HRESULT of initialization.
  HRESULT initialized_hr_ = S_OK;

  // Debug type of the class that this field belongs to.
  CComPtr<ICorDebugType> class_type_;

  // Depth of evaluation for this field.
  int depth_ = 0;
};

}  //  namespace google_cloud_debugger

#endif  //  DBG_CLASS_FIELD_H_
