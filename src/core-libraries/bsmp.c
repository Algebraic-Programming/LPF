
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

#include "lpf/bsmp.h"

#include <assert.hpp>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define _LPF_BSMP_ALIGNMENT 8

//define the constant objects
const lpf_bsmp_t LPF_INVALID_BSMP = NULL;

const lpf_err_t LPF_ERR_BSMP_FULL = 777; //this number is rather arbitrary

const lpf_err_t LPF_ERR_BSMP_INVAL = 778; //this number is rather arbitrary

const size_t LPF_INVALID_SIZE = SIZE_MAX;
//end define the constant objects

//internal helper function
static void lpf_bsmp_free( const lpf_bsmp_t bsmp ) {
	//clear non-array fields
	bsmp->in_slot = bsmp->out_slot = LPF_INVALID_MEMSLOT;
	bsmp->queued = bsmp->s = bsmp->p = 0;
	bsmp->buffer_size = bsmp->tag_size = 0;
	bsmp->ctx = LPF_ROOT;

	//clear buffers
	if( bsmp->message_queue != NULL ) {
		free( bsmp->message_queue );
		bsmp->message_queue = NULL;
	}
	if( bsmp->remote_pos != NULL ) {
		free( bsmp->remote_pos );
		bsmp->remote_pos = NULL;
	}
	if( bsmp->local_pos != NULL ) {
		free( bsmp->local_pos );
		bsmp->local_pos = NULL;
	}
	if( bsmp->headers != NULL ) {
		free( bsmp->headers );
		bsmp->headers = NULL;
	}
}

//public functions:

lpf_err_t lpf_bsmp_create(
    const lpf_t ctx,
    const lpf_pid_t s,
    const lpf_pid_t p,
    const size_t user_buffer_size,
    const size_t tag_size,
    const size_t max_messages,
    lpf_bsmp_t * const bsmp_p
) {
	//allocate main data structure
	lpf_bsmp_t bsmp = (lpf_bsmp_t) malloc( sizeof(struct lpf_bsmp_buffer) );
	if( bsmp == NULL ) {
		*bsmp_p = LPF_INVALID_BSMP;
		return LPF_ERR_OUT_OF_MEMORY;
	}

	//determine required buffer sizes
	const size_t header_size = max_messages * sizeof(size_t);
	const size_t buffer_size = max_messages * (tag_size + sizeof(size_t)) + user_buffer_size;

	//set trivial fields
	bsmp->s           = s;
	bsmp->p           = p;
	bsmp->pid_pos     = 0;
	bsmp->buffer_size = buffer_size;
	bsmp->queued      = 0;
	bsmp->tag_size    = tag_size;
	bsmp->mode        = WRITE;
	bsmp->ctx         = ctx;

	//initialise buffers of size \f$ \Theta(p) \f$
	if( posix_memalign( (void**) &(bsmp->remote_pos), _LPF_BSMP_ALIGNMENT, p * sizeof(size_t) ) != 0 ) {
		bsmp->remote_pos = NULL;
		bsmp->local_pos = NULL;
		bsmp->message_queue = NULL;
		bsmp->headers = NULL;
		lpf_bsmp_free( bsmp );
		*bsmp_p = LPF_INVALID_BSMP;
		return LPF_ERR_OUT_OF_MEMORY;
	}
	if( posix_memalign( (void**) &(bsmp->local_pos), _LPF_BSMP_ALIGNMENT, p * sizeof(size_t) ) != 0 ) {
		bsmp->local_pos = NULL;
		bsmp->message_queue = NULL;
		bsmp->headers = NULL;
		lpf_bsmp_free( bsmp );
		*bsmp_p = LPF_INVALID_BSMP;
		return LPF_ERR_OUT_OF_MEMORY;
	}

	//initialise initial buffer positions
	for( lpf_pid_t k = 0; k < p; ++k ) {
		bsmp->local_pos[ k ] = 0;
		if( max_messages == 0 ) {
			//use maximum possible offset as indication no outgoing messages can be queued
			bsmp->remote_pos[ k ] = (size_t)-1;
		} else {
			bsmp->remote_pos[ k ] = 0;
		}
	}

	//initialise main payload + tag buffer
	if( buffer_size == 0 ) {
		//catch trivial case
		bsmp->message_queue = NULL;
	} else {
		//non-trivial case allocates aligned memory area
		if( posix_memalign( (void**) &(bsmp->message_queue), _LPF_BSMP_ALIGNMENT, p * buffer_size ) != 0 ) {
			bsmp->message_queue = NULL;
			bsmp->headers = NULL;
			lpf_bsmp_free( bsmp );
			*bsmp_p = LPF_INVALID_BSMP;
			return LPF_ERR_OUT_OF_MEMORY;
		}
	}

	//initialise header array
	if( max_messages == 0 ) {
		//catch trivial case
		bsmp->headers = NULL;
	} else {
		//non-trivial case allocates aligned memory area
		if( posix_memalign( (void**) &(bsmp->headers), _LPF_BSMP_ALIGNMENT, header_size ) != 0 ) {
			bsmp->headers = NULL;
			lpf_bsmp_free( bsmp );
			*bsmp_p = LPF_INVALID_BSMP;
			return LPF_ERR_OUT_OF_MEMORY;
		}
	}

	//register buffer areas, globally: payload+tag buffer, local position buffer.
	lpf_err_t rc = lpf_register_global( ctx, bsmp->message_queue, p * buffer_size, &(bsmp->in_slot) );
	if( rc == LPF_SUCCESS ) {
		rc = lpf_register_global( ctx, bsmp->local_pos, p * sizeof(size_t), &(bsmp->lpos_slot) );
	}

	//locally: remote position buffer, header buffer
	if( rc == LPF_SUCCESS ) {
		rc = lpf_register_local( ctx, bsmp->headers, header_size, &(bsmp->out_slot) );
	}
	if( rc == LPF_SUCCESS ) {
		rc = lpf_register_local( ctx, bsmp->remote_pos, p * sizeof(size_t), &(bsmp->rpos_slot) );
	}

	//finalise initialisation
	if( rc == LPF_SUCCESS ) {
		rc = lpf_sync( ctx, LPF_SYNC_DEFAULT );
	}

	//if something failed, go into error state and return failure
	if( rc != LPF_SUCCESS ) {
		*bsmp_p = LPF_INVALID_BSMP;
		ASSERT( bsmp != LPF_INVALID_BSMP );
		lpf_bsmp_free( bsmp );
		return rc;
	}

	//all OK, record output and return SUCCESS
	*bsmp_p = bsmp;
	return LPF_SUCCESS;
}

lpf_err_t lpf_bsmp_destroy( lpf_bsmp_t * const bsmp_p ) {
	//retrieve pointer to actual struct
	lpf_bsmp_t bsmp = *bsmp_p;

	//deregister memslots
	lpf_err_t rc = lpf_deregister( bsmp->ctx, bsmp->in_slot );
	ASSERT( rc == LPF_SUCCESS );
	rc = lpf_deregister( bsmp->ctx, bsmp->out_slot );
	ASSERT( rc == LPF_SUCCESS );
	rc = lpf_deregister( bsmp->ctx, bsmp->rpos_slot );
	ASSERT( rc == LPF_SUCCESS );
	rc = lpf_deregister( bsmp->ctx, bsmp->lpos_slot );
	ASSERT( rc == LPF_SUCCESS );

	//free memory
	lpf_bsmp_free( bsmp );
	free( bsmp );

	//set output according to specification & return
	*bsmp_p = LPF_INVALID_BSMP;
	return rc;
}

lpf_err_t lpf_send(
	const lpf_bsmp_t bsmp,
	const lpf_pid_t pid,
	const lpf_memslot_t tag,
	const size_t tag_offset,
	const lpf_memslot_t payload,
	const size_t payload_offset,
	const size_t payload_size
) {
	//catch UB
	ASSERT( bsmp->mode == WRITE );

	//error checking
	if( bsmp->remote_pos[ pid ] == LPF_INVALID_SIZE ) { //this is either set on construction or
                                                            //set at runtime after a failed LPF call 
		return LPF_ERR_BSMP_INVAL;
	}
	if( bsmp->remote_pos[ pid ] + sizeof(size_t) + bsmp->tag_size + payload_size > bsmp->buffer_size ) {
		return LPF_ERR_BSMP_FULL;
	}

	//catch UB
	ASSERT( payload != LPF_INVALID_MEMSLOT );
	ASSERT( tag != LPF_INVALID_MEMSLOT );

	//schedule payload transmission
	size_t bfr_offset = bsmp->s * bsmp->buffer_size + bsmp->remote_pos[ pid ];
	lpf_err_t rc = lpf_put(
		bsmp->ctx,
		payload, payload_offset,
		pid, bsmp->in_slot, bfr_offset,
		payload_size, LPF_MSG_DEFAULT
	);
	//update offset
	bfr_offset += payload_size;

	//send tag
	if( rc == LPF_SUCCESS ) {
		rc = lpf_put(
			bsmp->ctx,
			tag, tag_offset,
			pid, bsmp->in_slot, bfr_offset,
			bsmp->tag_size, LPF_MSG_DEFAULT
		);
	}
	//update offset
	bfr_offset += bsmp->tag_size;

	//send size info
	if( rc == LPF_SUCCESS ) {
		//save message size
		bsmp->headers[ bsmp->queued ] = payload_size;
		//do send out
		rc = lpf_put(
			bsmp->ctx,
			bsmp->out_slot, bsmp->queued,
			pid, bsmp->in_slot, bfr_offset,
			sizeof(size_t), LPF_MSG_DEFAULT
		);
	}

	//if any of the above was not successful
	if( rc != LPF_SUCCESS ) {
		//set BSMP buffer in error mode
		for( lpf_pid_t k = 0; k < bsmp->p; ++k ) {
			bsmp->remote_pos[ k ] = LPF_INVALID_SIZE;
		}
		//and then return the error
		return rc;
	}
	//go to next message
	++(bsmp->queued);
	bsmp->remote_pos[ pid ] += sizeof(size_t) + bsmp->tag_size + payload_size;
	//done
	return LPF_SUCCESS;
}

//the single-object version will just delegate to the multi-object version below
lpf_err_t lpf_bsmp_sync( lpf_bsmp_t bsmp, lpf_sync_attr_t hint ) {
	return lpf_bsmp_syncall( &bsmp, 1, hint );
}

lpf_err_t lpf_bsmp_syncall( lpf_bsmp_t * const bsmp, const size_t num_objects, lpf_sync_attr_t hint ) {
	//handle each BSMP object in serial
	for( size_t i = 0; i < num_objects; ++i ) {
		//catch UB
		ASSERT( bsmp[ i <= 0 ? 0 : i - 1 ]->ctx == bsmp[ i ]->ctx );

		//if this BSMP object is in write mode, close it and broadcast headers
		if( bsmp[ i ]->mode == WRITE ) {
			//reset outgoing queue
			bsmp[ i ]->queued = 0;
			//reset incoming queue and communicate final remote queue sizes
			bsmp[ i ]->pid_pos = 0;
			//broadcast remote_pos to indicate number of messages in remote queues
			for( lpf_pid_t k = 0; k < bsmp[ i ]->p; ++k ) {
				//reset error state, if any
				if( bsmp[ i ]->remote_pos[ k ] == LPF_INVALID_SIZE ) {
					bsmp[ i ]->remote_pos[ k ] = 0;
				} else if( bsmp[ i ]->remote_pos[ k ] > 0 ) {
					lpf_put(
						bsmp[ i ]->ctx,
						bsmp[ i ]->rpos_slot, k * sizeof(size_t),
						k, bsmp[ i ]->lpos_slot, bsmp[ i ]->s * sizeof(size_t),
						sizeof(size_t), LPF_MSG_DEFAULT
					);
				}
			}
		}
	}
	//delegate to real sync, execute all scheduled communications
	lpf_err_t rc = lpf_sync( bsmp[ 0 ]->ctx, hint );

	//handle each BSMP object in serial
	for( size_t i = 0; i < num_objects; ++i ) {
		//when done with the read mode
		if( bsmp[ i ]->mode == READ ) {
			//reset remote_pos positions for new calls to lpf_send
			for( lpf_pid_t k = 0; k < bsmp[ i ]->p; ++k ) {
				bsmp[ i ]->remote_pos[ k ] = 0;
			}
			//switch to write mode
			bsmp[ i ]->mode = WRITE;
		} else {
			//otherwise switch to READ mode
			bsmp[ i ]->mode = READ;
		}
	}

	//done
	return rc;
}

lpf_err_t lpf_move(
	lpf_bsmp_t bsmp,
	void * * const tag_p,
	void * * const payload_p,
	size_t * const size
) {
	//catch UB
	ASSERT( bsmp->mode == READ );

	//determine current process ID to receive messages from
	while( bsmp->pid_pos < bsmp->p && bsmp->local_pos[ bsmp->pid_pos ] == 0 ) {
		++(bsmp->pid_pos);
	}

	//check if queue is empty
	if( bsmp->pid_pos >= bsmp->p ) {
		*tag_p = *payload_p = NULL;
		*size = LPF_INVALID_SIZE;
		return LPF_SUCCESS;
	}

	//otherwise, read out next message; get base offset
	const size_t offset = bsmp->pid_pos * bsmp->buffer_size;

	//get size
	bsmp->local_pos[ bsmp->pid_pos ] -= sizeof(size_t);
	memcpy( size, bsmp->message_queue + offset + bsmp->local_pos[ bsmp->pid_pos ], sizeof(size_t));

	//get tag
	bsmp->local_pos[ bsmp->pid_pos ] -= bsmp->tag_size;
	*tag_p = bsmp->message_queue + offset + bsmp->local_pos[ bsmp->pid_pos ];

	//get payload
	bsmp->local_pos[ bsmp->pid_pos ] -= *size;
	*payload_p = bsmp->message_queue + offset + bsmp->local_pos[ bsmp->pid_pos ];

	//some sanity checks
	ASSERT( bsmp->pid_pos >= 0 );
	ASSERT( bsmp->pid_pos < bsmp->p );
	ASSERT( bsmp->local_pos[ bsmp->pid_pos ] >= 0 && bsmp->local_pos[ bsmp->pid_pos ] < LPF_INVALID_SIZE );

	//done
	return LPF_SUCCESS;
}

