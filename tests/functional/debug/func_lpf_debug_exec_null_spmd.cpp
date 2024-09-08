
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
#include <string.h>
#include "gtest/gtest.h"


/** 
 * \test Test lpf_exec error of starting a NULL spmd
 * \pre P >= 1
 * \return Message: NULL spmd argument
 * \return Exit code: 6
 */
TEST( API, func_lpf_debug_exec_null_spmd )
{
    lpf_err_t rc = LPF_SUCCESS;
    EXPECT_EQ(lpf_exec( LPF_ROOT, LPF_MAX_P, NULL, LPF_NO_ARGS ), rc);
    FAIL();
}
