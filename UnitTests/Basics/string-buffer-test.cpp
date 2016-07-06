////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for TRI_string_buffer_t
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
/// @author Tim Becker
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include "Basics/StringBuffer.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

#define STRLEN(a) (strnlen((a), 1024))

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

#define ABC_C "ABCDEFGHIJKLMNOP"
#define AEP_C "AEPDEFGHIJKLMNOP"
#define REP_C "REPDEFGHIJKLMNOP"
#define STR_C "The quick brown fox jumped over the laxy dog"

static char const* ABC_const = ABC_C;
static char const* AEP = AEP_C;
static char const* F_2_T = "56789A";
static char const* ONETWOTHREE = "123";
static char const* REP = REP_C;
static char const* STR = STR_C;
static char const* STRSTR = STR_C STR_C;
static char const* STRSTRABC_const = STR_C STR_C ABC_C;
static char const* TWNTYA = "aaaaaaaaaaaaaaaaaaaa";
static char const* Z_2_T = "0123456789A";

#define TRI_LastCharStringBuffer(s) *(TRI_EndStringBuffer(s) - 1)
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CStringBufferSetup {
  CStringBufferSetup () {
    BOOST_TEST_MESSAGE("setup TRI_string_buffer_t");
  }

  ~CStringBufferSetup () {
    BOOST_TEST_MESSAGE("tear-down TRI_string_buffer_t");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CStringBufferTest, CStringBufferSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_str_append
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_str_append) {
  size_t l1, l2;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb, TRI_CORE_MEM_ZONE);

  TRI_AppendStringStringBuffer(&sb, STR);
  TRI_AppendStringStringBuffer(&sb, STR);

  l1 = STRLEN(STRSTR);
  l2 = STRLEN(sb._buffer);
  
  BOOST_TEST_CHECKPOINT("basic append (len)");
  BOOST_CHECK_EQUAL(l1, l2);

  BOOST_TEST_CHECKPOINT("basic append (cmp)");
  BOOST_CHECK_EQUAL_COLLECTIONS(STRSTR, STRSTR + l1, sb._buffer, sb._buffer + l2);
  
  TRI_AppendString2StringBuffer(&sb, ABC_const, 3); // ABC_const ... Z

  l2 = STRLEN(sb._buffer);

  BOOST_TEST_CHECKPOINT("basic append 2 (cmp)");
  BOOST_CHECK_EQUAL_COLLECTIONS(STRSTRABC_const, STRSTRABC_const + l2, sb._buffer, sb._buffer + l2);

  TRI_ClearStringBuffer(&sb);
  TRI_AppendStringStringBuffer(&sb, STR);

  l2 = STRLEN(sb._buffer);

  BOOST_TEST_CHECKPOINT("basic append 3 (cmp)");
  BOOST_CHECK_EQUAL_COLLECTIONS(STRSTR, STRSTR + l2, sb._buffer, sb._buffer + l2);

  BOOST_TEST_CHECKPOINT("basic append 4 (cmp)");

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_char_append
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_char_append) {
  size_t l1, l2, i;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb, TRI_CORE_MEM_ZONE);
  
  for (i = 0;  i != 20;  ++i) {
    TRI_AppendCharStringBuffer(&sb, 'a');
  }

  l1 = STRLEN(TWNTYA);
  l2 = STRLEN(sb._buffer);
  
  BOOST_TEST_CHECKPOINT("char append (len)");
  BOOST_CHECK_EQUAL(l1, l2);

  BOOST_TEST_CHECKPOINT("char append (cmp)");
  BOOST_CHECK_EQUAL_COLLECTIONS(TWNTYA, TWNTYA + l1, sb._buffer, sb._buffer + l2);

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_swp
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_swp) {
  size_t l1, l2, i;

  TRI_string_buffer_t sb1, sb2;
  TRI_InitStringBuffer(&sb1, TRI_CORE_MEM_ZONE);
  TRI_InitStringBuffer(&sb2, TRI_CORE_MEM_ZONE);
  
  for (i = 0;  i != 20;  ++i) {
    TRI_AppendCharStringBuffer(&sb1, 'a');
  }

  TRI_AppendStringStringBuffer(&sb2, STR);

  TRI_SwapStringBuffer(&sb1, &sb2);

  l1 = STRLEN(TWNTYA);
  l2 = STRLEN(STR);
  
  BOOST_TEST_CHECKPOINT("swp test 1");
  BOOST_CHECK_EQUAL_COLLECTIONS(TWNTYA, TWNTYA + l1, sb2._buffer, sb2._buffer + l1);

  BOOST_TEST_CHECKPOINT("swp test 2");
  BOOST_CHECK_EQUAL_COLLECTIONS(STR, STR + l2, sb1._buffer, sb1._buffer + l2);

  TRI_DestroyStringBuffer(&sb1);
  TRI_DestroyStringBuffer(&sb2);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_begin_end_empty_clear
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_begin_end_empty_clear) {
  size_t l1;
  const char * ptr;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb, TRI_CORE_MEM_ZONE);
  
  TRI_AppendStringStringBuffer(&sb, STR);
  
  ptr = TRI_BeginStringBuffer(&sb);

  BOOST_TEST_CHECKPOINT("begin test");
  BOOST_CHECK_EQUAL((void*) sb._buffer, (void*) ptr);

  l1 = STRLEN(STR);
  ptr = TRI_EndStringBuffer(&sb);

  BOOST_TEST_CHECKPOINT("end test");
  BOOST_CHECK_EQUAL((void*)(sb._buffer + l1), (void*) ptr);

  BOOST_TEST_CHECKPOINT("empty 1");
  BOOST_CHECK(! TRI_EmptyStringBuffer(&sb));

  TRI_ClearStringBuffer(&sb);

  BOOST_TEST_CHECKPOINT("empty 2");
  BOOST_CHECK(TRI_EmptyStringBuffer(&sb));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_cpy
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_cpy) {
  size_t l1, i;

  TRI_string_buffer_t sb1, sb2;
  TRI_InitStringBuffer(&sb1, TRI_CORE_MEM_ZONE);
  TRI_InitStringBuffer(&sb2, TRI_CORE_MEM_ZONE);
  
  for (i = 0;  i != 20;  ++i) {
    TRI_AppendCharStringBuffer(&sb1, 'a');
  }

  TRI_AppendStringStringBuffer(&sb2, STR);
  TRI_CopyStringBuffer(&sb1, &sb2);

  l1 = STRLEN(STR);

  BOOST_TEST_CHECKPOINT("copy (len)"); 
  BOOST_CHECK_EQUAL(l1, STRLEN(sb1._buffer));

  BOOST_TEST_CHECKPOINT("cpy test 1");
  BOOST_CHECK_EQUAL_COLLECTIONS(STR, STR + l1, sb2._buffer, sb2._buffer + l1);

  BOOST_TEST_CHECKPOINT("cpy test 2");
  BOOST_CHECK_EQUAL_COLLECTIONS(STR, STR + l1, sb1._buffer, sb1._buffer + l1);

  TRI_DestroyStringBuffer(&sb1);
  TRI_DestroyStringBuffer(&sb2);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_erase_frnt
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_erase_frnt) {
  size_t l;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb, TRI_CORE_MEM_ZONE);
  TRI_AppendStringStringBuffer(&sb, Z_2_T);
  TRI_EraseFrontStringBuffer(&sb, 5);
  
  BOOST_CHECK_EQUAL(strlen(Z_2_T) - 5, TRI_LengthStringBuffer(&sb));

  l = STRLEN(sb._buffer);

  BOOST_TEST_CHECKPOINT("erase front");
  BOOST_CHECK_EQUAL_COLLECTIONS(F_2_T, F_2_T + l, sb._buffer, sb._buffer + l);

  TRI_EraseFrontStringBuffer(&sb, 15);
  BOOST_CHECK_EQUAL(0UL, TRI_LengthStringBuffer(&sb));
  
  BOOST_TEST_CHECKPOINT("erase front2");
  BOOST_CHECK(TRI_EmptyStringBuffer(&sb));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_erase_frnt
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_erase_frnt2) {
  size_t l;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb, TRI_CORE_MEM_ZONE);
  TRI_AppendStringStringBuffer(&sb, "abcdef");
  TRI_EraseFrontStringBuffer(&sb, 5);

  l = STRLEN(sb._buffer);

  BOOST_CHECK_EQUAL(1UL, l);
  BOOST_CHECK_EQUAL(1UL, TRI_LengthStringBuffer(&sb));
  BOOST_CHECK_EQUAL("f", sb._buffer);

  // clang 5.1 failes without the cast
  BOOST_CHECK_EQUAL((unsigned int) 'f', (unsigned int) sb._buffer[0]);
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[1]);
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[2]);
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[3]);
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[4]);
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[5]);
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[6]);

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_erase_frnt
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_erase_frnt3) {
  size_t l, i;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb, TRI_CORE_MEM_ZONE);
  for (i = 0;  i != 500;  ++i) {
    TRI_AppendCharStringBuffer(&sb, 'a');
  }
  TRI_EraseFrontStringBuffer(&sb, 1);

  l = STRLEN(sb._buffer);
  
  BOOST_CHECK_EQUAL(499UL, l);
  BOOST_CHECK_EQUAL(499UL, TRI_LengthStringBuffer(&sb));

  // clang 5.1 failes without the cast
  BOOST_CHECK_EQUAL((unsigned int) 'a', (unsigned int) sb._buffer[498]);
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[499]);
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[500]);
  
  TRI_EraseFrontStringBuffer(&sb, 1);

  l = STRLEN(sb._buffer);
  
  BOOST_CHECK_EQUAL(498UL, l);
  BOOST_CHECK_EQUAL(498UL, TRI_LengthStringBuffer(&sb));

  // clang 5.1 failes without the cast
  BOOST_CHECK_EQUAL((unsigned int) 'a', (unsigned int) sb._buffer[497]);
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[498]);
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[499]);
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[500]);
  
  TRI_EraseFrontStringBuffer(&sb, 1000);

  l = STRLEN(sb._buffer);
  
  BOOST_CHECK_EQUAL(0UL, l);
  BOOST_CHECK_EQUAL(0UL, TRI_LengthStringBuffer(&sb));

  // clang 5.1 failes without the cast
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[0]);
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[1]);
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[496]);
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[497]);
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[498]);
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[499]);
  BOOST_CHECK_EQUAL((unsigned int) '\0', (unsigned int) sb._buffer[500]);

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_replace
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_replace) {
  size_t l;

  TRI_string_buffer_t sb;
  TRI_string_buffer_t sb2;

  TRI_InitStringBuffer(&sb, TRI_CORE_MEM_ZONE);

  TRI_AppendStringStringBuffer(&sb, ABC_const);
  TRI_ReplaceStringStringBuffer(&sb, "REP", 3);
  
  l = STRLEN(sb._buffer);

  BOOST_TEST_CHECKPOINT("replace1");
  BOOST_CHECK_EQUAL_COLLECTIONS(REP, REP + l, sb._buffer, sb._buffer + l);

  TRI_ReplaceStringStringBuffer(&sb, ABC_const, 1);
  l = STRLEN(sb._buffer);

  BOOST_TEST_CHECKPOINT("replace2");
  BOOST_CHECK_EQUAL_COLLECTIONS(AEP, AEP + l, sb._buffer, sb._buffer + l);

  TRI_ClearStringBuffer(&sb);
  TRI_AppendStringStringBuffer(&sb, ABC_const);

  TRI_InitStringBuffer(&sb2, TRI_CORE_MEM_ZONE);
  TRI_AppendStringStringBuffer(&sb2, "REP");

  TRI_DestroyStringBuffer(&sb);
  TRI_DestroyStringBuffer(&sb2);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_smpl_utils
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_smpl_utils) {
  char const* a234 = "234";
  char const* a23412 = "23412";
  char const* a2341212125 = "23412-12.125";

  // these are built on prev. tested building blocks...
  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb, TRI_CORE_MEM_ZONE);
  TRI_AppendInteger3StringBuffer(&sb, 123);

  BOOST_TEST_CHECKPOINT("append int3");
  BOOST_CHECK_EQUAL_COLLECTIONS(ONETWOTHREE, ONETWOTHREE + STRLEN(ONETWOTHREE), sb._buffer, sb._buffer + STRLEN(sb._buffer));

  TRI_ClearStringBuffer(&sb);
  TRI_AppendInteger3StringBuffer(&sb, 1234);

  BOOST_TEST_CHECKPOINT("append int3");
  BOOST_CHECK_EQUAL_COLLECTIONS(a234, a234 + STRLEN(a234), sb._buffer, sb._buffer + STRLEN(sb._buffer));
  
  TRI_AppendDoubleStringBuffer(&sb, 12.0);

  BOOST_TEST_CHECKPOINT("append int3");
  BOOST_CHECK_EQUAL_COLLECTIONS(a23412, a23412 + STRLEN(a23412), sb._buffer, sb._buffer + STRLEN(sb._buffer));

  TRI_AppendDoubleStringBuffer(&sb, -12.125);

  BOOST_TEST_CHECKPOINT("append int3");
  BOOST_CHECK_EQUAL_COLLECTIONS(a2341212125, a2341212125 + STRLEN(a2341212125), sb._buffer, sb._buffer + STRLEN(sb._buffer));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_length
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_length) {
  TRI_string_buffer_t sb;

  TRI_InitStringBuffer(&sb, TRI_CORE_MEM_ZONE);

  BOOST_TEST_CHECKPOINT("length empty");
  BOOST_CHECK_EQUAL(0UL, TRI_LengthStringBuffer(&sb));

  TRI_AppendStringStringBuffer(&sb, ONETWOTHREE);

  BOOST_TEST_CHECKPOINT("length string");
  BOOST_CHECK_EQUAL(strlen(ONETWOTHREE), TRI_LengthStringBuffer(&sb));

  TRI_AppendInt32StringBuffer(&sb, 123);

  BOOST_TEST_CHECKPOINT("length integer");
  BOOST_CHECK_EQUAL(strlen(ONETWOTHREE) + 3, TRI_LengthStringBuffer(&sb));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_clear
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_clear) {
  TRI_string_buffer_t sb;

  TRI_InitStringBuffer(&sb, TRI_CORE_MEM_ZONE);
  BOOST_CHECK_EQUAL(0UL, TRI_LengthStringBuffer(&sb));

  // clear an empty buffer
  TRI_ClearStringBuffer(&sb);
  BOOST_CHECK_EQUAL(0UL, TRI_LengthStringBuffer(&sb));

  TRI_AppendStringStringBuffer(&sb, "foo bar baz");
  BOOST_CHECK_EQUAL(11UL, TRI_LengthStringBuffer(&sb));

  const char* ptr = TRI_BeginStringBuffer(&sb);
  TRI_ClearStringBuffer(&sb);
  BOOST_CHECK_EQUAL(0UL, TRI_LengthStringBuffer(&sb));

  // buffer should still point to ptr
  BOOST_CHECK_EQUAL((void*) ptr, (void*) TRI_BeginStringBuffer(&sb));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_steal
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_steal) {
  TRI_string_buffer_t sb;

  TRI_InitStringBuffer(&sb, TRI_CORE_MEM_ZONE);
  TRI_AppendStringStringBuffer(&sb, "foo bar baz");

  const char* ptr = TRI_BeginStringBuffer(&sb);
 
  // steal the buffer
  char* stolen = TRI_StealStringBuffer(&sb);
  
  // buffer is now empty
  BOOST_CHECK_EQUAL(0UL, TRI_LengthStringBuffer(&sb));
  BOOST_CHECK_EQUAL((void*) 0, (void*) TRI_BeginStringBuffer(&sb));

  // stolen should still point to ptr
  BOOST_CHECK_EQUAL((void*) stolen, (void*) ptr);
  BOOST_CHECK_EQUAL(0, strcmp(stolen, ptr));

  TRI_DestroyStringBuffer(&sb);

  // destroying the string buffer should not affect us
  BOOST_CHECK_EQUAL((void*) stolen, (void*) ptr);
  BOOST_CHECK_EQUAL(0, strcmp(stolen, ptr));

  // must manually free the string
  TRI_Free(TRI_CORE_MEM_ZONE, stolen);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_last_char
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_last_char) {
  TRI_string_buffer_t sb;

  TRI_InitStringBuffer(&sb, TRI_CORE_MEM_ZONE);

  TRI_AppendStringStringBuffer(&sb, "f");
  BOOST_CHECK_EQUAL((unsigned int) 'f', (unsigned int) TRI_LastCharStringBuffer(&sb));

  TRI_AppendCharStringBuffer(&sb, '1');
  BOOST_CHECK_EQUAL((unsigned int) '1', (unsigned int) TRI_LastCharStringBuffer(&sb));
  
  TRI_AppendCharStringBuffer(&sb, '\n');
  BOOST_CHECK_EQUAL((unsigned int) '\n', (unsigned int) TRI_LastCharStringBuffer(&sb));

  TRI_ClearStringBuffer(&sb);
  BOOST_CHECK_EQUAL(0UL, TRI_LengthStringBuffer(&sb));
  
  for (size_t i = 0; i < 100; ++i) {
    TRI_AppendStringStringBuffer(&sb, "the quick brown fox jumped over the lazy dog");
    BOOST_CHECK_EQUAL((unsigned int) 'g', (unsigned int) TRI_LastCharStringBuffer(&sb));
  }
  TRI_AppendCharStringBuffer(&sb, '.');
  BOOST_CHECK_EQUAL((unsigned int) '.', (unsigned int) TRI_LastCharStringBuffer(&sb));
  
  TRI_AnnihilateStringBuffer(&sb);

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_reserve
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_reserve) {
  TRI_string_buffer_t sb;

  TRI_InitStringBuffer(&sb, TRI_CORE_MEM_ZONE);
  BOOST_CHECK_EQUAL(0UL, TRI_LengthStringBuffer(&sb));
  
  TRI_ReserveStringBuffer(&sb, 0);
  BOOST_CHECK_EQUAL(0UL, TRI_LengthStringBuffer(&sb));

  TRI_ReserveStringBuffer(&sb, 1000);
  BOOST_CHECK_EQUAL(0UL, TRI_LengthStringBuffer(&sb));

  TRI_AppendStringStringBuffer(&sb, "f");
  BOOST_CHECK_EQUAL(1UL, TRI_LengthStringBuffer(&sb));

  for (size_t i = 0; i < 5000; ++i) {
    TRI_AppendCharStringBuffer(&sb, '.');
  }
  BOOST_CHECK_EQUAL(5001UL, TRI_LengthStringBuffer(&sb));
  
  TRI_ReserveStringBuffer(&sb, 1000);
  BOOST_CHECK_EQUAL(5001UL, TRI_LengthStringBuffer(&sb));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_timing
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_timing) {
  char buffer[1024];
  size_t const repeats = 100;

  size_t const loop1 =  25 * 10000;
  size_t const loop2 = 200 * 10000;

  TRI_string_buffer_t sb;
  size_t i;
  size_t j;

  double t1 = TRI_microtime();

  // integer
  for (j = 0;  j < repeats;  ++j) {
    TRI_InitStringBuffer(&sb, TRI_CORE_MEM_ZONE);

    for (i = 0;  i < loop1;  ++i) {
      TRI_AppendInt32StringBuffer(&sb, 12345678);
    }

    BOOST_TEST_CHECKPOINT("length integer");
    BOOST_CHECK_EQUAL(loop1 * 8, TRI_LengthStringBuffer(&sb));

    TRI_DestroyStringBuffer(&sb);
  }

  t1 = TRI_microtime() - t1;

  snprintf(buffer, sizeof(buffer), "time for integer append: %f msec", t1 * 1000);
  BOOST_TEST_MESSAGE(buffer);

  // character
  t1 = TRI_microtime();

  for (j = 0;  j < repeats;  ++j) {
    TRI_InitStringBuffer(&sb, TRI_CORE_MEM_ZONE);

    for (i = 0;  i < loop2;  ++i) {
      TRI_AppendCharStringBuffer(&sb, 'A');
    }

    BOOST_TEST_CHECKPOINT("length character");
    BOOST_CHECK_EQUAL(loop2, TRI_LengthStringBuffer(&sb));

    TRI_DestroyStringBuffer(&sb);
  }

  t1 = TRI_microtime() - t1;

  snprintf(buffer, sizeof(buffer), "time for character append: %f msec", t1 * 1000);
  BOOST_TEST_MESSAGE(buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_doubles
////////////////////////////////////////////////////////////////////////////////

// try to turn off compiler warning for deliberate division by zero
BOOST_AUTO_TEST_CASE (tst_doubles) {
  TRI_string_buffer_t sb;
  double value;
  
  TRI_InitStringBuffer(&sb, TRI_CORE_MEM_ZONE);

  // + inf
  value = HUGE_VAL;
  TRI_AppendDoubleStringBuffer(&sb, value);
  BOOST_CHECK_EQUAL("inf", sb._buffer);

  // - inf
  value = -HUGE_VAL;
  TRI_ClearStringBuffer(&sb);
  TRI_AppendDoubleStringBuffer(&sb, value);
  BOOST_CHECK_EQUAL("-inf", sb._buffer);
  
  value = INFINITY;

  TRI_ClearStringBuffer(&sb);
  TRI_AppendDoubleStringBuffer(&sb, value);
  BOOST_CHECK_EQUAL("inf", sb._buffer);
  
#ifdef NAN  
  // NaN
  value = NAN;
  TRI_ClearStringBuffer(&sb);
  TRI_AppendDoubleStringBuffer(&sb, value);
  BOOST_CHECK_EQUAL("NaN", sb._buffer);
#endif
  
  // big numbers, hopefully this is portable enough
  double n = 244536.0;
  value = n * n * n * n;
  TRI_ClearStringBuffer(&sb);
  TRI_AppendDoubleStringBuffer(&sb, value);
  BOOST_CHECK_EQUAL("3575783498001355400000", sb._buffer);

  value *= -1.0;
  TRI_ClearStringBuffer(&sb);
  TRI_AppendDoubleStringBuffer(&sb, value);
  BOOST_CHECK_EQUAL("-3575783498001355400000", sb._buffer);

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
