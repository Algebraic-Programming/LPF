
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

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore args parameter

    lpf_err_t rc = LPF_SUCCESS;
        
    size_t maxMsgs = 2 , maxRegs = 2;
    rc = lpf_resize_message_queue( lpf, maxMsgs);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_resize_memory_register( lpf, maxRegs );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
 
    int x = 5, y = 10;
    lpf_memslot_t xslot = LPF_INVALID_MEMSLOT;
    lpf_memslot_t yslot = LPF_INVALID_MEMSLOT;
    rc = lpf_register_global( lpf, &x, sizeof(x), &xslot );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_register_local( lpf, &y, sizeof(y), &yslot );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );


    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    EXPECT_EQ( "%d", 10, y);

    rc = lpf_get( lpf, (pid+1)%nprocs, xslot, 0, yslot, 0, sizeof(x), LPF_MSG_DEFAULT );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    EXPECT_EQ( "%d", 5, y);

    rc = lpf_deregister( lpf, xslot );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_deregister( lpf, yslot );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
}

/** 
 * \test Test lpf_get by sending a message following a ring.
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( func_lpf_get_parallel_single )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ("%d", LPF_SUCCESS, rc );
    return 0;
}
