
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

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args )
{
    (void) nprocs; (void) args;
    int x[2] = {3, 4};
    int y[2] = {6, 7};
    lpf_memslot_t xSlot = LPF_INVALID_MEMSLOT;
    lpf_memslot_t ySlot = LPF_INVALID_MEMSLOT;

    lpf_err_t rc = lpf_resize_memory_register( lpf, 2 );
    EXPECT_EQ( LPF_SUCCESS, rc );
    
    rc = lpf_resize_message_queue( lpf, 1 );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_register_global( lpf, &x, sizeof(x), &xSlot );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_register_global( lpf, &y, sizeof(y), &ySlot );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    if (pid == 0) {
        rc = lpf_deregister( lpf, xSlot );
        EXPECT_EQ( LPF_SUCCESS, rc );
    }

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    FAIL();
}

/** 
 * \test A program with a lpf_deregister of a global slot that is not executed on all pids
 * \pre P >= 2
 * \return Message: Number of deregistrations of global slots does not match.
 * \return Exit code: 6
 */
TEST( API, func_lpf_debug_deregister_global_unequal )
{
    lpf_err_t rc = LPF_SUCCESS;
    rc = lpf_exec( LPF_ROOT, LPF_MAX_P, &spmd, LPF_NO_ARGS );
    EXPECT_EQ( LPF_SUCCESS, rc );
}
