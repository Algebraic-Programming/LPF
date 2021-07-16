
/*
 *   Copyright 2021 Huawei Technologies Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LPF_TESTS_FUNCTIONAL_TEST_H
#define LPF_TESTS_FUNCTIONAL_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assert.hpp"

#define EXPECT_EQ( format, expected, actual ) \
    do { LPFLIB_IGNORE_TAUTOLOGIES  \
        if ( (expected) != (actual) ) { \
        fprintf(stderr, "TEST FAILURE in " __FILE__ ":%d\n" \
                  "   Expected (%s) which evaluates to " format ", but\n" \
                  "   actual (%s) which evaluates to " format ".\n",  __LINE__, \
                  #expected, (expected), #actual, (actual) ); \
        abort(); } \
        LPFLIB_RESTORE_WARNINGS } while(0)

#define EXPECT_NE( format, expected, actual ) \
    do { LPFLIB_IGNORE_TAUTOLOGIES  \
        if ( (expected) == (actual) ) { \
        fprintf(stderr, "TEST FAILURE in " __FILE__ ":%d\n" \
                  "   Expected (%s) which evaluates to " format " to be different from\n" \
                  "   actual (%s) which evaluates to " format ".\n",  __LINE__, \
                  #expected, (expected), #actual, (actual) ); \
        abort(); } \
        LPFLIB_RESTORE_WARNINGS } while(0)

#define EXPECT_LE( format, expected, actual ) \
    do { LPFLIB_IGNORE_TAUTOLOGIES  \
        if ( !( (expected) <= (actual)) ) { \
        fprintf(stderr, "TEST FAILURE in " __FILE__ ":%d\n" \
                  "   Expected that (%s) which evaluates to " format ", is less than\n" \
                  "   or equal to (%s) which evaluates to " format ".\n",  __LINE__, \
                  #expected, (expected), #actual, (actual) ); \
        abort(); } \
        LPFLIB_RESTORE_WARNINGS } while(0)

#define EXPECT_GE( format, expected, actual ) \
    do { LPFLIB_IGNORE_TAUTOLOGIES  \
        if ( !( (expected) >= (actual) )) { \
        fprintf(stderr, "TEST FAILURE in " __FILE__ ":%d\n" \
                  "   Expected that (%s) which evaluates to " format ", is greater than\n" \
                  "   or equal to (%s) which evaluates to " format ".\n",  __LINE__, \
                  #expected, (expected), #actual, (actual) ); \
        abort(); } \
        LPFLIB_RESTORE_WARNINGS } while(0)

#define EXPECT_LT( format, expected, actual ) \
    do { LPFLIB_IGNORE_TAUTOLOGIES  \
        if ( !( (expected) < (actual)) ) { \
        fprintf(stderr, "TEST FAILURE in " __FILE__ ":%d\n" \
                  "   Expected that (%s) which evaluates to " format ", is less than\n" \
                  "   (%s) which evaluates to " format ".\n",  __LINE__, \
                  #expected, (expected), #actual, (actual) ); \
        abort(); } \
        LPFLIB_RESTORE_WARNINGS } while(0)

#define EXPECT_GT( format, expected, actual ) \
    do { LPFLIB_IGNORE_TAUTOLOGIES  \
        if ( !( (expected) > (actual) )) { \
        fprintf(stderr, "TEST FAILURE in " __FILE__ ":%d\n" \
                  "   Expected that (%s) which evaluates to " format ", is greater than\n" \
                  "   (%s) which evaluates to " format ".\n",  __LINE__, \
                  #expected, (expected), #actual, (actual) ); \
        abort(); } \
        LPFLIB_RESTORE_WARNINGS } while(0)

#define EXPECT_STREQ( N, expected, actual ) \
    do { LPFLIB_IGNORE_TAUTOLOGIES  \
        if ( strncmp( (expected), (actual), (N) ) != 0 ) { \
        fprintf(stderr, "TEST FAILURE in " __FILE__ ":%d\n" \
                  "   Expected (%s) which evaluates to %s, but\n" \
                  "   actual (%s) which evaluates to %s.\n",  __LINE__, \
                  #expected, (expected), #actual, (actual) ); \
        abort(); } \
        LPFLIB_RESTORE_WARNINGS } while(0)

#ifdef __GNUC__
    #define UNUSED __attribute__((unused))
#else
    #define UNUSED
#endif

/** \mainpage Test documentation
 *
 * This documentation lists the tests of the LPF implementation
 *  - \ref APITests
 *
 */

/** \defgroup APITests API Tests
  * A set of small programs to test the LPF API.
  */

#ifdef DOXYGEN
#define TEST( name )                  \
    /** \ingroup APITests        */  \
    int name( lpf_pid_t P )   

#else

#define TEST( name )                  \
    /** \ingroup APITests        */  \
    int name(int argc, char ** argv); \
                                      \
    int main(int argc, char ** argv)  \
    {                                 \
        (void) argc; (void) argv;     \
        return name (argc, argv);     \
    }                                 \
                                      \
    int name(int argc UNUSED, char ** argv UNUSED)  

#endif

#endif
