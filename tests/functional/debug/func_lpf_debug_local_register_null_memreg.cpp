
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
    int x = 0;
    lpf_memslot_t xSlot = LPF_INVALID_MEMSLOT;
    lpf_register_local( lpf, &x, sizeof(x), &xSlot );
}

/** 
 * \test Register a memory region locally without allocating space
 * \pre P >= 1
 * \return Message: Invalid local memory registration, which would have taken the 1-th slot, while only space for 0 slots has been reserved
 * \return Exit code: 6
 */
TEST( API, func_lpf_debug_local_register_null_memreg )
{
    lpf_exec( LPF_ROOT, LPF_MAX_P, &spmd, LPF_NO_ARGS );
    FAIL();
}
