
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
#include "gtest/gtest.h"

void test_spmd( const lpf_t lpf, const lpf_pid_t pid, const lpf_pid_t nprocs, const lpf_args_t args)
{
    // ignore all parameters
    (void) lpf;
    (void) pid;
    (void) nprocs;
    (void) args;

    /* empty */
}

extern lpf_spmd_t var_a;
static lpf_spmd_t var_b = NULL;
lpf_spmd_t var_c = & test_spmd;

/** \test Test whether the lpf_spmd_t typedef is defined appropriately
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( API, type_lpf_spmd_t )
{
    lpf_spmd_t var_d = NULL;
    lpf_spmd_t var_e = & test_spmd;

    lpf_t a = NULL;
    lpf_pid_t b = 0;
    lpf_pid_t c = 0;
    lpf_args_t e;
    (*var_e)(a, b, c, e);

    EXPECT_EQ( (lpf_spmd_t) NULL, var_b );
    EXPECT_EQ( &test_spmd, var_e );
    EXPECT_EQ( (lpf_spmd_t) NULL, var_d );
}
