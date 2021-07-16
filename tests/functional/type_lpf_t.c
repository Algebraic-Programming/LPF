
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


/** 
 * \test Test the existence of typedef lpf_t and its equivalence to (void *)
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( type_lpf_t )
{
    lpf_t var_d = NULL;

    int x = 5;
    void * y = & x;

    lpf_t var_e = y;
    y = var_d;

    EXPECT_EQ( "%ld", sizeof(lpf_t), sizeof(void *));
    EXPECT_EQ( "%p", NULL, y );
    EXPECT_EQ( "%p", (lpf_t) &x, var_e );
    return 0;
}
