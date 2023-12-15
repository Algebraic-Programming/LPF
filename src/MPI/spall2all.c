
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

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>

#include "spall2all.h"
#include "assert.hpp"


static int int_log2( int x )
{
    int y = 0;
    int incr = sizeof(int)*CHAR_BIT/2;
    do
    {
        int test = 1 << incr;
        ASSERT( incr > 0 );
        if (x >= test) {
            x >>= incr;
            y += incr;
        }
        incr /= 2;
        test /= 2;
    } while (incr > 0);
    return y;
}

struct spt_buf_header {
    size_t pos;
    int error;
} ;

struct spt_msg_header {
    int    pid, interm_pid;
    size_t size;
};


void sparse_all_to_all_create( sparse_all_to_all_t * obj, 
        int pid, int nprocs, uint64_t rng_seed, int vote_cats,
       size_t max_msg_size )
{
    obj->votes = NULL;
    obj->ballots = NULL;
    obj->rng = rng_create( rng_seed );
    rng_parallelize(&obj->rng, pid, nprocs);
    obj->vote_cats = vote_cats;
    if (max_msg_size <= sizeof(spt_buf_header_t))
        obj->max_msg_size = sizeof(spt_buf_header_t) + 1;
    else
        obj->max_msg_size = max_msg_size;
    int i;
    obj->heap = NULL;
    obj->buf_size = 0;
    for ( i =0; i < 4; ++i )
    {
        obj->buf_header[i] = NULL;
        obj->buf[i] = NULL;
    }
    obj->pid = pid; 
    obj->nprocs = nprocs;

    ASSERT( obj->nprocs > 0 );
    ASSERT( obj->pid >= 0);
    ASSERT( obj->pid < obj->nprocs );
    ASSERT( obj->max_msg_size <= (size_t) INT_MAX );
    ASSERT( obj->max_msg_size >= 1);
}

int sparse_all_to_all_reserve( sparse_all_to_all_t * obj, 
        size_t buf_size, size_t n_msgs )
{
    int error = 1;

    const unsigned byte_align = sizeof(spt_buf_header_t);
    const unsigned int_align = sizeof(int);
    ASSERT( byte_align >= int_align );
    ASSERT( byte_align % int_align == 0 );

    size_t SM = SIZE_MAX/4;
    if ( n_msgs <= SM / sizeof(spt_msg_header_t) 
            && buf_size <= SM - sizeof(spt_msg_header_t)*n_msgs
            && buf_size +  sizeof(spt_msg_header_t) * n_msgs 
                    <= SM - sizeof(spt_buf_header_t) - byte_align
       )  {

        buf_size = (buf_size + 
                    n_msgs*sizeof(spt_msg_header_t) +
                    sizeof(spt_buf_header_t) +
                    byte_align - 1)/ byte_align ;

        int vote_size = 
            ((1 + obj->vote_cats) * sizeof(int) + byte_align - 1)/byte_align;
        
        void * ptr   = calloc( byte_align, 4*buf_size + 2 * vote_size );

        if (ptr)
        {
            spt_buf_header_t * hdrmem = ptr;
            char * bytemem = ptr;
            int * votemem  = (int * ) ptr + 4*buf_size * byte_align / int_align;

            error = 0;
            sparse_all_to_all_t new_obj;

            new_obj.pid = obj->pid;
            new_obj.nprocs = obj->nprocs;
            new_obj.rng = obj->rng;
            new_obj.max_msg_size = obj->max_msg_size;
            new_obj.heap = bytemem;
            new_obj.buf_size = byte_align*buf_size;
            int i;
            for ( i =0; i < 4; ++i )
            {
                new_obj.buf_header[i] = hdrmem + i * buf_size;
                new_obj.buf[i] = bytemem + i * buf_size * byte_align;
                if (obj->buf_header[i] == NULL ) {
                    new_obj.buf_header[i]->pos = sizeof(spt_buf_header_t);
                    new_obj.buf_header[i]->error = 0;
                }
                else 
                {
                    size_t size = obj->buf_header[i]->pos;
                    if (size < new_obj.buf_size ) {
                        memcpy( new_obj.buf[i], obj->buf[i], size );
                    } else {
                        error = 1;
                        break;
                    }
                }
            }
            new_obj.vote_cats = obj->vote_cats;
            new_obj.votes = votemem;
            new_obj.ballots = votemem + 1 + obj->vote_cats ;

            if (error) {
                free(new_obj.heap);
            } else {
                free( obj->heap );
                memcpy( obj, &new_obj, sizeof(new_obj));
            }
        }
    }
    ASSERT( obj->nprocs > 0 );
    ASSERT( obj->pid >= 0);
    ASSERT( obj->pid < obj->nprocs );

    return error;
}

void sparse_all_to_all_destroy( sparse_all_to_all_t * obj )
{
    int i;
    for ( i =0; i < 4; ++i )
    {
        obj->buf_header[i] = NULL;
        obj->buf[i] = NULL;
    }
    free(obj->heap);
    obj->heap = NULL;
    obj->buf_size = 0;
    obj->vote_cats = 0;
    obj->votes = NULL;
    obj->ballots = NULL;
    obj->pid = -1;
    obj->nprocs = -1;
}

static int sparse_all_to_all_push( sparse_all_to_all_t * obj, int n,
        int dest_pid, const char * msg, size_t size)
{
    ASSERT( obj->pid >= 0);
    ASSERT( obj->pid < obj->nprocs );
    ASSERT( obj->nprocs > 0 );
    ASSERT( dest_pid >= 0); 
    ASSERT( dest_pid < obj->nprocs );
    int error = 1;
    int rnd_interm_pid = 
        (int) (rng_next(&obj->rng) / (1.0 + UINT64_MAX) * obj->nprocs);
    spt_buf_header_t * bufhdr = obj->buf_header[n];
    size_t bufsize = obj->buf_size;
    size_t bufhdrpos = bufhdr != NULL ? bufhdr->pos : bufsize;
    if ( bufsize - bufhdrpos >= size + sizeof(dest_pid) + sizeof(size) )
    {
        ASSERT( bufhdr != NULL );
        ASSERT( bufhdrpos <= bufsize );
        char * buf = obj->buf[n] ;
        memcpy( buf + bufhdrpos, msg, size);
        bufhdrpos += size;
        spt_msg_header_t header; 
        header.pid = dest_pid; 
        header.interm_pid = rnd_interm_pid;
        header.size = size;
        memcpy( buf + bufhdrpos, &header, sizeof(header));
        bufhdrpos += sizeof(header);
        bufhdr->pos = bufhdrpos;
        error = 0;
    }
    return error ;
}

int sparse_all_to_all_send( sparse_all_to_all_t * obj, 
        int dest_pid, const char * msg, size_t size )
{
    return sparse_all_to_all_push(obj, 0, dest_pid, msg, size);
}

static int sparse_all_to_all_pop( sparse_all_to_all_t * obj, int n, 
        char * msg, size_t * size, int * pid, int * interm_pid )
{
    ASSERT( n >= 0 && n < 4 );
    int error = 1;
    size_t max_size = *size;
    spt_buf_header_t * bufhdr = obj->buf_header[n];
    size_t bufhdrpos = bufhdr != NULL ? bufhdr->pos : 0;
    if ( bufhdrpos >= sizeof(spt_buf_header_t) + sizeof(spt_msg_header_t) )
    {
        ASSERT( bufhdr != NULL );
        spt_msg_header_t header;
        char * buf = obj->buf[n] ;
        bufhdrpos -= sizeof(header);
        memcpy( &header, buf + bufhdrpos , sizeof(header));
        *size = header.size;
        *pid = header.pid;
        *interm_pid = header.interm_pid;
        if ( header.size <= max_size ) {
            bufhdrpos -= header.size;
            memcpy( msg, buf + bufhdrpos, header.size);
            bufhdr->pos = bufhdrpos;
            error = 0;
        }
    }
    else 
    {
        *size = 0;
        *pid = -1;
        *interm_pid = -1;
    }

    return error ;
}

static void sparse_all_to_all_get_next_msg_pid( sparse_all_to_all_t * obj,
        int n, int * pid, int * interm_pid )
{
    spt_buf_header_t * bufhdr = obj->buf_header[n];
    size_t bufhdrpos = bufhdr != NULL ? bufhdr->pos : 0;
    if ( bufhdrpos >= sizeof(spt_buf_header_t) + sizeof(spt_msg_header_t) )
    {
        ASSERT( bufhdr != NULL );
        spt_msg_header_t header;
        char * buf = obj->buf[n] ;
        bufhdrpos -= sizeof(header);
        memcpy( &header, buf + bufhdrpos , sizeof(header) );
        *pid = header.pid;
        *interm_pid = header.interm_pid;
    }
    else 
    {
        *pid = -1;
        *interm_pid = -1;
    }
}

int sparse_all_to_all_recv( sparse_all_to_all_t * obj, 
        char * msg, size_t * size)
{
    int pid = -1, interm_pid = -1;
    return sparse_all_to_all_pop( obj, 0, msg, size, &pid, &interm_pid );
}

static void sparse_all_to_all_reset( sparse_all_to_all_t *obj, int n )
{
    if (obj->buf_header[n] != NULL ) {
        obj->buf_header[n]->pos = sizeof(spt_buf_header_t);
        obj->buf_header[n]->error = 0;
    }
    ASSERT( obj->nprocs > 0 );
    ASSERT( obj->pid >= 0);
    ASSERT( obj->pid < obj->nprocs );
}


static void sparse_all_to_all_send_error( sparse_all_to_all_t * obj, int n, int error )
{
    ASSERT( obj->buf_header[n] != NULL );
    obj->buf_header[n]->error = error;
}

static int sparse_all_to_all_recv_error( sparse_all_to_all_t * obj, int n)
{
    ASSERT( obj->buf_header[n] != NULL );
    return obj->buf_header[n]->error;
}

static int sparse_all_to_all_move_from_to( sparse_all_to_all_t * obj, int from, int to, int new_pid )
{
    ASSERT( from != to );
    int error = 1;
    int pid = -1;
    int interm_pid = -1;

    spt_buf_header_t * dstbufhdr = obj->buf_header[to];
    if ( dstbufhdr != NULL ) 
    {
        size_t dstbufhdrpos = dstbufhdr->pos;
        size_t bufsize = obj->buf_size;
        size_t size = bufsize - dstbufhdrpos;
        char * dstbuf = obj->buf[to];
        char * dst = dstbuf + dstbufhdrpos;
        if ( 0 == sparse_all_to_all_pop( obj, from, dst, &size, &pid, &interm_pid ) )
        {
           if ( bufsize - dstbufhdrpos >= size + sizeof(spt_msg_header_t ) ) 
           {
               spt_msg_header_t header; 
               header.size = size; header.pid = new_pid;
               header.interm_pid = interm_pid;
               memcpy( dst + size, &header, sizeof(header));
               dstbufhdr->pos = dstbufhdrpos + size + sizeof(header);
               error = 0;
           }
           else
           {
              obj->buf_header[from]->pos += sizeof(spt_msg_header_t) + size;
           }
        }
    }
    return error ;
}

static int sparse_all_to_all_merge_from_to( sparse_all_to_all_t * obj, int from, int to )
{
    int error = 1;
    size_t size = obj->buf_header[from]->pos - sizeof(spt_buf_header_t);
    if ( obj->buf_header[to] != NULL
            && obj->buf_size - obj->buf_header[to]->pos >= size ) 
    {
        memcpy( obj->buf[to] + obj->buf_header[to]->pos,
                obj->buf[from] + sizeof(spt_buf_header_t),
                size );
        obj->buf_header[to]->pos += size;
        obj->buf_header[to]->error |= obj->buf_header[from]->error;
        obj->buf_header[from]->pos = sizeof(spt_buf_header_t);
        obj->buf_header[from]->error = 0;
        error = 0;
    }

    return error;
}

static void sparse_all_to_all_copy_buf( sparse_all_to_all_t * obj, int from, int to)
{
    memcpy( obj->buf[to], obj->buf[from], obj->buf_header[from]->pos );
}

int sparse_all_to_all( MPI_Comm comm, sparse_all_to_all_t * obj, int do_prerandomize,
        const int * vote, int * ballot )
{
    const int nprocs = obj->nprocs;
    const int pid = obj->pid;
    int rc = MPI_SUCCESS;
#ifndef NDEBUG
    { int s, p;
      MPI_Comm_size( comm, &p);
      MPI_Comm_rank( comm, &s );
      ASSERT( nprocs == p );
      ASSERT( pid == s );
    }
#endif

    // Some memory must have been reserved before;
    ASSERT ( obj->buf_size != 0 ) ;

    sparse_all_to_all_reset( obj, 1);
    sparse_all_to_all_reset( obj, 2);
    sparse_all_to_all_copy_buf(obj, 0, 3); // make backup of original data

    // the log of p rounded upwards
    const int log_p = int_log2(nprocs-1)+1;

    int error = 0;
    ASSERT( nprocs > 0 );
    ASSERT( pid >= 0); 
    ASSERT( pid < nprocs);
    ASSERT( nprocs == obj->nprocs );
    ASSERT( pid == obj->pid );

    
    int i;
    int blockid[2];
    for (i = do_prerandomize?0:1; i < 2; ++i) 
    {
        // Bruck's algorithm with radix=2 follows
        // Note: Phase 1 is fused with the first subphase of phase 3
        //       Phase 3 is unnecessary, because we don't require any
        //       order between messages
        // 
        // Phase 2:
        const int w = log_p;
        int x = 0;
        for ( x = 0; x < w; ++x )
        {
            const int dst_pid = (pid + (1<<x)) % nprocs;
            const int src_pid = (pid + nprocs - (1<<x)) % nprocs;

            while (!error) {

                // get size and destination of next message in buf 0
                sparse_all_to_all_get_next_msg_pid( obj, 0, &blockid[1], &blockid[0]);

                // exit the loop if sorted out all messages
                if (blockid[0] == -1)
                    break;

                // we've found now a non-empty message
                //
                // on the first iteration execute phase 1: shifting local blocks upward
                if (i == 1 && x == 0 )
                    blockid[1] = (blockid[1] + nprocs - pid) % nprocs;

                int z = (blockid[i] >> x) & 0x1;
                if ( z ) 
                {
                    error |= sparse_all_to_all_move_from_to( obj, 0, 1, blockid[1] );
                }
                else
                {
                    int overflow = sparse_all_to_all_move_from_to( obj, 0, 2, blockid[1] );
                    ASSERT( !overflow );
                }
            }

            sparse_all_to_all_send_error( obj, 1, error );

            ASSERT( sizeof(spt_buf_header_t) <= obj->max_msg_size );
            ASSERT( obj->max_msg_size <= INT_MAX );

            size_t sent = 0, received = 0;
            size_t total_send = obj->buf_header[1]->pos;
            size_t total_recv = obj->buf_size;
            do {
                ASSERT( total_send >= sent );
                ASSERT( total_recv >= received );
                size_t send_chunk  = total_send - sent;
                size_t recv_chunk  = total_recv - received;
                
                if (send_chunk > obj->max_msg_size)
                    send_chunk = obj->max_msg_size;

                if (recv_chunk > obj->max_msg_size)
                    recv_chunk = obj->max_msg_size;

                MPI_Status status; 
                const int tag = 0;
                if ( send_chunk > 0 && recv_chunk > 0 ) {
                    rc = MPI_Sendrecv( 
                       obj->buf[1] + sent,     send_chunk, MPI_BYTE, dst_pid, 
                           tag,
                       obj->buf[0] + received, recv_chunk, MPI_BYTE, src_pid, 
                           tag,
                       comm, &status );
                    ASSERT( rc == MPI_SUCCESS  );
                    int r = 0; rc = MPI_Get_count( &status, MPI_BYTE, &r);
                    ASSERT( rc == MPI_SUCCESS  );
                    recv_chunk = r;
                }
                else if (send_chunk > 0 ) {
                    rc = MPI_Send( obj->buf[1] + sent, send_chunk, MPI_BYTE, 
                            dst_pid, tag, comm );
                }
                else { ASSERT( recv_chunk > 0 );
                    rc = MPI_Recv( obj->buf[0] + received, recv_chunk, MPI_BYTE, 
                            src_pid, tag, comm, &status );
                    ASSERT( rc == MPI_SUCCESS  );
                    int r = 0; rc = MPI_Get_count( &status, MPI_BYTE, &r);
                    ASSERT( rc == MPI_SUCCESS  );
                    recv_chunk = r;
                }

                sent += send_chunk;
                received += recv_chunk;
                total_recv = obj->buf_header[0]->pos;
            } while ( sent < total_send || received < total_recv );

            error |= sparse_all_to_all_recv_error( obj, 0 );

            sparse_all_to_all_reset( obj, 1);
            error |= sparse_all_to_all_merge_from_to( obj, 2, 0 );
        }
    }

    // do the final extra summation
    memcpy( obj->votes, vote, obj->vote_cats*sizeof(int));
    // since an overflow may be only detected on a subset of the processes
    obj->votes[obj->vote_cats] = error?1:0;

    memset( obj->ballots, 0, sizeof(int)*(obj->vote_cats + 1));
    rc = MPI_Allreduce( obj->votes, obj->ballots, obj->vote_cats + 1, 
            MPI_INT, MPI_SUM, comm );
    ASSERT( MPI_SUCCESS == rc );
    memcpy( ballot, obj->ballots, obj->vote_cats * sizeof(int));

    if (obj->ballots[obj->vote_cats] > 0 ) // restore data from backup
    {
        sparse_all_to_all_copy_buf(obj, 3, 0);
        return -1;
    }

    return 0;
}


size_t sparse_all_to_all_get_buf_capacity( const sparse_all_to_all_t * obj,
        size_t message_size )
{
    return obj->buf_size / (message_size + sizeof(spt_msg_header_t));
}


int sparse_all_to_all_is_empty( const sparse_all_to_all_t * obj)
{
    return obj->buf_size == 0 
        || obj->buf_header[0]->pos == sizeof(spt_buf_header_t);
}

void sparse_all_to_all_clear( sparse_all_to_all_t * obj )
{
    sparse_all_to_all_reset( obj, 0);
}


