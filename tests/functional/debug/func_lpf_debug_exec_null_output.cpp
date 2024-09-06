
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

void spmd( lpf_t a, lpf_pid_t b, lpf_pid_t c, lpf_args_t d)
{
    (void) a; (void) b; (void) c; (void) d;
}


/** 
 * \test Test lpf_exec error of using NULL output with nonzero size
 * \pre P >= 1
 * \return Message: NULL output argument while output_size is non-zero
 * \return Exit code: 6
 */
TEST( API, func_lpf_debug_exec_null_output )
{
    lpf_args_t args;
    args.input = NULL;
    args.input_size = 0;
    args.output = NULL;
    args.output_size = 10;
    args.f_symbols = NULL;
    args.f_size = 0;
    EXPECT_DEATH(lpf_exec( LPF_ROOT, LPF_MAX_P, &spmd, args ), "NULL output argument");
}
