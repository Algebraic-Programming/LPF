
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
    (void) pid; (void) nprocs; (void) args;
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

    EXPECT_DEATH(lpf_get( lpf, 0, xSlot, 0, ySlot, 0, sizeof(int), LPF_MSG_DEFAULT ), "LOL");

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ(LPF_SUCCESS, rc );

    EXPECT_EQ( 3, y[0] );
    EXPECT_EQ( 4, y[1] );
}

/** 
 * \test Testing for a lpf_get to itself, for which it needs an allocation of 2
 * \pre P >= 1
 * \pre P <= 1
 * \return Message: Too many messages on pid 0. Reserved was 1 while there were actually 2 in total
 * \return Exit code: 6
 */
TEST( API, func_lpf_debug_get_too_many_requests_self )
{
    lpf_err_t rc = LPF_SUCCESS;
    rc = lpf_exec( LPF_ROOT, LPF_MAX_P, &spmd, LPF_NO_ARGS );
    EXPECT_EQ( LPF_SUCCESS, rc );
}
