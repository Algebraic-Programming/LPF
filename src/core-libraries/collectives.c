
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

#include "lpf/collectives.h"
#include <assert.hpp>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/** Alignment to use when allocating #lpf_coll memory areas. */
#define _LPF_COLLECTIVES_ALIGN 64

/** The difference between pid and root, modulus P - circumvents weird modulus behaviour under -ve numbers */
#define DIFF(pid,root,P) ((pid < root) ? pid+P-root : pid-root) % P

const lpf_coll_t LPF_INVALID_COLL = { .ctx = NULL, .translate = NULL, .inv_translate = NULL, .buffer = NULL, .s = 0, .P = 0 };

lpf_t lpf_collectives_get_context( lpf_coll_t coll )
{
    return coll.ctx;
}

static lpf_err_t lpf_collectives_finish_init(
	lpf_pid_t p,
	size_t max_elem_size,
	size_t max_byte_size,
	lpf_coll_t * coll
) {
	const size_t base_size = (max_elem_size > max_byte_size) ? max_elem_size : max_byte_size;
	const size_t size = p * base_size;
	ASSERT( p > 0 );
	if( size / p != base_size ) {
		coll->buffer = NULL;
		coll->slot   = LPF_INVALID_MEMSLOT;
		return LPF_ERR_OUT_OF_MEMORY;
	}

	if( size == 0 ) {
		coll->buffer = NULL;
		coll->slot   = LPF_INVALID_MEMSLOT;
		return LPF_SUCCESS;
	}

	const int rc1 = posix_memalign( (void**)(&(coll->buffer)), _LPF_COLLECTIVES_ALIGN, size );
	if( rc1 != 0 ) {
		coll->buffer = NULL;
		coll->slot   = LPF_INVALID_MEMSLOT;
		return LPF_ERR_OUT_OF_MEMORY;
	}

	const lpf_err_t rc2 = lpf_register_global( coll->ctx, coll->buffer, size, &(coll->slot) );
	if( rc2 != LPF_SUCCESS ) {
		free( coll->buffer );
		coll->buffer = NULL;
		coll->slot = LPF_INVALID_MEMSLOT;
		return rc2;
	} else {
		return lpf_sync( coll->ctx, LPF_SYNC_DEFAULT );
	}
}

lpf_err_t lpf_collectives_init(
	lpf_t ctx,
	lpf_pid_t s,
	lpf_pid_t p,
	size_t max_calls,
	size_t max_elem_size,
	size_t max_byte_size,
	lpf_coll_t * coll
) {
	(void)max_calls;

	coll->inv_translate = coll->translate = NULL;

	coll->ctx = ctx;
	coll->P   = p;
	coll->s   = s;

	return lpf_collectives_finish_init( p, max_elem_size, max_byte_size, coll );
}

lpf_err_t lpf_collectives_init_strided(
	lpf_t ctx,
	lpf_pid_t s,
	lpf_pid_t p,
	lpf_pid_t lo,
	lpf_pid_t hi,
	lpf_pid_t str,
	size_t max_calls,
	size_t max_elem_size,
	size_t max_byte_size,
	lpf_coll_t * coll
) {
    (void)ctx;
	(void)max_calls;

	coll->P = 0;
	coll->s = LPF_MAX_P;

	lpf_pid_t new_PID = 0;
	for( lpf_pid_t k = lo; k < hi; k += str ) {
		if( k == s ) {
			coll->s = new_PID;
		}
		++(coll->P);
		++new_PID;
	}

	const int rc1 = posix_memalign( (void**)(&(coll->translate)), _LPF_COLLECTIVES_ALIGN, coll->P * sizeof(lpf_pid_t) );
	if( rc1 != 0 ) {
		coll->translate = NULL;
		coll->inv_translate = NULL;
		coll->buffer = NULL;
		coll->slot = LPF_INVALID_MEMSLOT;
		return LPF_ERR_OUT_OF_MEMORY;
	}

	const int rc2 = posix_memalign( (void**)(&(coll->inv_translate)), _LPF_COLLECTIVES_ALIGN, p * sizeof(lpf_pid_t) );
	if( rc2 != 0 ) {
		coll->translate = NULL;
		coll->inv_translate = NULL;
		coll->buffer = NULL;
		coll->slot = LPF_INVALID_MEMSLOT;
		return LPF_ERR_OUT_OF_MEMORY;
	}

	for( size_t i = 0; i < p; ++i ) {
		coll->translate[ i ] = LPF_MAX_P;
	}

	new_PID = 0;
	for( lpf_pid_t k = lo; k < hi; k += str ) {
		coll->translate[ new_PID ] = k;
		coll->translate[ k ] = new_PID;
		++new_PID;
	}

	return lpf_collectives_finish_init( p, max_elem_size, max_byte_size, coll );
}

lpf_err_t lpf_collectives_destroy( lpf_coll_t coll ) {
	if( coll.translate != NULL || coll.inv_translate != NULL ) {
		ASSERT( coll.    translate != NULL );
		ASSERT( coll.inv_translate != NULL );
		free( coll.    translate );
		free( coll.inv_translate );
	}

	if( coll.buffer == NULL ) {
		ASSERT( coll.slot == LPF_INVALID_MEMSLOT );
		return LPF_SUCCESS;
	}

	const lpf_err_t rc = lpf_deregister( coll.ctx, coll.slot );
	if( rc != LPF_SUCCESS ) {
		return rc;
	}

	free( coll.buffer );
	return LPF_SUCCESS;
}

lpf_err_t lpf_broadcast(
	lpf_coll_t coll,
	lpf_memslot_t src,
	lpf_memslot_t dst,
	size_t size,
	lpf_pid_t _root
) {
	ASSERT( coll.P > 0 );
	ASSERT( coll.s < coll.P );
	const size_t P = coll.P;
	const size_t me = coll.s;

	//catch trivial case
	if( size == 0 ) {
		return LPF_SUCCESS;
	}
	//self-copies are not allowed
	if( P == 1 ) {
		return LPF_SUCCESS;
	}

	const lpf_pid_t root = ( coll.inv_translate != NULL ) ? coll.inv_translate[ _root ] : _root;

	//gather machine params
	lpf_machine_t machine = LPF_INVALID_MACHINE;
	lpf_probe(coll.ctx, &machine);

	//decide on optimal broadcast
	double g = machine.g(P, size, LPF_SYNC_DEFAULT);
	double l = machine.l(P, size, LPF_SYNC_DEFAULT);

	// one superstep basic approach: pNg + l
	double basic_cost = (P * size * g) + l;
	// two supersteps using striping: 3Ng + 2l
	double striping_cost = (3 * size * g) + (2 * l);
	// two supersteps using sqrt(p) degree tree: 2sqrt(p)Ng + 2l
	double tree_cost = (2 * sqrt(P) * size * g) + (2 * l);

	// only choose the basic option if its the lowest cost
	if (basic_cost < striping_cost && basic_cost < tree_cost) {
		if( me != root ) {
			// read size bytes from root:src to my dst
			const lpf_err_t rc = lpf_get( coll.ctx, root, src, 0, dst, 0, size, LPF_MSG_DEFAULT );
			if( rc != LPF_SUCCESS ) {
				return rc;
			}
		}
	}
	// choose the tree option if the problem size is too small to stripe
	else if (size < P) {
		// the (max) interval between each core process
		size_t hop = (size_t) sqrt(P);
		// the offset from my core process
		size_t core_offset = DIFF(me,root,P) % hop;
		// my core process
		size_t core_home = DIFF(me,core_offset,P);
		// am i a core process
		bool is_core = (core_offset == 0);

		// step 1: the core processes read from root
		if( me != root  && is_core ) {
			// read size bytes from root:src to my dst
			const lpf_err_t rc = lpf_get( coll.ctx, root, src, 0, dst, 0, size, LPF_MSG_DEFAULT );
			if( rc != LPF_SUCCESS ) {
				return rc;
			}
		}
		lpf_sync(coll.ctx, LPF_SYNC_DEFAULT);

		// step 2: all non-core processes read from their designated core process
		if( me != root  && !is_core ) {
			// read size bytes from assigned:dst to my dst
			const lpf_err_t rc = lpf_get( coll.ctx, core_home, dst, 0, dst, 0, size, LPF_MSG_DEFAULT );
			if( rc != LPF_SUCCESS ) {
				return rc;
			}
		}
	}
	// striping option
	else {
		//share message equally apart from maybe the final process (global chunk size)
		size_t chunk = (size+P-1) / P;

		// step 1: read my chunk
		{
			//calculate offset and my chunk size
			size_t offset = chunk * me;
			size_t my_chunk = chunk;
			if( offset + my_chunk >= size ) {
				//two options: either I have the last bit
				if( size > offset ) {
					my_chunk = size - offset;
				} else {
					//or a process preceding me had the last bit
					my_chunk = 0;
				}
			}
			//if I am not the originating process or when src does not equal destination
			if( (me != root || src != dst) && my_chunk > 0 ) {
				// read my_chunk bytes from root:src+offset to my dst slot
				const lpf_err_t rc = lpf_get( coll.ctx, root, src, offset, dst, offset, my_chunk, LPF_MSG_DEFAULT );
				if( rc != LPF_SUCCESS ) {
					return rc;
				}
			}
		}

		//do communicate phase 1
		lpf_err_t brc = lpf_sync( coll.ctx, LPF_SYNC_DEFAULT );
		if( brc != LPF_SUCCESS ) {
			return brc;
		}

		// step 2: gather the chunks from across all processes
		if( me != root ) {
			for (size_t pid = 0; pid < P; pid++) {
				//skip getting my own contribution as I already have it
				if( pid == me ) {
					continue;
				}
				//calculate offset and target chunk size
				size_t offset = chunk * pid;
				size_t remote_chunk = chunk;
				if( offset + remote_chunk >= size ) {
					//two options: either target has the last bit
					if( size > offset ) {
						remote_chunk = size - offset;
					} else {
						//or a process preceding pid had the last bit
						remote_chunk = 0;
					}
				}
				// read my_chunk bytes from pid:dst slot to my dst+offset
                if ( remote_chunk > 0 ) {
                    brc = lpf_get( coll.ctx, pid, dst, offset, dst, offset, remote_chunk, LPF_MSG_DEFAULT );
                    if( brc != LPF_SUCCESS ) {
                        return brc;
                    }
                }
			}
		}
	}

	//done
	return LPF_SUCCESS;
}


lpf_err_t lpf_gather(
	lpf_coll_t coll,
	lpf_memslot_t src,
	lpf_memslot_t dst,
	size_t size,
	lpf_pid_t _root
) {
	ASSERT( coll.P > 0 );
	ASSERT( coll.s < coll.P );
	ASSERT( _root < coll.P );
	const lpf_pid_t root = (coll.inv_translate != NULL ) ? coll.inv_translate[ _root ] : _root;
	const size_t offset = coll.s * size;
	if( coll.s != root ) {
		const lpf_err_t rc = lpf_put( coll.ctx, src, 0, root, dst, offset, size, LPF_MSG_DEFAULT );
		if( rc != LPF_SUCCESS ) {
			return rc;
		}
	}
	return LPF_SUCCESS;
}


lpf_err_t lpf_scatter(
	lpf_coll_t coll,
	lpf_memslot_t src,
	lpf_memslot_t dst,
	size_t size,
	lpf_pid_t _root
) {
	ASSERT( coll.P > 0 );
	ASSERT( coll.s < coll.P );
	ASSERT( _root < coll.P );
	const lpf_pid_t root = (coll.inv_translate != NULL ) ? coll.inv_translate[ _root ] : _root;
	const size_t offset = coll.s * size;
	if( coll.s != root ) {
		const lpf_err_t rc = lpf_get( coll.ctx, root, src, offset, dst, 0, size, LPF_MSG_DEFAULT );
		if( rc != LPF_SUCCESS ) {
			return rc;
		}
	}
	return LPF_SUCCESS;
}

lpf_err_t lpf_allgather(
	lpf_coll_t coll,
	lpf_memslot_t src,
	lpf_memslot_t dst,
	size_t size, 
	bool exclude_myself
) {
	ASSERT( coll.P > 0 );
	ASSERT( coll.s < coll.P );

	const size_t offset = coll.s * size;
	for( lpf_pid_t _k = 0; _k < coll.P; ++_k ) {
		if( exclude_myself && _k == coll.s ) continue;
		const lpf_pid_t k = (coll.translate == NULL) ? _k : coll.translate[ _k ];
		const lpf_err_t rc = lpf_put( coll.ctx, src, 0, k, dst, offset, size, LPF_MSG_DEFAULT );
		if( rc != LPF_SUCCESS ) {
			return rc;
		}
	}
	return LPF_SUCCESS;
}

lpf_err_t lpf_alltoall(
	lpf_coll_t coll,
	lpf_memslot_t src,
	lpf_memslot_t dst,
	size_t size
) {
	ASSERT( coll.P > 0 );
	ASSERT( coll.s < coll.P );

	for( lpf_pid_t _k = 0; _k < coll.P; ++_k ) {
		if( _k == coll.s ) continue;
		const lpf_pid_t k = (coll.translate == NULL) ? _k : coll.translate[ _k ];
		const lpf_err_t rc = lpf_get( coll.ctx, k, src, coll.s * size, dst, k * size, size, LPF_MSG_DEFAULT );
		if( rc != LPF_SUCCESS ) {
			return rc;
		}
	}
	return LPF_SUCCESS;
}

lpf_err_t lpf_reduce(
	lpf_coll_t coll,
	void * restrict element,
	lpf_memslot_t element_slot,
	size_t size,
	lpf_reducer_t reducer,
	lpf_pid_t _root
) {
	ASSERT( coll.P > 0 );
	ASSERT( coll.s < coll.P );
	ASSERT( _root < coll.P );

	(void)element_slot;

	//handle trivial case first
	if( coll.P == 1 ) {
		return LPF_SUCCESS;
	}

	const lpf_pid_t root = (coll.inv_translate != NULL ) ? coll.inv_translate[ _root ] : _root;

	memcpy( coll.buffer, element, size );

	if( coll.s == root ) {
		memcpy( element, coll.buffer, size );
	}

	const lpf_err_t rc1 = lpf_gather( coll, coll.slot, coll.slot, size, _root );
	if( rc1 != LPF_SUCCESS ) {
		return rc1;
	}

	const lpf_err_t rc2 = lpf_sync( coll.ctx, LPF_SYNC_DEFAULT );
	if( rc2 != LPF_SUCCESS ) {
		return rc2;
	}

	if( coll.s == _root ) {
		//coll.buffer[ size * coll.s ] has not been communicated-- skip that gap
		//first, left of coll.s * size, reduce into element:
		(*reducer)( coll.s, coll.buffer, element );
		//then, right of (coll.s + 1) * size, reduce into element:
		(*reducer)( coll.P - coll.s - 1, coll.buffer + (coll.s + 1) * size, element );
	}

	return LPF_SUCCESS;
}

lpf_err_t lpf_allreduce(
	lpf_coll_t coll,
	void * restrict element,
	lpf_memslot_t element_slot,
	size_t size,
	lpf_reducer_t reducer
) {
	ASSERT( coll.P > 0 );
	ASSERT( coll.s < coll.P );

	//trivial case first
	if( coll.P == 1 ) {
		return LPF_SUCCESS;
	}

	const lpf_err_t rc1 = lpf_allgather( coll, element_slot, coll.slot, size, true );
	if( rc1 != LPF_SUCCESS ) {
		return rc1;
	}

	const lpf_err_t rc2 = lpf_sync( coll.ctx, LPF_SYNC_DEFAULT );
	if( rc2 != LPF_SUCCESS ) {
		return rc2;
	}

	//coll.buffer[ size * coll.s ] has not been communicated-- skip that gap
	//first, left of coll.s * size:
	(*reducer)( coll.s, coll.buffer, element );
	//then, right of (coll.s + 1) * size:
	(*reducer)( coll.P - coll.s - 1, coll.buffer + (coll.s + 1) * size, element );

	return LPF_SUCCESS;
}

lpf_err_t lpf_combine(
	lpf_coll_t coll,
	void * restrict array,
	lpf_memslot_t slot,
	size_t num,
	size_t size,
	lpf_combiner_t combiner,
	lpf_pid_t _root
) {
	ASSERT( coll.P > 0 );
	ASSERT( coll.s < coll.P );

	//handle trivial case first
	if( coll.P == 1 ) {
		return LPF_SUCCESS;
	}

	const lpf_pid_t root = (coll.inv_translate != NULL ) ? coll.inv_translate[ _root ] : _root;

	//gather machine params
	lpf_machine_t machine = LPF_INVALID_MACHINE;
	lpf_probe(coll.ctx, &machine);
	size_t P = coll.P;
	size_t me = coll.s;
	size_t N = size * num; // size on each process
	double g = machine.g(P, size, LPF_SYNC_DEFAULT);
	double l = machine.l(P, size, LPF_SYNC_DEFAULT);

	// one superstep basic approach: pNg + l
	// p small
	double basic_cost = (P * N * g) + l;
	// two supersteps using transpose and gather: 2Ng + 2l
	// p large, N >= p
	double transpose_cost = (2 * N * g) + (2 * l);
	// two supersteps using sqrt(p) degree tree: 2sqrt(p)Ng + 2l
	// p large, N < p
	double tree_cost = (2 * sqrt((unsigned int)P) * N * g) + (2 * l);

	// choose basic cost if its the lowest
	if (basic_cost < transpose_cost && basic_cost < tree_cost) {
		const lpf_err_t rc1 = lpf_gather( coll, slot, coll.slot, N, _root );
		if( rc1 != LPF_SUCCESS ) {
			return rc1;
		}
	
		const lpf_err_t rc2 = lpf_sync( coll.ctx, LPF_SYNC_DEFAULT );
		if( rc2 != LPF_SUCCESS ) {
			return rc2;
		}

		if( me == root ) {
			for( lpf_pid_t k = 0; k < P; ++k ) {
				if( k == me ) continue;
				(*combiner)( num, coll.buffer + k * N, array );
			}
		}
	}
	// choose tree if N is too small to transpose
	else if (num < P) {

		// the (max) interval between each core process
		size_t hop = (size_t) sqrt(P);
		// the offset from my core process
		size_t core_offset = DIFF(me,root,P) % hop;
		// my core process
		size_t core_home = DIFF(me,core_offset,P);
		// am i a core process
		bool is_core = (core_offset == 0);
		// number of processes in my core group
		size_t core_count = hop;
		while (core_count > 1) {
			size_t tmp_proc = me + (core_count-1);
			size_t tmp_core_offset = DIFF(tmp_proc,root,P) % hop;
			size_t tmp_core_home = DIFF(tmp_proc,tmp_core_offset,P);
			if (tmp_core_home == core_home) break;
			core_count--;
		}

                // step 1: all non-core processes write to their designated core process
                if( !is_core ) {
                        const lpf_err_t rc = lpf_put( coll.ctx, slot, 0, core_home, coll.slot, me*N, N, LPF_MSG_DEFAULT );
                        if( rc != LPF_SUCCESS ) {
                                return rc;
                        }
                }
		lpf_sync(coll.ctx, LPF_SYNC_DEFAULT);

		// step 2: all core processes combine their results into their array
		if ( is_core ) {
			for( size_t k = 1; k < core_count; ++k ) {
				(*combiner)( num, coll.buffer + ((me + k) % P) * N, array );
			}
		}
		// non-root processes will write their result to root
		if ( is_core && me != root ) {
                        const lpf_err_t rc = lpf_put( coll.ctx, slot, 0, root, coll.slot, me*N, N, LPF_MSG_DEFAULT );
			if( rc != LPF_SUCCESS ) {
				return rc;
			}
		}
		lpf_sync(coll.ctx, LPF_SYNC_DEFAULT);

		// step 3: root process combines its results from the core processes
		if ( me == root ) {
			for( size_t k = hop; k < P; k += hop ) {
				(*combiner)( num, coll.buffer + ((k + root) % P) * N, array );
			}
		}
	}
	// choose transpose and gather
	else {
		// share message equally apart from maybe the final process
		size_t chunk = (num+P-1) / P;

		// step 1: my_chunk*size bytes from each process to my collectives slot

		size_t offset = chunk * me;
		size_t my_chunk = (offset+chunk) <= num ? chunk : (num-offset);
		for (size_t pid = 0; pid < P; pid++) {
			const lpf_err_t rc = lpf_get( coll.ctx, pid, slot, offset*size, coll.slot, pid*my_chunk*size, my_chunk*size, LPF_MSG_DEFAULT );
			if( rc != LPF_SUCCESS ) {
				return rc;
			}
		}
		lpf_sync(coll.ctx, LPF_SYNC_DEFAULT);

		// step 2: combine the chunks and write to the root process
		for( size_t pid = 0; pid < P; pid++ ) {
			if (pid == me) continue;
			(*combiner)( my_chunk, coll.buffer + pid*my_chunk*size, array + offset*size );
		}
		if (me != root) {
			const lpf_err_t rc = lpf_put( coll.ctx, slot, offset*size, root, slot, offset*size, my_chunk*size, LPF_MSG_DEFAULT );
			if( rc != LPF_SUCCESS ) {
				return rc;
			}
		}
	}

	return LPF_SUCCESS;
}

lpf_err_t lpf_allcombine(
	lpf_coll_t coll,
	void * restrict array,
	lpf_memslot_t slot,
	size_t num,
	size_t size,
	lpf_combiner_t combiner
) {
	ASSERT( coll.P > 0 );
	ASSERT( coll.s < coll.P );

	//handle trivial case first
	if( coll.P == 1 ) {
		return LPF_SUCCESS;
	}

	//gather machine params
	lpf_machine_t machine = LPF_INVALID_MACHINE;
	lpf_probe(coll.ctx, &machine);
	size_t P = coll.P;
	size_t me = coll.s;
	size_t N = size * num; // size on each process
	double g = machine.g(P, size, LPF_SYNC_DEFAULT);
	double l = machine.l(P, size, LPF_SYNC_DEFAULT);

	// one superstep basic approach: pNg + l
	// p small
	double basic_cost = (P * N * g) + l;
	// two supersteps using transpose and gather: 2Ng + 2l
	// p large, N >= p
	double transpose_cost = (2 * N * g) + (2 * l);

	// choose basic cost if its the lowest
	if (basic_cost < transpose_cost) {
		const lpf_err_t rc1 = lpf_allgather( coll, slot, coll.slot, num * size, true );
		if( rc1 != LPF_SUCCESS ) {
			return rc1;
		}
	
		const lpf_err_t rc2 = lpf_sync( coll.ctx, LPF_SYNC_DEFAULT );
		if( rc2 != LPF_SUCCESS ) {
			return rc2;
		}

		for( lpf_pid_t k = 0; k < coll.P; ++k ) {
			if( k == coll.s ) continue;
			(*combiner)( num, coll.buffer + k * num * size, array );
		}
	}
	// choose transpose and allgather
	else {
		// share message equally apart from maybe the final process
		size_t chunk = (num+P-1) / P;

		// step 1: my_chunk*size bytes from each process to my collectives slot

		size_t offset = chunk * me;
		size_t my_chunk = (offset+chunk) <= num ? chunk : (num-offset);
		for (size_t pid = 0; pid < P; pid++) {
			const lpf_err_t rc = lpf_get( coll.ctx, pid, slot, offset*size, coll.slot, pid*my_chunk*size, my_chunk*size, LPF_MSG_DEFAULT );
			if( rc != LPF_SUCCESS ) {
				return rc;
			}
		}
		lpf_sync(coll.ctx, LPF_SYNC_DEFAULT);

		// step 2: combine the chunks and write to each process
		for( size_t pid = 0; pid < P; pid++ ) {
			if (pid == me) continue;
			(*combiner)( my_chunk, coll.buffer + pid*my_chunk*size, array + offset*size );
		}
		for( size_t pid = 0; pid < P; pid++ ) {
			if (pid == me) continue;
			const lpf_err_t rc = lpf_put( coll.ctx, slot, offset*size, pid, slot, offset*size, my_chunk*size, LPF_MSG_DEFAULT );
			if( rc != LPF_SUCCESS ) {
				return rc;
			}
		}
	}

	return LPF_SUCCESS;
}

