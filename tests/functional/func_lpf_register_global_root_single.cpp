
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

/** 
 * \test Test registering one global variable.
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( API, func_lpf_register_global_root_single )
{
    char a[1] = { 'j' };
    char b[2] = { 'a', 'b' };
    lpf_memslot_t aSlot = LPF_INVALID_MEMSLOT;
    lpf_memslot_t bSlot = LPF_INVALID_MEMSLOT;
    lpf_err_t rc = LPF_SUCCESS;

    rc = lpf_resize_message_queue( LPF_ROOT, 2);
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_resize_memory_register( LPF_ROOT, 2);
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_sync( LPF_ROOT, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_register_global( LPF_ROOT, &a, sizeof(a), &aSlot );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_register_local( LPF_ROOT, &b, sizeof(b), &bSlot );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync( LPF_ROOT, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    EXPECT_EQ( 'j', a[0]);
    EXPECT_EQ( 'a', b[0]);
    EXPECT_EQ( 'b', b[1]);

    rc = lpf_put( LPF_ROOT, bSlot, 1u * sizeof(b[0]), 
            0u, aSlot, 0u*sizeof(a[0]), sizeof(a[0]), LPF_MSG_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync( LPF_ROOT, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    EXPECT_EQ( 'b', a[0]);
    EXPECT_EQ( 'a', b[0]);
    EXPECT_EQ( 'b', b[1]);

}
