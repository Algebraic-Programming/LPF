
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
#include <lpf/bsplib.h>
#include "gtest/gtest.h"

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore any arguments passed through call to lpf_exec

    bsplib_err_t rc = BSPLIB_SUCCESS;
    
    bsplib_t bsplib;
    rc = bsplib_create( lpf, pid, nprocs, 1, (size_t) -1, &bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    if (pid==0) {
        rc = bsplib_sync( bsplib );
        EXPECT_EQ( BSPLIB_ERR_FATAL, rc );
    }

    rc = bsplib_destroy( bsplib);
    EXPECT_EQ( BSPLIB_ERR_FATAL, rc );
}

/** 
 * \test Test whether lpf_sync will detect sync mismatch when only pid 0 syncs
 * \pre P >= 2
 * \return Exit code: 0
 */
TEST( API, func_bsplib_sync_only_p0 )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS , rc );
}

