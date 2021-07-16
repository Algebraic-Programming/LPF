
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

#include <lpf/core.h>
#include "Test.h"

#ifdef _LPF_VERSION
  #if _LPF_VERSION == 202000L
    // everything is OK
  #else
     #error Macro _LPF_VERSION has not been defined as 202000L
  #endif
#else
   #error Macro _LPF_VERSION has not been defined
#endif

/** 
 * \test Test the existence and value of the preprocessor macro _LPF_VERSION
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( macro_LPF_VERSION )
{
    EXPECT_EQ( "%ld", 202000L, _LPF_VERSION );
    return 0;
}
