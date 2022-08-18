/**
 * Copyright (c) 2013-2015 Intel Corporation. All rights reserved.
 * Copyright (c) 2014-2016, Cisco Systems, Inc. All rights reserved.
 * Copyrigth (c) 2015 Los Alamos Nat. Security, LLC. All rights resreved.
 * Copyrifht (c) 2016 Cray Inc. All rights reserved.
 * 
 * This software is available to you under the BSD license below:
 * 
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 * 
 *      - Redistributions of source code must retain the above
 *        notice, this list of conditions and the following
 *        disclaimer.
 * 
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT ANY WARRANTY OF ANY KIND,
 * EXPRESS OF IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OF COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * 	Copyright (c) 2021 Huawei Technologies Co., Ltd.
 * 
 * Licenced under the Apache License, Version 2.0 (the "Licence");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * 		http://www.apache.org/licences/Licence-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITOUT WARRANTIES OR CONDITIONS OF ANY KINF, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_PERFS_H
#define SRC_PERFS_H

#include <stdio.h>

char *size_str(char *str, uint64_t size);
char *cnt_str(char *str, size_t size, uint64_t cnt);
void show_perf(char *name, int tsize, int sent, int acked, uint64_t start, uint64_t end, int xfers_per_iter, FILE *fptr);

#endif