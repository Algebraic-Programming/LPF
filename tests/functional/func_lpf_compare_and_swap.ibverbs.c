
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
#include <stdint.h>
#include "Test.h"

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore args parameter
    lpf_err_t rc = LPF_SUCCESS;
        
    // local x is the compare-and-swap value and is important at non-root
    uint64_t localSwap = 0ULL; 
    // global y is the global slot at 0, and should be initialized to 0ULL
    uint64_t globalSwap = 0ULL; 
    int x = 0;
    int y = 0;
    lpf_memslot_t localSwapSlot = LPF_INVALID_MEMSLOT;
    lpf_memslot_t globalSwapSlot = LPF_INVALID_MEMSLOT;
    size_t maxmsgs = 2 , maxregs = 2;
    rc = lpf_resize_message_queue( lpf, maxmsgs);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_resize_memory_register( lpf, maxregs );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    lpf_memslot_t xslot = LPF_INVALID_MEMSLOT;
    lpf_memslot_t yslot = LPF_INVALID_MEMSLOT;
    rc = lpf_register_local( lpf, &localSwap, sizeof(localSwap), &localSwapSlot );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_register_local( lpf, &x, sizeof(x), &xslot );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_register_global( lpf, &globalSwap, sizeof(globalSwap), &globalSwapSlot );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_register_global( lpf, &y, sizeof(y), &yslot );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );


    // BLOCKING
    rc = lpf_lock_slot(lpf, localSwapSlot, 0, 0 /* rank where global slot to lock resides*/, globalSwapSlot, 0, sizeof(globalSwapSlot), LPF_MSG_DEFAULT);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_get( lpf, 0, yslot, 0, xslot, 0, sizeof(x), LPF_MSG_DEFAULT );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_sync_per_slot( lpf, LPF_SYNC_DEFAULT, xslot);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    x = x + 1;
    rc = lpf_put( lpf, xslot, 0, 0, yslot, 0, sizeof(x), LPF_MSG_DEFAULT );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_sync_per_slot( lpf, LPF_SYNC_DEFAULT, xslot);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    // BLOCKING
    lpf_unlock_slot(lpf, localSwapSlot, 0, 0 /* rank where global slot to lock resides*/, globalSwapSlot, 0, sizeof(globalSwapSlot), LPF_MSG_DEFAULT);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    lpf_sync(lpf, LPF_MSG_DEFAULT);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    if (pid == 0)
        printf("Rank %d: y = %d\n", pid, y);
}

/** 
 * \test Test atomic compare-and-swap on a global slot
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( func_lpf_compare_and_swap )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    return 0;
}
