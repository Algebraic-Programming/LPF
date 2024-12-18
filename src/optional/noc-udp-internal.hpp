
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

// standard C++ mangling is OK -- these symbols to be used only internally

/**
 * @file
 *
 * This file contains configuration parameters for the UDP implementation of
 * the non-coherent RDMA extension to LPF. It also contains the headers that
 * engine that wish to include this optional extension.
 *
 * @author A. N. Yzelman
 * @date 18th of December, 2024
 */

#ifndef _H_NOC_UDP_INTERNAL
#define _H_NOC_UDP_INTERNAL

#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <cstdio>
#include <vector>
#include <thread>

/**
 * Each unique LPF context listens for get-requests on its own port. Ports are
 * assigned from this number onwards.
 */
constexpr uint32_t start_port = 7000;

/**
 * Which ports to listen for incoming "RDMA" requests.
 *
 * \note RDMA has quotes since this is not really RDMA -- both sender and
 *       receiver are active.
 */
constexpr uint32_t recv_start_port = 70000;

/**
 * Maximum number of supported contexts.
 *
 * \warning Enlarging this number requires in-depth code changes; presently,
 *          RDMA receive ports are numbered 7xyyy, where \a x is the context ID
 *          and yyy is the RDMA request. There hence may be 1000 RDMA requests
 *          active concurrently.
 */
constexpr uint32_t max_contexts = 10;

constexpr uint32_t max_incoming_rdma = 1000;

/**
 * Every get-request consists of a pointer and a requested number of bytes.
 */
constexpr size_t meta_size = sizeof( void * ) + sizeof( size_t );

/**
 * The chunk-size used by a single UDP datagram. This refers to payload data, in
 * bytes.
 */
constexpr size_t chunk_size = 65507;

/**
 * Internal storage corresponding to a locally registered memory area.
 */
class NOCRegister {

	private:


	public:

		/**
		 * Base constructor.
		 */
		NOCRegister() {}

};

/**
 * The state of UDP-based non-conflict RDMA that is to reside within each
 * separate LPF context.
 */
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
			char buffer[ meta_size + 1 ];
			while( true ) {
				struct sockaddr_in client;
				socklen_t client_len = sizeof(struct sockaddr_in);
				const ssize_t nBytesRecv = recvfrom( fd, (char*)buffer, meta_size, MSG_WAITALL,
					(struct sockaddr *) &client, &client_len );
				if( nBytesRecv < 0 ) {
					(void) fprintf( stderr, "Error while waiting for incoming request\n" );
					break;
				}
				assert( static_cast< size_t >(nBytesRecv) <= meta_size );
				buffer[ nBytesRecv ] = '\0';
				(void) printf( "Client sent: %s\n", buffer );
				if( nBytesRecv != meta_size ) {
					(void) fprintf( stderr, "Received message has unexpected size\n" );
					break;
				}
				assert( nBytesRecv == meta_size );

				// decode request
				void * buffer_p = &(buffer[0]);
				assert( reinterpret_cast< uintptr_t >( buffer_p ) % sizeof(int) == 0 );
				void * const ptr = reinterpret_cast< void * >( buffer_p );
				buffer_p = reinterpret_cast< char * >( buffer_p) + sizeof(void *);
				assert( reinterpret_cast< uintptr_t >( buffer_p ) % sizeof(int) == 0 );
				const size_t size = *reinterpret_cast< size_t * >( buffer_p );
				(void) printf( "Decoded request: send %zd bytes from %p onwards\n",
					size, ptr );

				// handle request:
				const size_t nchunks = size / chunk_size +
					(size % chunk_size > 0
						? 1
						: 0
					);
				//std::array< bool, batch_size > mask;
				//TODO launch ack-tracker thread
				bool send_error = false;
				for( size_t chunk = 0; chunk < nchunks; ++chunk ) {
					const size_t offset = chunk * chunk_size;
					const size_t size = offset > size
						? (size - offset)
						: chunk_size;
					void * const offset_ptr = reinterpret_cast< char * >(ptr) + offset;
					const ssize_t chksum = sendto( fd, offset_ptr, size, MSG_CONFIRM,
						(const struct sockaddr *) &client, client_len );
					if( chksum < 0 || static_cast< size_t >(chksum) != size ) {
						(void) fprintf( stderr, "Error during message send, chunk %zu/%zu; "
								"expected %zu bytes to be sent successfully but confirmed %zd only.\n",
							chunk, nchunks, size, chksum );
						send_error = true;
					}
					// TODO wait on ack-tracker
				}
				if( send_error ) {
					break;
				}
			}
			(void) printf( "Listener thread at port %u terminating. Register capacity at "
				"exit was %zd\n", port, registers.size() );
		}


	public:

		/** Base constructor. */
		NOCState();

		NOCState( NOCState &&toMove );

		~NOCState();

		NOCState& operator=( NOCState &&toMove );

		void resize_registers( const size_t nregs );

};


/**
 * The implementing engine's implementation of lpf_sync should call this
 * function.
 */
lpf_err_t lpf_noc_sync( lpf_t ctx, lpf_sync_attr_t attr );

#endif

