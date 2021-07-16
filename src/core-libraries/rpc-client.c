
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

#include "lpf/rpc-client.h"

#include <stdio.h>
#include <assert.hpp>
#include <stdlib.h>
#include <string.h>

const lpf_rpc_server_t LPF_RPC_SERVER_NONE = NULL;
const lpf_rpc_query_t LPF_RPC_QUERY_NONE   = { NULL, 0, NULL, 0, NULL, 0 };


// Sends a query to the server. Invisible to users.
static void sendQuery( lpf_t context, lpf_pid_t s, lpf_pid_t p, lpf_args_t args ) {
    //sanity checks
    ASSERT( s == 1 );
    ASSERT( p == 2 );
    ASSERT( args.output_size == sizeof(lpf_err_t) );
#ifdef NDEBUG
    (void)s;
    (void)p;
#endif

    //retrieve exit code field and set default
    lpf_err_t * exit_code = (lpf_err_t*) args.output;
    *exit_code = LPF_SUCCESS;

    //send query to daemon: declare new buffer
    lpf_err_t rc = lpf_resize_message_queue( context, 1 );
    if( rc != LPF_SUCCESS ) {
        fprintf( stderr, "Call to lpf_resize_message_queue resulted in non-SUCCESS error code %d\n", rc );
        *exit_code = rc;
        return;
    }

    rc = lpf_resize_memory_register( context, 2 );
    if( rc != LPF_SUCCESS ) {
        fprintf( stderr, "Call to lpf_resize_memory_register resulted in non-SUCCESS error code %d\n", rc );
        *exit_code = rc;
        return;
    }

    //activate buffer
    rc = lpf_sync( context, LPF_SYNC_DEFAULT );
    if( rc != LPF_SUCCESS ) {
        fprintf( stderr, "Error when activating new buffer size. Error code is %d\n", rc );
        *exit_code = rc;
        return;
    }

    //register slots for communication
    lpf_memslot_t slot = LPF_INVALID_MEMSLOT;

    //first register args.input_size
    rc = lpf_register_global( context, &(args.input_size), sizeof(size_t), &slot );
    if( rc != LPF_SUCCESS ) {
        fprintf( stderr, "Call to register input memory area size field resulted in non-SUCCESS error code %d\n", rc );
        *exit_code = rc;
        return;
    }

    //register input query area
    rc = lpf_register_global( context, (void*) (args.input), args.input_size, &slot );
    if( rc != LPF_SUCCESS ) {
        fprintf( stderr, "Call to register input memory area resulted in non-SUCCESS error code %d\n", rc );
        *exit_code = rc;
        return;
    }

    //activate registration
    rc = lpf_sync( context, LPF_SYNC_DEFAULT );
    if( rc != LPF_SUCCESS ) {
        (void) fprintf( stderr, "Could not activate input query registrations (code %d)\n", rc );
	*exit_code = rc;
	return;
    }

    //send input size
    rc = lpf_put( context, slot, 0, 0, slot, 0, sizeof(size_t), LPF_MSG_DEFAULT );
    if( rc != LPF_SUCCESS ) {
        fprintf( stderr, "Scheduling send of input query size failed with code %d\n", rc );
        *exit_code = rc;
        return;
    }

    //make sure input query size is sent
    rc = lpf_sync( context, LPF_SYNC_DEFAULT );
    if( rc != LPF_SUCCESS ) {
        fprintf( stderr, "Error while transmitting query. Error code is %d\n", rc );
        *exit_code = rc;
        return;
    }

    //deregister slot
    rc = lpf_deregister( context, slot );
    if( rc != LPF_SUCCESS ) {
        fprintf( stderr, "Call to deregister (1) failed with %d\n", rc );
        *exit_code = rc;
        return;
    }

    //send input query
    rc = lpf_put( context, slot, 0, 0, slot, 0, args.input_size, LPF_MSG_DEFAULT );
    if( rc != LPF_SUCCESS ) {
        fprintf( stderr, "Could not register the outgoing communication of the input query. Exit code: %d\n", rc );
        *exit_code = rc;
        return;
    }

    //make sure query is sent
    rc = lpf_sync( context, LPF_SYNC_DEFAULT );
    if( rc != LPF_SUCCESS ) {
        fprintf( stderr, "Error while transmitting query. Error code is %d\n", rc );
        *exit_code = rc;
        return;
    }

    //deregister slot
    rc = lpf_deregister( context, slot );
    if( rc != LPF_SUCCESS ) {
        fprintf( stderr, "Call to deregister (2) failed with %d\n", rc );
        *exit_code = rc;
        return;
    }

    //done
}

// Receives the answer from the server. Invisible to users.
static void receiveAnswer( lpf_t context, lpf_pid_t s, lpf_pid_t p, lpf_args_t args ) {
    //sanity checks
    ASSERT( s == 1 );
    ASSERT( p == 2 );
    ASSERT( args.output_size == sizeof(lpf_err_t) );
    ASSERT( args.output_size >= sizeof(lpf_err_t ) );
#ifdef NDEBUG
    (void)s;
    (void)p;
#endif

    //register slots for communication
    lpf_memslot_t slot = LPF_INVALID_MEMSLOT;

    //retrieve exit code field and set default
    void * output_code = ((char*)args.output) + args.output_size - sizeof(lpf_err_t);
    (void) memcpy(output_code, & LPF_SUCCESS, sizeof(lpf_err_t));

    //send query to daemon: declare new buffer
    lpf_err_t rc = lpf_resize_message_queue( context, 1 );
    if( rc != LPF_SUCCESS ) {
        (void) fprintf( stderr,
            "Call to lpf_resize_message_queue resulted in non-SUCCESS error code %d\n",
	    rc
	);
        (void) memcpy( output_code, & rc, sizeof(rc));
        return;
    }

    rc = lpf_resize_memory_register( context, 1 );
    if( rc != LPF_SUCCESS ) {
        (void) fprintf( stderr,
            "Call to lpf_resize_memory_register resulted in non-SUCCESS error code %d\n",
	    rc
	);
        (void) memcpy( output_code, & rc, sizeof(rc) );
        return;
    }

    //activate buffer
    rc = lpf_sync( context, LPF_SYNC_DEFAULT );
    if( rc != LPF_SUCCESS ) {
        (void) fprintf( stderr, "Error when activating new buffer size. Error code is %d\n", rc );
        (void) memcpy( output_code, & rc, sizeof(rc));
        return;
    }

    //register output area
    rc = lpf_register_global( context, args.output, args.output_size, &slot );
    if( rc != LPF_SUCCESS ) {
        (void) fprintf( stderr,
            "Call to register output memory area resulted in non-SUCCESS error code (LPF exit code is %d)\n",
	    rc
	);
        (void) memcpy( output_code, & rc, sizeof(rc));
        return;
    }

    //activate registration
    rc = lpf_sync( context, LPF_SYNC_DEFAULT );
    if( rc != LPF_SUCCESS ) {
        (void) fprintf( stderr, "Could not activate output memory area registration. Error code is %d\n",
	    rc
	);
	return;
    }

    //make sure result has been received
    rc = lpf_sync( context, LPF_SYNC_DEFAULT );
    if( rc != LPF_SUCCESS ) {
        (void) fprintf( stderr,
            "Error while receiving answer. Error code is %d\n",
	    rc
	);
        (void) memcpy( output_code, & rc, sizeof(rc));
        return;
    }

    //deregister slot
    rc = lpf_deregister( context, slot );
    if( rc != LPF_SUCCESS ) {
        (void) fprintf( stderr, "Call to deregister (3) failed with %d\n", rc );
        (void) memcpy( output_code, & rc, sizeof(rc));
        return;
    }

    //done
}

lpf_err_t lpf_rpc_query_create(
    const void * const payload,
    const size_t payload_size,
    const size_t response_size,
    lpf_rpc_query_t * const query
) {
    //try and allocate
    void * const tmp = malloc( response_size + sizeof(lpf_err_t));
    if( tmp == NULL ) {
        return LPF_ERR_OUT_OF_MEMORY;
    }

    //success, so set query contents
    query->input = payload;
    query->input_size = payload_size;
    query->output = tmp;
    query->output_size = response_size + sizeof(lpf_err_t);
    query->f_symbols = NULL;
    query->f_size = 0;
    return LPF_SUCCESS;
}

lpf_err_t lpf_rpc_query_destroy( lpf_rpc_query_t query ) {
    if( query.output != NULL ) {
        free( query.output );
    }
    query.input = query.output = NULL;
    query.input_size = query.output_size = 0;
    return LPF_SUCCESS;
}

lpf_err_t lpf_rpc_send_query( lpf_rpc_query_t query, lpf_rpc_server_t server, void * * const response ) {
    //sanity checks
    ASSERT( query.output_size >= sizeof(lpf_err_t) );

    //get handle to exit code
    lpf_err_t * const exit_code = (lpf_err_t*) (query.output + query.output_size - sizeof(lpf_err_t));

    //construct arguments
    const lpf_args_t args1 = { query.input, query.input_size, query.output + query.output_size - sizeof(lpf_err_t), sizeof(lpf_err_t), NULL, 0 };
    const lpf_args_t args2 = { NULL, 0, query.output, query.output_size, NULL, 0 };

    //send query
    lpf_err_t rc = lpf_hook( server, sendQuery, args1 );

    //retrieve result
    if( rc == LPF_SUCCESS ) {
        if( *exit_code == LPF_SUCCESS ) {
            rc = lpf_hook( server, receiveAnswer, args2 );
        }
    }

    //set output pointer
    if( rc == LPF_SUCCESS ) {
        if( *exit_code == LPF_SUCCESS ) {
            *response = query.output;
        }
    }

    //check return code
    if( rc == LPF_SUCCESS && *exit_code != LPF_SUCCESS ) {
        return *exit_code;
    }

    //done
    return rc;
}

