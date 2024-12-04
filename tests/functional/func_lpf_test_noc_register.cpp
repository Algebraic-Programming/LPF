
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
#include <lpf/noc.h>
#include <string.h>
#include "gtest/gtest.h"


void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args )
{
    (void) args;
    int x = 3, y = 6;
    lpf_memslot_t xSlot = LPF_INVALID_MEMSLOT;
    lpf_memslot_t ySlot = LPF_INVALID_MEMSLOT;

    printf("Hello\n");
    lpf_err_t rc = lpf_resize_memory_register( lpf, 3);
    EXPECT_EQ( LPF_SUCCESS, rc );
    
    rc = lpf_resize_message_queue( lpf, 1 );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_noc_register( lpf, &x, sizeof(x), &xSlot );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_noc_register( lpf, &y, sizeof(x), &ySlot );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_noc_put( lpf, ySlot, 0, (pid+1)%nprocs, xSlot, 0, sizeof(x), LPF_MSG_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_sync(lpf, LPF_SYNC_DEFAULT);
    EXPECT_EQ( LPF_SUCCESS, rc );
    // Every process should copy 
    EXPECT_EQ(x, 6);
}

/** 
 * \test Testing a lpf_put with an inactive destination memory slot
 * \pre P >= 2
 * \pre P <= 2
 * \return Message: destination memory slot was not yet active
 * \return Exit code: 0
 */
TEST( API, func_lpf_test_noc_register )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc );
}
