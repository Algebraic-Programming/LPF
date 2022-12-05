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
 * 	Copyright (c) 2022 Huawei Technologies Co., Ltd.
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

#include <stdio.h>
#include <stdlib.h>
#include "pingpong.h"
#include "perfs.h"

/*******************************************************************************
 *                                    Performance output
 ******************************************************************************/

/* str must be an allocated buffer of PP_STR_LEN bytes */
char *size_str(char *str, uint64_t size)
{
	uint64_t base, fraction = 0;
	char mag;

	memset(str, '\0', PP_STR_LEN);

	if (size >= (1 << 30)) {
		base = 1 << 30;
		mag = 'g';
	} else if (size >= (1 << 20)) {
		base = 1 << 20;
		mag = 'm';
	} else if (size >= (1 << 10)) {
		base = 1 << 10;
		mag = 'k';
	} else {
		base = 1;
		mag = '\0';
	}

	if (size / base < 10)
		fraction = (size % base) * 10 / base;

	if (fraction)
		snprintf(str, PP_STR_LEN, "%" PRIu64 ".%" PRIu64 "%c", size / base, fraction, mag);
	else
		snprintf(str, PP_STR_LEN, "%" PRIu64 "%c", size / base, mag);

	return str;
}

/* str must be an allocated buffer of PP_STR_LEN bytes */
char *cnt_str(char *str, size_t size, uint64_t cnt)
{
	// if (cnt >= 1000000000)
	// 	snprintf(str, size, "%" PRIu64 "b", cnt / 1000000000);
	// else if (cnt >= 1000000)
	// 	snprintf(str, size, "%" PRIu64 "m", cnt / 1000000);
	// else if (cnt >= 1000)
	// 	snprintf(str, size, "%" PRIu64 "k", cnt / 1000);
	// else
		snprintf(str, size, "%" PRIu64, cnt);

	return str;
}

void show_perf(char *name, int tsize, int sent, int acked, uint64_t start, uint64_t end, int xfers_per_iter, FILE *fptr)
{
	static int header = 1;
	char str[PP_STR_LEN];
	int64_t elapsed = end - start;
	uint64_t bytes = (uint64_t)sent * tsize * xfers_per_iter;
	float usec_per_xfer;

	if (sent == 0)
		return;

	if (name) {
		if (header) {
			fprintf(fptr, "%-50s%-8s%-8s%-9s%-8s%8s %10s%13s%13s\n",
			       "name", "bytes", "#sent", "#ack", "total",
			       "time", "MB/sec", "usec/xfer", "Kxfers/sec");
			header = 0;
		}

		fprintf(fptr, "%-50s", name);
	} else {
		if (header) {
			fprintf(fptr, "%-8s%-8s%-9s%-8s%8s %10s%13s%13s\n", "bytes",
			       "#sent", "#ack", "total", "time", "MB/sec",
			       "usec/xfer", "Kxfers/sec");
			header = 0;
		}
	}

	fprintf(fptr, "%-8s", size_str(str, tsize));
	fprintf(fptr, "%-8s", cnt_str(str, sizeof(str), sent));

	if (sent == acked)
		fprintf(fptr, "=%-8s", cnt_str(str, sizeof(str), acked));
	else if (sent < acked)
		fprintf(fptr, "-%-8s", cnt_str(str, sizeof(str), acked - sent));
	else
		fprintf(fptr, "+%-8s", cnt_str(str, sizeof(str), sent - acked));

	fprintf(fptr, "%-8s", size_str(str, bytes));

	usec_per_xfer = ((float)elapsed / sent / xfers_per_iter);
	fprintf(fptr, "%8.2fs%10.2f%11.2f%11.2f\n", elapsed / 1000000.0,
	       bytes / (1.0 * elapsed), usec_per_xfer, 1000.0 / usec_per_xfer);
}