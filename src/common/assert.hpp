
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


#ifndef LPFLIB_COMMON_ASSERT_HPP
#define LPFLIB_COMMON_ASSERT_HPP

#include "suppressions.h"

/* Definition of this project's specific assert function.
 * It improves by disabling compiler warnings about
 * tautologies and unused variables
 */

#ifdef NDEBUG

    /*lint -emacro(866, ASSERT) we use sizeof here to use variable 'x' */
    #define ASSERT( x ) /*lint -e{717} we use the do{} while(0) idom here*/\
                        do { /*lint -e438 do not flag unused variables*/\
                             LPFLIB_IGNORE_TAUTOLOGIES \
                             (void) sizeof(x);               \
                             LPFLIB_RESTORE_WARNINGS;   \
                             /*lint +e438 */\
                        } while(0)
#else
    #include "stack.hpp"
    #define ASSERT( x ) /*lint -e{717} we use the do{} while(0) idom here*/\
                        do { LPFLIB_IGNORE_TAUTOLOGIES \
                             if ( !(x) ) {                   \
                                 lpfAssertionFailure( __func__, __FILE__, #x, __LINE__);\
                             }                               \
                             LPFLIB_RESTORE_WARNINGS   \
                        } while(0)

#endif


#endif

