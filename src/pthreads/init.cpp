
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

#include <string>
#include <cstdio>  //iostream segfaults when used in a constructor when
                   //preloaded, probably because the STL constructors have not
		   //executed yet. This means we have to use printf instead of
		   //IO streams or the log module, since the latter also uses
		   //IO streams.
#include <cstdlib> //for std::getenv


namespace lpf {

	static void __attribute__((constructor)) pthread_initializer( int argc, char ** argv ) {
		(void)argc;
		(void)argv;
		const char * const engine_c = std::getenv( "LPF_ENGINE" );
		const std::string engine = std::string(
				engine_c == NULL ?
				"<none set>" :
				engine_c
			);
		if( engine.compare( "<none set>" ) == 0 ) {
			(void) std::fprintf( stderr, "Warning: LPF_ENGINE was not set.\n" );
		} else {
			const bool selected_engine_requires_pthread_engine =
				(engine.compare( "pthread" ) == 0) ||
				(engine.compare( "hybrid" ) == 0);
			if( !selected_engine_requires_pthread_engine ) {
				(void) std::fprintf( stderr, "Warning: program was compiled for the pthread engine but run-time requests the %s engine instead. For stable results please compile the program into a universal LPF program (by omitting the -engine flag to the lpfcc/lpfcxx utilities).\n", engine.c_str() );
			}
		}
	}
}

