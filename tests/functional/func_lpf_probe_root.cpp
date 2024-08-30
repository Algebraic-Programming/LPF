
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

/** 
 * \test Test lpf_probe function on LPF_ROOT 
 * \note Extra lpfrun parameters: -probe 1.0
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( func_lpf_probe_root )
{
    lpf_err_t rc = LPF_SUCCESS;

    lpf_machine_t machine = LPF_INVALID_MACHINE;

    rc = lpf_probe( LPF_ROOT, &machine );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    EXPECT_LE( "%u", 1u, machine.p );
    EXPECT_LE( "%u", 1u, machine.free_p );
    EXPECT_LT( "%g", 0.0, (*(machine.g))(1, 0, LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(machine.l))(1, 0, LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(machine.g))(machine.p, 0, LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(machine.l))(machine.p, 0, LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(machine.g))(machine.p, (size_t)(-1), LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(machine.l))(machine.p, (size_t)(-1), LPF_SYNC_DEFAULT) );

    return 0;
}
