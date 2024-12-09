
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

#ifndef LPFLIB_NOC_STANDALONE_H
#define LPFLIB_NOC_STANDALONE_H

/** \internal Warning: This is likely to be a temporary file, only used for
 *            standalone testing of the NOC extension for sockets first
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {

	uint32_t port;

} lpf_memslot_t;

typedef void * lpf_t;
typedef int lpf_err_t;
typedef unsigned int lpf_pid_t;
typedef int lpf_msg_attr_t;
typedef int lpf_sync_attr_t;

const lpf_t LPF_ROOT = nullptr;
const lpf_err_t LPF_SUCCESS = 0;
const lpf_err_t LPF_ERR_FATAL = 255;

#ifdef __cplusplus
}
#endif

#endif // LPFLIB_NOC_STANDALONE_H

