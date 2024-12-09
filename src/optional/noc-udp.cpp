
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

// \internal In a final implementation, parts of the below should be integrated
//           with the engine's core implementations, and the below include
//           enabled.
//#include <lpf/core.h>

#include <lpf/noc.h>

#include <assert.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <map>
#include <cstdio>
#include <thread>
#include <vector>
#include <stdexcept>

constexpr uint32_t start_port = 7000;

class NOCRegister {

	private:


	public:

		NOCRegister() {}

};

class NOCState;

/**
 * \internal Everything defined in this class should be internal to the engine's
 *           core. This class (in the final implementation) hence will not
 *           exist.
 */
class GlobalNOCState {

	public:

		static uint32_t contextID;
		static std::map< lpf_t, NOCState > map;

};

uint32_t GlobalNOCState::contextID = 0;
std::map< lpf_t, NOCState > GlobalNOCState::map;
// end part that should move into the engine's core implementation

class NOCState {

	private:

		/** The socket file descriptor. */
		int fd;

		/** A vector of NOC registers */
		std::vector< NOCRegister > registers;

		/** Port info */
		uint32_t port;

		/** Server info -- needed for passing into new registers. */
		struct sockaddr_in server;

		/** Handler to listener thread -- needed for termination on destruction */
		std::thread listener;

		/** Listener function. */
		static void listen(
			int fd, const uint32_t port,
			const std::vector< NOCRegister > &registers
		) {
			(void) printf( "Listener thread has started on port %u\n", port );
			char buffer[8192];
			while( true ) {
				const int nBytesRecv = recvfrom( fd, (char*)buffer, 8192, MSG_WAITALL,
					NULL, NULL );
				assert( nBytesRecv < 8192 );
				if( nBytesRecv < 0 ) {
					break;
				}
				buffer[ nBytesRecv ] = '\0';
				(void) printf( "Client sent: %s\n", buffer );
			}
			(void) printf( "Listener thread at port %u terminating. Register capacity at "
				"exit was %zd\n", port, registers.size() );
		}


	public:

		/** Base constructor. */
		NOCState() {
			(void) printf( "New NOCState being constructed- contextID = %u\n",
				GlobalNOCState::contextID );
			fd = socket( AF_INET, SOCK_DGRAM, 0 );
			if( fd < 0 ) {
				throw std::runtime_error( "Could not open a socket" );
			}
			port = start_port + GlobalNOCState::contextID;
			server.sin_family = AF_INET;
			server.sin_addr.s_addr = INADDR_ANY;
			server.sin_port = htonl( port );

			int rc = bind( fd, (const struct sockaddr*) &server, sizeof(server) );
			if( rc < 0 ) {
				throw std::runtime_error( "Could not bind socket to port " + port );
			}

			listener = std::thread( &NOCState::listen, fd, port, registers );
			listener.detach();

			// all OK, so increment ID
			(void) ++(GlobalNOCState::contextID);
			// TODO enact some maximum number of ports used by NOC RDMA
		}

		NOCState( NOCState &&toMove ) : fd( toMove.fd ) {
			(void) printf( "NOCState with port %u is move-constructed\n", toMove.port );
			registers = std::move( toMove.registers );
			port = toMove.port;
			server = std::move( toMove.server );
			listener = std::move( toMove.listener );
			toMove.fd = 0;
			toMove.port = 0;
			(void) printf( "\t returning NOCState instance with port %u\n", port );
		}

		~NOCState() {
			// Note the std::thread destructor will terminate the listener thread
			(void) printf( "NOCState at port %u is being destroyed\n", port );
		}

		NOCState& operator=( NOCState &&toMove ) {
			(void) printf( "NOCState is move-assigned\n" );
			fd = toMove.fd;
			registers = std::move( toMove.registers );
			port = toMove.port;
			server = std::move( toMove.server );
			listener = std::move( toMove.listener );
			toMove.fd = 0;
			toMove.port = 0;
			return *this;
		}

		void resize_registers( const size_t nregs ) {
			if( registers.size() < nregs ) {
				registers.resize( nregs );
			}
		}

};

lpf_err_t lpf_noc_resize_memory_register( lpf_t ctx, size_t max_regs ) {
	(void) printf( "lpf_noc_resize_memory_register called, "
		"requested capacity: %zd\n", max_regs );
	const auto registers_it = GlobalNOCState::map.find( ctx );
	if( registers_it == GlobalNOCState::map.cend() ) {
		// not found, inject one and then call again
		const size_t cursize = GlobalNOCState::map.size();
		try {
			GlobalNOCState::map.insert( std::make_pair( ctx, NOCState() ) );
		} catch(... ) {
			(void) fprintf( stderr, "Error during the creation of a new NOC state\n" );
			return LPF_ERR_FATAL;
		}
		assert( cursize + 1 == GlobalNOCState::map.size() );
		(void) printf( "New context %p was registered\n", ctx );
		return lpf_noc_resize_memory_register( ctx, max_regs );
	}

	// do the resize
	assert( registers_it != GlobalNOCState::map.cend() );
	registers_it->second.resize_registers( max_regs );
	return LPF_SUCCESS;
}

