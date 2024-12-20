
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


#include <unistd.h>

#include <cstdio>

#include <lpf/noc-standalone.h>

int main() {
	lpf_err_t rc = lpf_noc_init( LPF_ROOT, 2 );
	if( rc != LPF_SUCCESS ) {
		(void) printf( "Error during call to lpf_noc_init\n" );
	}
	lpf_noc_resize_memory_register( LPF_ROOT, 1 );
	if( rc != LPF_SUCCESS ) {
		(void) printf( "Error during call to lpf_noc_resize_memory_register (I)\n" );
	}
	rc = lpf_noc_resize_memory_register( LPF_ROOT, 100 );
	if( rc != LPF_SUCCESS ) {
		(void) printf( "Error during call to lpf_noc_resize_memory_register (II)\n" );
	}
	rc = lpf_noc_resize_memory_register( LPF_ROOT, 10 );
	if( rc != LPF_SUCCESS ) {
		(void) printf( "Error during call to lpf_noc_resize_memory_register (III)\n"
			);
	}
	sleep( 10 );
	rc = lpf_noc_finalise( LPF_ROOT );
	if( rc != LPF_SUCCESS ) {
		(void) printf( "Error during call to lpf_noc_finalise\n" );
	}
	return 0;
}

