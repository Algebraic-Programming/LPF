
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

#include <vector>
#include <cassert>
#include <cstring> //for strncpy

#include <cstdio> //iostream segfaults when used in a constructor when
                  //preloaded, probably because the STL constructors have not
		  //executed yet. This means we have to use printf instead of
		  //IO streams or the log module, since the latter also uses
		  //IO streams.

#include <cstdlib> //for abort

#include "interface.hpp"

extern "C" const int LPF_MPI_AUTO_INITIALIZE;


namespace lpf {

	bool mpi_initializer_ran = false;

	void __attribute__((constructor)) mpi_initializer() {
		const char * const engine_c = std::getenv( "LPF_ENGINE" );
		const std::string engine = std::string(
				engine_c == NULL ?
				"<none set>" :
				engine_c
			);
		if( engine.compare( "<none set>" ) == 0 ) {
			(void) std::fprintf( stderr, "Warning: LPF_ENGINE was not set.\n" );
		}
		//inspect LPF_ENGINE to decide if initialisation
		//should proceed. If no LPF_ENGINE was detected,
		//assume we should initialise anyway since we
		//were linked with this constructor
		const bool engine_is_MPI = (engine.compare( "<none set>" ) == 0) ||
			(engine.compare( "mpirma" ) == 0) ||
			(engine.compare( "mpimsg" ) == 0) ||
			(engine.compare( "ibverbs" ) == 0) ||
			(engine.compare( "hybrid" ) == 0);
		if( !engine_is_MPI ) {
			(void) std::fprintf( stderr, "Warning: program was compiled for the mpirma, mpimsg, ibverbs, or hybrid engine but run-time requests the %s engine instead. For stable results please compile the program into a universal LPF program (by omitting the -engine flag to the lpfcc/lpfcxx utilities).\n", engine.c_str() );
		}

		if( mpi_initializer_ran || !engine_is_MPI ) {
			return;
		}

		//ensure we run only once
		mpi_initializer_ran = true;

		//check if we need to initialise MPI
		if( LPF_MPI_AUTO_INITIALIZE ) {
			try {
				// the linker of recent Linux distributions does NOT initialize
				// argc/argv anymore during LD_PRELOAD (it was an undocumented feature);
				// hence, we cannot get implementation-specific MPI flags (which no major
				// MPI implementor anyway has), and we don't care about other flags;
				// therefore, ignore argc/argv tout court and pass NULL
				// (which is allowed by MPI_Init[_thread]())
				lpf::Interface::initRoot( NULL, NULL );
			}
			catch( std::exception &e ) {
				const std::string error = e.what();
				(void) std::fprintf( stderr, "Failed to auto-initialize engine:\n\t%s\n", error.c_str() );
				std::abort();
			}
		}
	}
}

