
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
#include <limits>
#include <sstream>
#include <iostream>

#include <lpf/noc-standalone.h>

int main( int argc, char ** const argv ) {
	if( argc < 3 || argc > 3 ) {
		std::cout << "Usage:  " << argv[ 0 ] << " <pid> <nprocs>\n";
		return 255;
	}
	lpf_pid_t s, p;
	s = p = LPF_MAX_P;
	{
		unsigned int s_parsed, p_parsed;
		{
			std::stringstream ss( argv[1] );
			ss >> s_parsed;
		}
		{
			std::stringstream ss( argv[2] );
			ss >> p_parsed;
		}
		if(
			static_cast< size_t >( s_parsed ) >
				std::numeric_limits< lpf_pid_t >::max()
		) {
			std::cerr << "Given PID " << argv[1] << " overflows\n";
			return 255;
		}
		if(
			static_cast< size_t >( p_parsed ) >
				std::numeric_limits< lpf_pid_t >::max()
		) {
			std::cerr << "Given #procs " << argv[2] << " overflows\n";
			return 255;
		}
		if( s_parsed >= p_parsed ) {
			std::cerr << "Invalid pair of PID ( " << argv[1] << " ) and #procs ( "
				<< argv[2] << " )\n";
			return 255;
		}
		s = static_cast< lpf_pid_t >( s_parsed );
		p = static_cast< lpf_pid_t >( p_parsed );
	}
	lpf_t ctx = LPF_ROOT;
	lpf_err_t rc = lpf_noc_init( s, p, &ctx );
	if( rc != LPF_SUCCESS ) {
		(void) printf( "Error during call to lpf_noc_init\n" );
	}
	lpf_noc_resize_memory_register( ctx, 1 );
	if( rc != LPF_SUCCESS ) {
		(void) printf( "Error during call to lpf_noc_resize_memory_register (I)\n" );
	}
	rc = lpf_noc_resize_memory_register( ctx, 100 );
	if( rc != LPF_SUCCESS ) {
		(void) printf( "Error during call to lpf_noc_resize_memory_register (II)\n" );
	}
	rc = lpf_noc_resize_memory_register( ctx, 10 );
	if( rc != LPF_SUCCESS ) {
		(void) printf( "Error during call to lpf_noc_resize_memory_register (III)\n"
			);
	}
	sleep( 10 );
	rc = lpf_noc_finalize( ctx );
	if( rc != LPF_SUCCESS ) {
		(void) printf( "Error during call to lpf_noc_finalise\n" );
	}
	return 0;
}

