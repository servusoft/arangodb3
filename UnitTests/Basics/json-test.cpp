////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for json.c
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include "Basics/json.h"
#include "Basics/StringBuffer.h"
#include "Basics/Utf8Helper.h"

#if _WIN32
#include "Basics/win-utils.h"
#define FIX_ICU_ENV     TRI_FixIcuDataEnv()
#else
#define FIX_ICU_ENV
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------
  
#define INIT_BUFFER  TRI_string_buffer_t* sb = TRI_CreateStringBuffer(TRI_UNKNOWN_MEM_ZONE);
#define FREE_BUFFER  TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, sb);
#define STRINGIFY    TRI_StringifyJson(sb, json);
#define STRING_VALUE sb->_buffer
#define FREE_JSON    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CJsonSetup {
  CJsonSetup () {
    FIX_ICU_ENV;
    if (!arangodb::basics::Utf8Helper::DefaultUtf8Helper.setCollatorLanguage("")) {
      std::string msg =
        "cannot initialize ICU; please make sure ICU*dat is available; "
        "the variable ICU_DATA='";
      if (getenv("ICU_DATA") != nullptr) {
        msg += getenv("ICU_DATA");
      }
      msg += "' should point the directory containing the ICU*dat file.";
      BOOST_TEST_MESSAGE(msg);
      BOOST_CHECK_EQUAL(false, true);
    }
    BOOST_TEST_MESSAGE("setup json");
  }

  ~CJsonSetup () {
    BOOST_TEST_MESSAGE("tear-down json");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CJsonTest, CJsonSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test null value
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_null) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNullJson(TRI_UNKNOWN_MEM_ZONE);
  BOOST_CHECK_EQUAL(false, TRI_IsStringJson(json));

  STRINGIFY
  BOOST_CHECK_EQUAL("null", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test true value
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_true) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, true);
  BOOST_CHECK_EQUAL(false, TRI_IsStringJson(json));

  STRINGIFY
  BOOST_CHECK_EQUAL("true", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test false value
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_false) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, false);
  BOOST_CHECK_EQUAL(false, TRI_IsStringJson(json));

  STRINGIFY
  BOOST_CHECK_EQUAL("false", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test number value 0
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_number0) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 0.0);
  BOOST_CHECK_EQUAL(false, TRI_IsStringJson(json));

  STRINGIFY
  BOOST_CHECK_EQUAL("0", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test number value (positive)
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_number_positive1) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 1.0);
  BOOST_CHECK_EQUAL(false, TRI_IsStringJson(json));

  STRINGIFY
  BOOST_CHECK_EQUAL("1", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test number value (positive)
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_number_positive2) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 46281);
  BOOST_CHECK_EQUAL(false, TRI_IsStringJson(json));

  STRINGIFY
  BOOST_CHECK_EQUAL("46281", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test number value (negative)
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_number_negative1) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, -1.0);
  BOOST_CHECK_EQUAL(false, TRI_IsStringJson(json));

  STRINGIFY
  BOOST_CHECK_EQUAL("-1", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test number value (negative)
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_number_negative2) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, -2342);
  BOOST_CHECK_EQUAL(false, TRI_IsStringJson(json));

  STRINGIFY
  BOOST_CHECK_EQUAL("-2342", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value (empty)
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_string_empty) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "", strlen(""));
  BOOST_CHECK_EQUAL(true, TRI_IsStringJson(json));

  STRINGIFY
  BOOST_CHECK_EQUAL("\"\"", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_string1) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "the quick brown fox", strlen("the quick brown fox"));
  BOOST_CHECK_EQUAL(true, TRI_IsStringJson(json));

  STRINGIFY
  BOOST_CHECK_EQUAL("\"the quick brown fox\"", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_string2) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "The Quick Brown Fox", strlen("The Quick Brown Fox"));
  BOOST_CHECK_EQUAL(true, TRI_IsStringJson(json));

  STRINGIFY
  BOOST_CHECK_EQUAL("\"The Quick Brown Fox\"", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value (escaped)
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_string_escaped) {
  INIT_BUFFER
  
  char const* value = "\"the quick \"fox\" jumped over the \\brown\\ dog '\n\\\" \\' \\\\ lazy";

  TRI_json_t* json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, value, strlen(value));
  BOOST_CHECK_EQUAL(true, TRI_IsStringJson(json));

  STRINGIFY
  BOOST_CHECK_EQUAL("\"\\\"the quick \\\"fox\\\" jumped over the \\\\brown\\\\ dog '\\n\\\\\\\" \\\\' \\\\\\\\ lazy\"", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value (special chars)
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_string_utf8_1) {
  INIT_BUFFER

  char const* value = "코리아닷컴 메일알리미 서비스 중단안내 [안내] 개인정보취급방침 변경 안내 회사소개 | 광고안내 | 제휴안내 | 개인정보취급방침 | 청소년보호정책 | 스팸방지정책 | 사이버고객센터 | 약관안내 | 이메일 무단수집거부 | 서비스 전체보기";
  TRI_json_t* json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, value, strlen(value));
  BOOST_CHECK_EQUAL(true, TRI_IsStringJson(json));

  STRINGIFY
  BOOST_CHECK_EQUAL("\"코리아닷컴 메일알리미 서비스 중단안내 [안내] 개인정보취급방침 변경 안내 회사소개 | 광고안내 | 제휴안내 | 개인정보취급방침 | 청소년보호정책 | 스팸방지정책 | 사이버고객센터 | 약관안내 | 이메일 무단수집거부 | 서비스 전체보기\"", STRING_VALUE);
      
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value (special chars)
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_string_utf8_2) {
  INIT_BUFFER

  char const* value = "äöüßÄÖÜ€µ";
  TRI_json_t* json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, value, strlen(value));

  STRINGIFY
  BOOST_CHECK_EQUAL("\"äöüßÄÖÜ€µ\"", STRING_VALUE);
   
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value (unicode surrogate pair)
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_string_utf8_3) {
  INIT_BUFFER

  char const* value = "a𝛢";
  TRI_json_t* json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, value, strlen(value));

  STRINGIFY
  BOOST_CHECK_EQUAL("\"a𝛢\"", STRING_VALUE);
   
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty json list
////////////////////////////////////////////////////////////////////////////////
   
BOOST_AUTO_TEST_CASE (tst_json_list_empty) {
  INIT_BUFFER
          
  TRI_json_t* json = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
              
  STRINGIFY
  BOOST_CHECK_EQUAL("[]", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test json list mixed
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_list_mixed) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, TRI_CreateNullJson(TRI_UNKNOWN_MEM_ZONE));
  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, true));
  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, false));
  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, -8093));
  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 1.5));
  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, (char*) "the quick brown fox", strlen("the quick brown fox")));
  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE));
  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE));

  STRINGIFY
  BOOST_CHECK_EQUAL("[null,true,false,-8093,1.5,\"the quick brown fox\",[],{}]", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test json lists nested
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_list_nested) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_json_t* list1 = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_json_t* list2 = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_json_t* list3 = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_json_t* list4 = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, list1, TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, true));
  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, list1, TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, false));
  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, list2, TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, -8093));
  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, list2, TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 1.5));
  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, list3, TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, (char*) "the quick brown fox", strlen("the quick brown fox")));
  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, list1);
  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, list2);
  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, list3);
  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, list4);

  STRINGIFY
  BOOST_CHECK_EQUAL("[[true,false],[-8093,1.5],[\"the quick brown fox\"],[]]", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty json array
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_array_empty) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);

  STRINGIFY
  BOOST_CHECK_EQUAL("{}", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test json array mixed
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_array_mixed) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "one", TRI_CreateNullJson(TRI_UNKNOWN_MEM_ZONE));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "two", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, true));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "three", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, false));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "four", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, -8093));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "five", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 1.5));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "six", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, (char*) "the quick brown fox", strlen("the quick brown fox")));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "seven", TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "eight", TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE));

  STRINGIFY
  BOOST_CHECK_EQUAL("{\"one\":null,\"two\":true,\"three\":false,\"four\":-8093,\"five\":1.5,\"six\":\"the quick brown fox\",\"seven\":[],\"eight\":{}}", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test nested json array 
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_array_nested) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_json_t* array1 = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_json_t* array2 = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_json_t* array3 = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_json_t* array4 = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, array1, "one", TRI_CreateNullJson(TRI_UNKNOWN_MEM_ZONE));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, array1, "two", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, true));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, array1, "three", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, false));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, array2, "four", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, -8093));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, array2, "five", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 1.5));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, array2, "six", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, (char*) "the quick brown fox", strlen("the quick brown fox")));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, array3, "seven", TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, array3, "eight", TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "one", array1);
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "two", array2);
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "three", array3);
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "four", array4);

  STRINGIFY
  BOOST_CHECK_EQUAL("{\"one\":{\"one\":null,\"two\":true,\"three\":false},\"two\":{\"four\":-8093,\"five\":1.5,\"six\":\"the quick brown fox\"},\"three\":{\"seven\":[],\"eight\":{}},\"four\":{}}", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test json array keys
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_array_keys) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "\"quoted\"", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 1));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "'quoted'", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 2));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "\\slashed\\\"", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 3));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "white spaced", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 4));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "line\\nbreak", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 5));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 6));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, " ", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 7));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "null", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 8));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "true", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 9));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "false", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 10));

  STRINGIFY
  BOOST_CHECK_EQUAL("{\"\\\"quoted\\\"\":1,\"'quoted'\":2,\"\\\\slashed\\\\\\\"\":3,\"white spaced\":4,\"line\\\\nbreak\":5,\"\":6,\" \":7,\"null\":8,\"true\":9,\"false\":10}", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test utf8 json array keys
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_array_keys_utf8) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "äöüÄÖÜß", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 1));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "코리아닷컴", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 2));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "ジャパン", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 3));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "мадридского", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 4));

  STRINGIFY
  BOOST_CHECK_EQUAL("{\"äöüÄÖÜß\":1,\"코리아닷컴\":2,\"ジャパン\":3,\"мадридского\":4}", STRING_VALUE);

  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
