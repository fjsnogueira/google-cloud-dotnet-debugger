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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <string>

#include "ccomptr.h"
#include "common_action_mocks.h"
#include "dbgclassproperty.h"
#include "i_cordebug_mocks.h"
#include "i_evalcoordinator_mock.h"
#include "i_metadataimport_mock.h"

using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::SetArrayArgument;
using ::testing::_;
using google::cloud::diagnostics::debug::Variable;
using google_cloud_debugger::CComPtr;
using google_cloud_debugger::DbgClassProperty;
using std::string;
using std::vector;

namespace google_cloud_debugger_test {

// Test Fixture for DbgClassField.
class DbgClassPropertyTest : public ::testing::Test {
 protected:
  virtual void SetUp() {}

  virtual void SetUpProperty() {
    uint32_t class_property_name_len = class_property_name_.size();

    // GetPropertyProps should be called twice.
    EXPECT_CALL(metadataimport_mock_,
                GetPropertyPropsFirst(property_def_, _, _, _, _, _, _, _, _))
        .Times(2)
        .WillOnce(DoAll(
            SetArgPointee<4>(class_property_name_len + 1),
            Return(S_OK)))  // Sets the length of the class the first time.
        .WillOnce(DoAll(
            SetArg2ToWcharArray(&wchar_string_[0], class_property_name_len),
            SetArgPointee<4>(class_property_name_len),
            Return(S_OK)));  // Sets the class' name the second time.

    EXPECT_CALL(metadataimport_mock_,
                GetPropertyPropsSecond(_, _, _, _, _, _, _))
        .Times(2)
        .WillRepeatedly(DoAll(SetArgPointee<6>(1), Return(S_OK)));

    class_property_.Initialize(property_def_, &metadataimport_mock_);

    HRESULT hr = class_property_.GetInitializeHr();
    EXPECT_TRUE(SUCCEEDED(hr)) << "Failed with hr: " << hr;
  }

  // ICorDebugType for this field.
  ICorDebugTypeMock type_mock_;

  // Property Def for this field.
  mdProperty property_def_ = 10;

  // Class that this property belongs too.
  ICorDebugClassMock debug_class_;

  // MetaDataImport for this class.
  IMetaDataImportMock metadataimport_mock_;

  // ICorDebugModule extracted from ICorDebugClass.
  ICorDebugModuleMock debug_module_;

  // ICorDebugFunction extracted from Module.
  ICorDebugFunctionMock debug_function_;

  // Object that represents the class of this property.
  ICorDebugObjectValueMock object_value_;

  // Reference to the object_value_.
  ICorDebugReferenceValueMock reference_value_;

  // Object represents the value of this property.
  ICorDebugGenericValueMock generic_value_;

  // ICorDebugEvals created when trying to evaluate the property.
  ICorDebugEvalMock debug_eval_;
  ICorDebugEval2Mock debug_eval2_;

  // EvalCoordinator used to evaluate the property.
  IEvalCoordinatorMock eval_coordinator_mock_;

  // DbgClassProperty for the test.
  // Note that this has to be the first to be destroyed (that is
  // why it is placed near the end) among the mock objects.
  // This is because if the other mock objects are destroyed first,
  // this class will try to destroy the mock objects it stored again,
  // causing error.
  DbgClassProperty class_property_;

  string class_property_name_ = "PropertyName";
// TODO(quoct): The WCHAR_STRING macro is supposed to expand
// the string literal but was not able to compile on Linux.
#ifdef PAL_STDCPP_COMPAT
  WCHAR wchar_string_[13] = u"PropertyName";
#else
  WCHAR wchar_string_[13] = L"PropertyName";
#endif
};

// Tests the Initialize function of DbgClassProperty.
TEST_F(DbgClassPropertyTest, TestInitialize) {
  SetUpProperty();
  EXPECT_EQ(class_property_.GetPropertyName(), class_property_name_);
}

// Tests error cases for the Initialize function of DbgClassProperty.
TEST_F(DbgClassPropertyTest, TestInitializeError) {
  class_property_.Initialize(property_def_, nullptr);
  EXPECT_EQ(class_property_.GetInitializeHr(), E_INVALIDARG);

  EXPECT_CALL(metadataimport_mock_,
              GetPropertyPropsFirst(property_def_, _, _, _, _, _, _, _, _))
      .Times(1)
      .WillRepeatedly(Return(E_ACCESSDENIED));

  EXPECT_CALL(metadataimport_mock_, GetPropertyPropsSecond(_, _, _, _, _, _, _))
      .Times(1)
      .WillRepeatedly(Return(E_ACCESSDENIED));

  class_property_.Initialize(property_def_, &metadataimport_mock_);
  EXPECT_EQ(class_property_.GetInitializeHr(), E_ACCESSDENIED);
}

// Tests the PopulateVariableValue function of DbgClassProperty.
TEST_F(DbgClassPropertyTest, TestPopulateVariableValue) {
  SetUpProperty();

  // Sets various expectation for PopulateVariableValue call.
  EXPECT_CALL(reference_value_, Dereference(_))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgPointee<0>(&object_value_), Return(S_OK)));

  // ICorDebugReferenceValue should dereference to object value.
  EXPECT_CALL(object_value_, QueryInterface(_, _))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgPointee<1>(&object_value_), Return(S_OK)));

  // From object_value, ICorDebugClass should be extracted.
  EXPECT_CALL(object_value_, GetClass(_))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgPointee<0>(&debug_class_), Return(S_OK)));

  // ICorDebugModule extracted from ICorDebugClass.
  EXPECT_CALL(debug_class_, GetModule(_))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgPointee<0>(&debug_module_), Return(S_OK)));

  // ICorDebugFunction extracted from Module.
  EXPECT_CALL(debug_module_, GetFunctionFromToken(_, _))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgPointee<1>(&debug_function_), Return(S_OK)));

  // ICorDebugEval created from eval_coordinator_mock.
  EXPECT_CALL(eval_coordinator_mock_, CreateEval(_))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgPointee<0>(&debug_eval_), Return(S_OK)));

  // ICorDebugEval2 extracted from ICorDebugEval.
  EXPECT_CALL(debug_eval_, QueryInterface(_, _))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgPointee<1>(&debug_eval2_), Return(S_OK)));

  // IEvalCoordinator returns a generic value.
  // When GetValue is called from generic value, returns 20 as the value.
  int32_t int32_value = 20;
  SetUpMockGenericValue(&generic_value_, int32_value);

  EXPECT_CALL(eval_coordinator_mock_, WaitForEval(_, _, _))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgPointee<2>(&generic_value_), Return(S_OK)));

  Variable variable;
  vector<CComPtr<ICorDebugType>> generic_types;

  {
    // CallParameterizedFunction of ICorDebugEval2 is called
    // with 0 arguments (since size of generic types passing in is 0).
    EXPECT_CALL(debug_eval2_, CallParameterizedFunction(_, 0, _, 1, _))
        .Times(1)
        .WillRepeatedly(Return(S_OK));

    EXPECT_EQ(class_property_.PopulateVariableValue(
                  &variable, &reference_value_, &eval_coordinator_mock_,
                  &generic_types, 1),
              S_OK);

    EXPECT_EQ(variable.type(), "System.Int32");
    EXPECT_EQ(variable.value(), std::to_string(int32_value));
  }

  {
    generic_types.resize(2);
    // CallParameterizedFunction of ICorDebugEval2 is called
    // with 2 arguments (since size of generic types passing in is 0).
    EXPECT_CALL(debug_eval2_, CallParameterizedFunction(_, 2, _, 1, _))
        .Times(1)
        .WillRepeatedly(Return(S_OK));

    EXPECT_EQ(class_property_.PopulateVariableValue(
                  &variable, &reference_value_, &eval_coordinator_mock_,
                  &generic_types, 1),
              S_OK);

    EXPECT_EQ(variable.type(), "System.Int32");
    EXPECT_EQ(variable.value(), std::to_string(int32_value));
  }
}

// Tests the PopulateVariableValue function of DbgClassProperty.
TEST_F(DbgClassPropertyTest, TestPopulateVariableValueError) {
  SetUpProperty();

  Variable variable;
  vector<CComPtr<ICorDebugType>> generic_types;

  // Checks null argument error.
  EXPECT_EQ(class_property_.PopulateVariableValue(nullptr, &reference_value_,
                                                  &eval_coordinator_mock_,
                                                  &generic_types, 1),
            E_INVALIDARG);
  EXPECT_EQ(class_property_.PopulateVariableValue(
                &variable, nullptr, &eval_coordinator_mock_, &generic_types, 1),
            E_INVALIDARG);
  EXPECT_EQ(class_property_.PopulateVariableValue(&variable, &reference_value_,
                                                  nullptr, &generic_types, 1),
            E_INVALIDARG);
  EXPECT_EQ(
      class_property_.PopulateVariableValue(
          &variable, &reference_value_, &eval_coordinator_mock_, nullptr, 1),
      E_INVALIDARG);

  {
    // Errors out if dereference fails.
    EXPECT_CALL(reference_value_, Dereference(_))
        .Times(1)
        .WillRepeatedly(Return(CORDBG_E_BAD_REFERENCE_VALUE));

    EXPECT_EQ(class_property_.PopulateVariableValue(
                  &variable, &reference_value_, &eval_coordinator_mock_,
                  &generic_types, 1),
              CORDBG_E_BAD_REFERENCE_VALUE);
  }

  // reference_value should be dereferenced to object_value.
  EXPECT_CALL(reference_value_, Dereference(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(&object_value_), Return(S_OK)));

  {
    // Errors out if we cannot extract ICorDebugObjectValue.
    EXPECT_CALL(object_value_, QueryInterface(_, _))
        .Times(1)
        .WillRepeatedly(Return(E_NOINTERFACE));

    EXPECT_EQ(class_property_.PopulateVariableValue(
                  &variable, &reference_value_, &eval_coordinator_mock_,
                  &generic_types, 1),
              E_NOINTERFACE);
  }

  EXPECT_CALL(object_value_, QueryInterface(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(&object_value_), Return(S_OK)));

  {
    // Errors out if ICorDebugClass extraction fails.
    EXPECT_CALL(object_value_, GetClass(_))
        .Times(1)
        .WillRepeatedly(Return(CORDBG_E_PROCESS_TERMINATED));

    EXPECT_EQ(class_property_.PopulateVariableValue(
                  &variable, &reference_value_, &eval_coordinator_mock_,
                  &generic_types, 1),
              CORDBG_E_PROCESS_TERMINATED);
  }

  // From object_value, ICorDebugClass should be extracted.
  EXPECT_CALL(object_value_, GetClass(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(&debug_class_), Return(S_OK)));

  {
    // Errors out if ICorDebugModule extraction fails.
    EXPECT_CALL(debug_class_, GetModule(_))
        .Times(1)
        .WillRepeatedly(Return(CORDBG_E_MODULE_NOT_LOADED));

    EXPECT_EQ(class_property_.PopulateVariableValue(
                  &variable, &reference_value_, &eval_coordinator_mock_,
                  &generic_types, 1),
              CORDBG_E_MODULE_NOT_LOADED);
  }

  // ICorDebugModule extracted from ICorDebugClass.
  EXPECT_CALL(debug_class_, GetModule(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(&debug_module_), Return(S_OK)));

  {
    // Errors out if ICorDebugFunction extraction fails.
    EXPECT_CALL(debug_module_, GetFunctionFromToken(_, _))
        .Times(1)
        .WillRepeatedly(Return(CORPROF_E_FUNCTION_NOT_COMPILED));

    EXPECT_EQ(class_property_.PopulateVariableValue(
                  &variable, &reference_value_, &eval_coordinator_mock_,
                  &generic_types, 1),
              CORPROF_E_FUNCTION_NOT_COMPILED);
  }

  // ICorDebugFunction extracted from Module.
  EXPECT_CALL(debug_module_, GetFunctionFromToken(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(&debug_function_), Return(S_OK)));

  {
    // Errors out if ICorDebugEval not created.
    EXPECT_CALL(eval_coordinator_mock_, CreateEval(_))
        .Times(1)
        .WillRepeatedly(Return(CORDBG_E_FUNC_EVAL_BAD_START_POINT));

    EXPECT_EQ(class_property_.PopulateVariableValue(
                  &variable, &reference_value_, &eval_coordinator_mock_,
                  &generic_types, 1),
              CORDBG_E_FUNC_EVAL_BAD_START_POINT);
  }

  // ICorDebugEval created from eval_coordinator_mock.
  EXPECT_CALL(eval_coordinator_mock_, CreateEval(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(&debug_eval_), Return(S_OK)));

  {
    // Errors out if ICorDebugEval2 extraction fails.
    EXPECT_CALL(debug_eval_, QueryInterface(_, _))
        .Times(1)
        .WillRepeatedly(Return(E_NOINTERFACE));

    EXPECT_EQ(class_property_.PopulateVariableValue(
                  &variable, &reference_value_, &eval_coordinator_mock_,
                  &generic_types, 1),
              E_NOINTERFACE);
  }

  // ICorDebugEval2 extracted from ICorDebugEval.
  EXPECT_CALL(debug_eval_, QueryInterface(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(&debug_eval2_), Return(S_OK)));

  {
    // Errors out if CallParameterizedFunction fails.
    EXPECT_CALL(debug_eval2_, CallParameterizedFunction(_, _, _, _, _))
        .Times(1)
        .WillRepeatedly(Return(E_ABORT));

    EXPECT_EQ(class_property_.PopulateVariableValue(
                  &variable, &reference_value_, &eval_coordinator_mock_,
                  &generic_types, 1),
              E_ABORT);
  }

  EXPECT_CALL(debug_eval2_, CallParameterizedFunction(_, _, _, _, _))
      .WillRepeatedly(Return(S_OK));

  // Errors out if WaitForEval fails.
  EXPECT_CALL(eval_coordinator_mock_, WaitForEval(_, _, _))
      .Times(1)
      .WillRepeatedly(Return(CORDBG_E_FUNC_EVAL_NOT_COMPLETE));

  EXPECT_EQ(class_property_.PopulateVariableValue(&variable, &reference_value_,
                                                  &eval_coordinator_mock_,
                                                  &generic_types, 1),
            CORDBG_E_FUNC_EVAL_NOT_COMPLETE);
}

}  // namespace google_cloud_debugger_test