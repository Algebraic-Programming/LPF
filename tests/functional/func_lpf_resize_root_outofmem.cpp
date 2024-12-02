
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



/** 
 * \test Test lpf_resize function on LPF_ROOT when requested resources take too much memory
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( API, func_lpf_resize_root_outofmem )
{
    lpf_err_t rc = LPF_SUCCESS;
 
    size_t maxMsgs = ((size_t) -1)/10 ;
    size_t maxRegs = ((size_t) -1)/10;
    rc = lpf_resize_message_queue( LPF_ROOT, maxMsgs);
    EXPECT_EQ( LPF_ERR_OUT_OF_MEMORY, rc );
    rc = lpf_resize_memory_register( LPF_ROOT, maxRegs );
    EXPECT_EQ( LPF_ERR_OUT_OF_MEMORY, rc );

       

    maxMsgs = -1;
    maxRegs = -1;
    rc = lpf_resize_message_queue( LPF_ROOT, maxMsgs);
    EXPECT_EQ( LPF_ERR_OUT_OF_MEMORY, rc );
    rc = lpf_resize_memory_register( LPF_ROOT, maxRegs );
    EXPECT_EQ( LPF_ERR_OUT_OF_MEMORY, rc );

}
