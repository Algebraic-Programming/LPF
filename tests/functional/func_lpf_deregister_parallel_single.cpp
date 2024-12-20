
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

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore any arguments passed through call to lpf_exec

    lpf_err_t rc = LPF_SUCCESS;
        
    size_t maxMsgs = 4 , maxRegs = 4;
    rc = lpf_resize_message_queue( lpf, maxMsgs);
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_resize_memory_register( lpf, maxRegs );
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    int x = 1, y = 2, z = 3;
    lpf_memslot_t xslot = LPF_INVALID_MEMSLOT;
    lpf_memslot_t yslot = LPF_INVALID_MEMSLOT;
    lpf_memslot_t zslot = LPF_INVALID_MEMSLOT;

    rc = lpf_register_local( lpf, &x, sizeof(x), &xslot );
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_register_local( lpf, &y, sizeof(y), &yslot );
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_register_global( lpf, &z, sizeof(z), &zslot );
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_deregister( lpf, xslot );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    if ( (pid | 0x1) < nprocs )
    {
        rc = lpf_put( lpf, yslot, 0, pid ^ 0x1, zslot, 0, sizeof(y), LPF_MSG_DEFAULT );
        EXPECT_EQ( LPF_SUCCESS, rc );
    }

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    EXPECT_EQ( (pid|0x1) < nprocs ? 2 : 3, z );

    lpf_deregister( lpf, yslot );
    lpf_deregister( lpf, zslot );
}

/** 
 * \test Allocate some registers, deregister a local register, and communicate
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( API, func_lpf_deregister_parallel_single )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc );
}
