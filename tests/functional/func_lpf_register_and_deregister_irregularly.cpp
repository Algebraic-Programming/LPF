
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
    (void) pid; 
    (void) nprocs;

    lpf_err_t rc = LPF_SUCCESS;
        
    size_t maxMsgs = 0 , maxRegs = 16;
    rc = lpf_resize_message_queue( lpf, maxMsgs);
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_resize_memory_register( lpf, maxRegs );
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    std::string buffer = "abcdefghijklmnop";
    lpf_memslot_t slots[16];

    // register 3 entries
    int i;
    for ( i = 0; i < 16; ++i)
    {
        rc = lpf_register_global( lpf, &buffer[i], sizeof(buffer[i]), &slots[i] );
        EXPECT_EQ( LPF_SUCCESS, rc );
    }

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    // deregister all but the last
    for ( i = 0; i < 15; ++i)
    {
        rc = lpf_deregister( lpf, slots[i] );
        EXPECT_EQ( LPF_SUCCESS, rc );
    }

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    // and resize to 2
    maxRegs = 2;
    rc = lpf_resize_memory_register( lpf, maxRegs );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    // and deregister the last one
    rc = lpf_deregister( lpf, slots[15] );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );
}

/** 
 * \test Register a few global registers and deregister them again in a funny way. This broke a previous memory registration implementation.
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( API, func_lpf_register_and_deregister_irregularly )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc );
}

