
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

#include "lpf/mpirpc-client.h"
#include "lpf/mpirpc-server.h"

#include <limits.h>


lpf_err_t lpf_mpirpc_open(
    const char * const hostname, const char * const portname,
    const int timeout,
    lpf_rpc_server_t * const server
) {
    //input argument checks
    if( hostname == NULL || portname == NULL ) {
        return LPF_SUCCESS;
    }
    if( hostname[ 0 ] == '\0' || portname[ 0 ] == '\0' || timeout == 0 ) {
        return LPF_SUCCESS;
    }

    //create init_t
    const lpf_err_t rc = lpf_mpi_initialize_over_tcp(
        hostname, portname,
        timeout, 1, 2,
        &server
    );

    //done
    return rc;
}

lpf_err_t lpf_mpirpc_close( const lpf_rpc_server_t server ) {
    return lpf_mpi_finalize( server );
}

lpf_err_t lpf_mpirpc_listen(
    const char * const portname,
    lpf_rpc_connection_t * const client
) {
    //input sanity check
    if( portname == NULL ) {
        return LPF_SUCCESS;
    }
    if( portname[0] == '\0' ) {
        return LPF_SUCCESS;
    }

    //wait for client connection
    lpf_err_t rc = lpf_mpi_initialize_over_tcp(
        "localhost",    //I shall become server
        portname,       //I listen to this port
        INT_MAX,        //will wait 30 seconds for a client connection
        0, 2,           //I will take PID 0 (out of 2) 
        &(client->init) //where to create the init struct
    );
    //on error (such as timeout), exit
    if( rc != LPF_SUCCESS ) {
        return rc;
    }

    //set standard values
    client->data = NULL;
    client->size = 0;

    //done
    return rc;
}

lpf_err_t lpf_mpirpc_conn_close( const lpf_rpc_connection_t client ) {
#ifdef NDEBUG
    if( client->size == 0 ) {
        assert( client->data == NULL );
    } else {
        assert( client->data != NULL );
    }
#endif

    //clean up
    if( client->size > 0 ) {
        free( client->data );
    }
    //done
    return lpf_mpi_finalize( client->init );
}

