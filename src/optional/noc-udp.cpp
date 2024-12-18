
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

#ifndef _LPF_NOC_STANDALONE
 #include <lpf/noc.h>
#else
 #include <lpf/noc-standalone.h>
#endif
#include "noc-udp-internal.hpp"

#include <assert.h>
#include <pthread.h>

#include <map>
#include <stdexcept>

/**
 * \internal Everything defined in this class should be internal to the engine's
 *           core context. This class (in the final implementation) hence will
 *           not exist. For this reason, this definition is purposefully \em not
 *           in noc-udp-internal.hpp.
 */
class GlobalNOCState {

	public:

		static uint32_t contextID;
		static std::map< lpf_t, NOCState > map;

};

uint32_t GlobalNOCState::contextID = 0;
std::map< lpf_t, NOCState > GlobalNOCState::map;
// end part that should move into the engine's core implementation

NOCState::NOCState() {
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

NOCState::NOCState( NOCState &&toMove ) : fd( toMove.fd ) {
	(void) printf( "NOCState with port %u is move-constructed\n", toMove.port );
	registers = std::move( toMove.registers );
	port = toMove.port;
	server = std::move( toMove.server );
	listener = std::move( toMove.listener );
	toMove.fd = 0;
	toMove.port = 0;
	(void) printf( "\t returning NOCState instance with port %u\n", port );
}

NOCState::~NOCState() {
	// Note the std::thread destructor will terminate the listener thread
	(void) printf( "NOCState at port %u is being destroyed\n", port );
}

NOCState& NOCState::operator=( NOCState &&toMove ) {
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

void NOCState::resize_registers( const size_t nregs ) {
	if( registers.size() < nregs ) {
		registers.resize( nregs );
	}
}

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

lpf_err_t lpf_noc_register(
	lpf_t ctx,
	void * const pointer, const size_t size,
	lpf_memslot_t * const memslot
) {
	(void) ctx; (void) size;
	(void) printf( "lpf_noc_register called, pointer %p and size: %zu\n",
		pointer, size );
	// TODO the below is presently not needed -- a NOC memslot is simply a pointer,
	//      and we do not do any error checking to quickly reach a POC state first.
	/*const auto registers_it = GlobalNOCState::map.find( ctx );
	if( registers_it == GlobalNOCState::map.cend() ) {
		throw std::runtime_error( "Could not find context - no preceding call to "
			"lpf_noc_resize_memory_register?\n" );
	}*/
	assert( memslot != NULL );
	*memslot = pointer;
	return LPF_SUCCESS;
}

lpf_err_t lpf_deregister( lpf_t ctx, lpf_memslot_t slot ) {
	(void) ctx; (void) slot;
	return LPF_SUCCESS;
}

lpf_err_t lpf_noc_put(
	lpf_t ctx,
	lpf_memslot_t src_slot, const size_t src_offset,
	const lpf_pid_t dst_pid, lpf_memslot_t dst_slot, const size_t dst_offset,
	const size_t size,
	lpf_msg_attr_t attr
) {
	(void) ctx; (void) src_slot; (void) src_offset;
	(void) dst_pid; (void) dst_slot; (void) dst_offset;
	(void) size; (void) attr;
	(void) fprintf( stderr, "lpf_noc_put not yet implemented\n" );
	return LPF_ERR_FATAL;
}

lpf_err_t lpf_noc_get(
	lpf_t ctx,
	lpf_pid_t src_pid, lpf_memslot_t src_slot, const size_t src_offset,
	lpf_memslot_t dst_slot, const size_t dst_offset,
	const size_t size, lpf_msg_attr_t attr
) {
	(void) attr;
	const auto registers_it = GlobalNOCState::map.find( ctx );
	if( registers_it == GlobalNOCState::map.cend() ) {
		throw std::runtime_error( "Could not find context - no preceding call to "
			"lpf_noc_resize_memory_register?\n" );
	}
	// TODO spawn receiver thread that opens a get-specific (or
	// memslot-specific) port. I think the latter is preferred, but not sure if
	// such a coarser handling of incoming messages could cause h-relations be
	// violated-- intuitively I don't think so, but requires a bit more thought.
	// The receiver thread will issue the request to the remote listener thread,
	// and hence this call can immediately return:
	return LPF_SUCCESS;
}

lpf_err_t lpf_noc_sync( lpf_t ctx, lpf_sync_attr_t attr ) {
	(void) attr;
	// TODO wait on all receiver threads
	// TODO wait on all outgoing messages
	return LPF_SUCCESS;
}

