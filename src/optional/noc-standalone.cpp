
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

#ifndef _LPF_NOC_STANDALONE
 #pragma error "This file should be compiled with _LPF_NOC_STANDALONE defined"
#endif

#include "lpf/noc-standalone.h"

const lpf_t LPF_ROOT = nullptr;
const lpf_err_t LPF_SUCCESS = 0;
const lpf_err_t LPF_ERR_FATAL = 255;
const lpf_msg_attr_t LPF_MSG_DEFAULT = 0;
const lpf_sync_attr_t LPF_SYNC_DEFAULT = 0;

