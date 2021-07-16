
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
#include <lpf/collectives.h>
#include "Test.h"


void spmd( lpf_t ctx, lpf_pid_t s, lpf_pid_t p, lpf_args_t args)
{
    (void) args; // ignore any arguments passed through call to lpf_exec
    lpf_coll_t coll1, coll2, coll3, coll4;
    lpf_err_t rc;

    rc = lpf_resize_memory_register( ctx, 4 );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_sync( ctx, LPF_SYNC_DEFAULT );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_collectives_init( ctx, s, p, 1, 0, 0, &coll1 );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_collectives_init( ctx, s, p, 1, 8, 0, &coll2 );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_collectives_init( ctx, s, p, (1<<7), 0, (1<<19), &coll3 );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_collectives_init( ctx, s, p, (1<<4), 8, (1<<25), &coll4 );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_collectives_destroy( coll1 );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_collectives_destroy( coll2 );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_collectives_destroy( coll3 );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_collectives_destroy( coll4 );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
}

/** 
 * \test Initialises lpf_coll_t objects and deletes them.
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( func_lpf_collectives_init )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    return 0;
}

