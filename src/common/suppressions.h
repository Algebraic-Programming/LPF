
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

#ifndef _H_LPFLIB_SUPPRESSIONS_H
#define _H_LPFLIB_SUPPRESSIONS_H

#if defined(__clang__)
    /*  CLang compiler also define __GNUC__ ; For that reason this branch
     *  should appear first*/

    #define LPFLIB_IGNORE_TAUTOLOGIES \
       _Pragma( "clang diagnostic push" ) ;\
       _Pragma( "clang diagnostic ignored \"-Wtype-limits\"" );\
       _Pragma( "clang diagnostic ignored \"-Wtautological-compare\""); \
       _Pragma( "clang diagnostic ignored \
                \"-Wtautological-constant-out-of-range-compare\"");

    #define LPFH_IGNORE_WEAK_VTABLES \
        _Pragma( "clang diagnostic push" ) \
        _Pragma( "clang diagnostic ignored \"-Wweak-vtables\"")

    #define LPFLIB_IGNORE_SHORTEN \
        _Pragma( "clang diagnostic push" );\
        _Pragma( "clang diagnostic ignored \"-Wshorten-64-to-32\"");

    #define LPFLIB_IGNORE_UNUSED_RESULT \
       _Pragma( "clang diagnostic push" );\
       _Pragma( "clang diagnostic ignored \"-Wunused-result\"" );

    #define LPFH_RESTORE_WARNINGS \
       _Pragma( "clang diagnostic pop")

    #define LPFLIB_RESTORE_WARNINGS \
       _Pragma( "clang diagnostic pop");

#elif defined(__GNUC__) && __GNUC__ >= 4
    #if __GNUC__ < 7
      #define LPFLIB_IGNORE_TAUTOLOGIES \
        _Pragma( "GCC diagnostic push" ) ;\
        _Pragma( "GCC diagnostic ignored \"-Wtype-limits\"" );\
        /*lint -save */\
        /*lint -e568  disable warning that unsigned integers are non-negative */\
        /*lint -e685  disable warning that relational operator is always true */\
        /*lint -e2650 -e845  disable warning about tautologie */
    #else
      #define LPFLIB_IGNORE_TAUTOLOGIES \
        _Pragma( "GCC diagnostic push" ) ;\
        _Pragma( "GCC diagnostic ignored \"-Wtype-limits\"" );\
        _Pragma( "GCC diagnostic ignored \"-Wtautological-compare\""); \
        /*lint -save */\
        /*lint -e568  disable warning that unsigned integers are non-negative */\
        /*lint -e685  disable warning that relational operator is always true */\
        /*lint -e2650 -e845  disable warning about tautologie */
    #endif

    // note: warning does not exist in GCC
    #define LPFH_IGNORE_WEAK_VTABLES \
       _Pragma( "GCC diagnostic push" );\

    // note: warning does not exist in GCC
    #define LPFLIB_IGNORE_SHORTEN \
       _Pragma( "GCC diagnostic push" );\

    #define LPFLIB_IGNORE_UNUSED_RESULT \
       _Pragma( "GCC diagnostic push" );\
       _Pragma( "GCC diagnostic ignored \"-Wunused-result\"" );

    #define LPFH_RESTORE_WARNINGS \
       _Pragma( "GCC diagnostic pop" )\

    #define LPFLIB_RESTORE_WARNINGS \
       _Pragma( "GCC diagnostic pop" );\
       /*lint -restore */

#else
    #define LPFLIB_IGNORE_TAUTOLOGIES
    #define LPFH_IGNORE_WEAK_VTABLES
    #define LPFLIB_IGNORE_SHORTEN
    #define LPFLIB_IGNORE_UNUSED_RESULT
    #define LPFLIB_RESTORE_WARNINGS
    #define LPFH_RESTORE_WARNINGS
#endif

#endif //end _H_LPFLIB_SUPPRESSIONS_H

