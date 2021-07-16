
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

#include <bsp/bsp.h>
#undef bsp_end    
#undef bsp_abort  
#undef bsp_time   
#undef bsp_sync   
#undef bsp_nprocs 
#undef bsp_pid   
#undef bsp_push_reg 
#undef bsp_pop_reg 
#undef bsp_put   
#undef bsp_hpput 
#undef bsp_get   
#undef bsp_hpget 
#undef bsp_send  
#undef bsp_hpsend 
#undef bsp_qsize 
#undef bsp_get_tag 
#undef bsp_move  
#undef bsp_hpmove 

#include <cerrno>
#include <cstring>
#include <pthread.h>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <climits>

#include "lpf/core.h"
#include "lpf/bsplib.h"
#include "assert.hpp"
#include "config.hpp"

#ifndef _LPF_INCLUSIVE_MEMORY
#error "BSPlib cannot run on LPF without inclusive memory"
#endif

extern "C" int main( int argc, char ** argv ); 

void bsplibstd_main_spmd(void)
{
    main( 0, NULL );
}

namespace {

    pthread_key_t g_parent_bsp;
    pthread_key_t g_current_bsp;
    pthread_key_t g_bsplib;
    pthread_key_t g_spmd_func;

    pthread_once_t g_init = PTHREAD_ONCE_INIT;

    void init_thread_specifics(void) 
    {
        int rc = 0;
        rc |= pthread_key_create( & g_parent_bsp, NULL );
        rc |= pthread_key_create( & g_current_bsp, NULL );
        rc |= pthread_setspecific( g_current_bsp, &LPF_ROOT );
        rc |= pthread_key_create( & g_bsplib, NULL );
        rc |= pthread_key_create( & g_spmd_func, NULL );
        rc |= pthread_setspecific( g_spmd_func, 
                reinterpret_cast<void *>(&bsplibstd_main_spmd) );
        if ( rc != 0 )
        {
            fprintf(stderr, "bsplib: internal error at initialization\n");
            abort();
        }
    }


    void check( const char * func, bsplib_err_t rc )
    {
        switch (rc) {
            case BSPLIB_SUCCESS:
                return;

            case BSPLIB_ERR_OUT_OF_MEMORY:
                bsplibstd_abort("%s: out of memory\n", func );
                break;

            case BSPLIB_ERR_MEMORY_NOT_REGISTERED:
                bsplibstd_abort("%s: memory was not registered\n", func );
                break;

            case BSPLIB_ERR_PUSHREG_MISMATCH:
                bsplibstd_abort("%s: mismatch with memory registrations\n", func );
                break;

            case BSPLIB_ERR_POPREG_MISMATCH:
                bsplibstd_abort("%s: mismatch with memory deregistrations\n", func );
                break;

            case BSPLIB_ERR_TAGSIZE_MISMATCH:
                bsplibstd_abort("%s: mismatch with tag sizes\n", func );
                break;

            case BSPLIB_ERR_NULL_POINTER:
                bsplibstd_abort("%s: illegal use of null pointer\n", func );
                break;

            case BSPLIB_ERR_PID_OUT_OF_RANGE:
                bsplibstd_abort("%s: pid out of range\n", func );
                break;

            case BSPLIB_ERR_MEMORY_ACCESS_OUT_OF_RANGE:
                bsplibstd_abort("%s: remote memory access was out of range\n", func );
                break;

            case BSPLIB_ERR_EMPTY_MESSAGE_QUEUE:
                bsplibstd_abort("%s: BSMP message queue is empty\n", func );
                break;

            case BSPLIB_ERR_FATAL:
                bsplibstd_abort("%s: fatal error in communication sublayer\n", func );
                break;
        }
        bsplibstd_abort("internal error: unknown error code\n");
    }
} // anonymous namespace

void bsplibstd_spmd_proxy( lpf_t bsp, unsigned int pid, unsigned int nprocs, lpf_args_t args)
{
    pthread_once(&g_init, init_thread_specifics);

    ASSERT( args.f_size == 1);
    void (*spmd_part)(void);
    spmd_part = reinterpret_cast<void (*)(void)>(args.f_symbols[0]);

    bsplib_t bsplib;
    int safemode = lpf::Config::instance().getBSPlibSafetyMode();
    size_t maxHpRegs = lpf::Config::instance().getBSPlibMaxHpRegs();
    check("bsp_begin", bsplib_create( bsp, pid, nprocs, safemode, maxHpRegs, &bsplib ) );

    int prc = 0;
    lpf_t * grand_parent_bsp = static_cast<lpf_t *>(pthread_getspecific( g_parent_bsp ));
    lpf_t * parent_bsp = static_cast<lpf_t *>(pthread_getspecific( g_current_bsp ));
    bsplib_t * parent_bsplib = static_cast<bsplib_t *>(pthread_getspecific( g_bsplib ));

    prc |= pthread_setspecific( g_parent_bsp, parent_bsp );
    prc |= pthread_setspecific( g_current_bsp, & bsp );
    prc |= pthread_setspecific( g_bsplib, & bsplib );
    prc |= pthread_setspecific( g_spmd_func, NULL );
    if (prc != 0 )
        bsplibstd_abort( "bsp_begin: internal error in environment set-up; %s\n",
                std::strerror(errno) );

    (*spmd_part)();

    prc |= pthread_setspecific( g_parent_bsp, grand_parent_bsp );
    prc |= pthread_setspecific( g_current_bsp, parent_bsp );
    prc |= pthread_setspecific( g_bsplib, parent_bsplib );
    if (prc != 0 )
        bsplibstd_abort( "bsp_end: internal error in environment clean-up; %s\n",
                std::strerror(errno) );

    check( "bsp_end",  bsplib_destroy( bsplib ) );
}

namespace {
    void launch( unsigned int nprocs, void (*spmd_part)(void) )
    {
        lpf_args_t args;
        args.input = NULL;
        args.input_size = 0;
        args.output = NULL;
        args.output_size = 0;

        args.f_symbols = &spmd_part;
        args.f_size = 1;

        void *parent_spmd = pthread_getspecific( g_spmd_func );

        lpf_t * bsp = static_cast< lpf_t *>( pthread_getspecific( g_current_bsp ) );

        lpf_err_t rc = lpf_exec( *bsp, nprocs, &bsplibstd_spmd_proxy, args );
        if ( rc != LPF_SUCCESS )
            bsplibstd_abort( "bsp_end: spmd computation did not complete successfully\n");

        int prc = 0;
        prc = pthread_setspecific( g_spmd_func, parent_spmd );
        if ( prc != 0 )
            bsplibstd_abort( "bsp_end: internal error during environment clean-up; %s\n",
                    std::strerror(errno));
    }
}


void bsplibstd_init( void ( *spmd_part ) ( void ), int argc, char *argv[] )
{
    (void) argc;
    (void) argv;

    pthread_once(&g_init, init_thread_specifics);

    if ( spmd_part == NULL )
        bsplibstd_abort( "bsp_init: the pointer to the spmd function may not be NULL\n" );

    int rc = pthread_setspecific( g_spmd_func, reinterpret_cast<void *>(spmd_part) );
    if ( rc != 0 )
        bsplibstd_abort( "bsp_init: internal error saving spmd function; %s\n", std::strerror(errno));
}


void bsplibstd_error_spmd(void)
{
    bsplibstd_abort("bsp_begin: bsp_begin() was for called the second time without a call to bsp_init()\n");
}

int bsplibstd_spawn( unsigned int maxprocs )
{
    pthread_once(&g_init, init_thread_specifics);

    void (*spmd_func)(void) 
       = reinterpret_cast<void (*)(void)>( pthread_getspecific( g_spmd_func ));

    // whenever an spmd section is executed the g_spmd_func is set to NULL
    // (see bsplibstd_spmd_proxy() )

    if ( spmd_func == NULL ) {
        int rc = pthread_setspecific( g_spmd_func, 
                reinterpret_cast<void *>(bsplibstd_error_spmd) );

        if ( rc != 0 )
            bsplibstd_abort( "bsp_begin: internal error during environment set-up\n" );

        return 0;
    }
    else {
        if (maxprocs < 1) 
            bsplibstd_abort("bsp_begin: cannot perform computation on "
                      "less than one process\n"); 
        launch( maxprocs, spmd_func );

        spmd_func = reinterpret_cast<void (*)(void)>( pthread_getspecific( g_spmd_func) );
        if (spmd_func == bsplibstd_main_spmd)
            std::exit(EXIT_SUCCESS);
        return 1;
    }
}

void bsplibstd_end( void ) 
{
    pthread_once(&g_init, init_thread_specifics);
    void (*spmd_func)(void) 
       = reinterpret_cast<void (*)(void)>( pthread_getspecific( g_spmd_func ));

    if ( spmd_func != bsplibstd_error_spmd ) {
        bsplibstd_abort("bsp_end: called without preceding call to bsp_begin()\n");
    }
}

void bsplibstd_abort( const char *format, ... )
{
    va_list ap;
    va_start( ap, format );
    std::vfprintf( stderr, format, ap );
    std::abort();
    va_end( ap );
}

unsigned int mcbsp_nprocs( void )
{
    pthread_once(&g_init, init_thread_specifics);

    bsplib_t * bsplib 
        = static_cast<bsplib_t *>( pthread_getspecific( g_bsplib ) );
    if (bsplib == NULL)
    {
        if (LPF_ROOT == LPF_NONE)
            bsplibstd_abort("bsp_nprocs: cannot query machine parameters because "
                    "parallel context could not be found" );
        lpf_machine_t machine = LPF_INVALID_MACHINE;
        lpf_err_t rc = lpf_probe( LPF_ROOT, &machine );
        if (rc != LPF_SUCCESS)
            bsplibstd_abort("bsp_nprocs: failed to query machine parameters\n");

        return machine.p;
    }
    return bsplib_nprocs( *bsplib );
}

int bsplibstd_nprocs( void )
{ return static_cast<int>( mcbsp_nprocs() ); }

unsigned int mcbsp_pid( void )
{
    pthread_once(&g_init, init_thread_specifics);
    bsplib_t * bsplib 
        = static_cast<bsplib_t *>( pthread_getspecific( g_bsplib ) );
    if (bsplib == NULL)
        bsplibstd_abort("bsp_pid: can only be called between bsp_begin() and bsp_end()");
    return bsplib_pid( *bsplib );
}

int bsplibstd_pid( void )
{ return static_cast<int>( mcbsp_pid() ); }

double bsplibstd_time( void )
{
    pthread_once(&g_init, init_thread_specifics);
    bsplib_t * bsplib 
        = static_cast<bsplib_t *>( pthread_getspecific( g_bsplib ) );
    if (bsplib == NULL)
        bsplibstd_abort("bsp_time: can only be called between bsp_begin() and bsp_end()");
    return bsplib_time( *bsplib );
}

void bsplibstd_sync( void )
{
    pthread_once(&g_init, init_thread_specifics);
    bsplib_t * bsplib 
        = static_cast<bsplib_t *>( pthread_getspecific( g_bsplib ) );
    if (bsplib == NULL)
        bsplibstd_abort("bsp_sync: can only be called between bsp_begin() and bsp_end()");
    check( "bsp_sync", bsplib_sync( *bsplib ) );
}

void bsplibstd_push_reg( const void *ident, int size )
{
    if (size < 0)
        bsplibstd_abort("bsp_push_reg: cannot register memory with a negative extent\n");

    mcbsp_push_reg(ident, size);
}

void mcbsp_push_reg( const void *ident, size_t size )
{
    pthread_once(&g_init, init_thread_specifics);
    bsplib_t * bsplib 
        = static_cast<bsplib_t *>( pthread_getspecific( g_bsplib ) );
    if (bsplib == NULL)
        bsplibstd_abort("bsp_push_reg: can only be called between bsp_begin() and bsp_end()");
    check( "bsp_push_reg", bsplib_push_reg( *bsplib, ident, size ) );
}


void bsplibstd_pop_reg( const void *ident )
{
    pthread_once(&g_init, init_thread_specifics);
    bsplib_t * bsplib 
        = static_cast<bsplib_t *>( pthread_getspecific( g_bsplib ) );
    if (bsplib == NULL)
        bsplibstd_abort("bsp_pop_reg: can only be called between bsp_begin() and bsp_end()");
    check( "bsp_pop_reg", bsplib_pop_reg( *bsplib, ident ) );
}

void bsplibstd_put( int pid, const void *src, void *dst, int offset, int nbytes )
{
    if (nbytes == 0)
        return;
    if (pid < 0)
        bsplibstd_abort("bsp_put: pid cannot be negative\n");

    if (offset < 0)
        bsplibstd_abort("bsp_put: offset cannot be negative\n");

    if (nbytes < 0 )
        bsplibstd_abort("bsp_put: nbytes cannot be negative\n");

    mcbsp_put( pid, src, dst, offset, nbytes );
}

void mcbsp_put( unsigned int pid, const void *src, void *dst,
    size_t offset, size_t nbytes )
{
    pthread_once(&g_init, init_thread_specifics);
    bsplib_t * bsplib 
        = static_cast<bsplib_t *>( pthread_getspecific( g_bsplib ) );
    if (bsplib == NULL)
        bsplibstd_abort("bsp_put: can only be called between bsp_begin() and bsp_end()");
    check( "bsp_put", bsplib_put( *bsplib, pid, src, dst, offset, nbytes ) );
}

void bsplibstd_hpput( int pid, const void *src, void *dst, int offset, int nbytes )
{
    if (nbytes == 0)
        return;
    if (pid < 0)
        bsplibstd_abort("bsp_hpput: pid cannot be negative\n");

    if (offset < 0)
        bsplibstd_abort("bsp_hpput: offset cannot be negative\n");

    if (nbytes < 0 )
        bsplibstd_abort("bsp_hpput: nbytes cannot be negative\n");

    mcbsp_hpput( pid, src, dst, offset, nbytes );
}

void mcbsp_hpput( unsigned int pid, const void *src, void *dst,
    size_t offset, size_t nbytes )
{
    pthread_once(&g_init, init_thread_specifics);
    bsplib_t * bsplib 
        = static_cast<bsplib_t *>( pthread_getspecific( g_bsplib ) );
    if (bsplib == NULL)
        bsplibstd_abort("bsp_hpput: can only be called between bsp_begin() and bsp_end()");
    check( "bsp_hpput", bsplib_hpput( *bsplib, pid, src, dst, offset, nbytes ) );
}


void bsplibstd_get( int pid, const void *src, int offset, void *dst, int nbytes )
{
    if (nbytes == 0)
        return;
    if (pid < 0)
        bsplibstd_abort("bsp_get: pid cannot be negative\n");

    if (offset < 0)
        bsplibstd_abort("bsp_get: offset cannot be negative\n");

    if (nbytes < 0 )
        bsplibstd_abort("bsp_get: nbytes cannot be negative\n");

    mcbsp_get( pid, src, offset, dst, nbytes );
}

void mcbsp_get( unsigned int pid, const void *src, size_t offset,
    void *dst, size_t nbytes )
{
    pthread_once(&g_init, init_thread_specifics);
    bsplib_t * bsplib 
        = static_cast<bsplib_t *>( pthread_getspecific( g_bsplib ) );
    if (bsplib == NULL)
        bsplibstd_abort("bsp_get: can only be called between bsp_begin() and bsp_end()");
    check( "bsp_get", bsplib_get( *bsplib, pid, src, offset, dst, nbytes ) );
}

void bsplibstd_hpget( int pid, const void *src, int offset, void *dst, int nbytes )
{
    if (nbytes == 0)
        return;
    if (pid < 0)
        bsplibstd_abort("bsp_hpget: pid cannot be negative\n");

    if (offset < 0)
        bsplibstd_abort("bsp_hpget: offset cannot be negative\n");

    if (nbytes < 0 )
        bsplibstd_abort("bsp_hpget: nbytes cannot be negative\n");

    mcbsp_hpget( pid, src, offset, dst, nbytes );
}

void mcbsp_hpget( unsigned int pid, const void *src, size_t offset,
    void *dst, size_t nbytes )
{
    pthread_once(&g_init, init_thread_specifics);
    bsplib_t * bsplib 
        = static_cast<bsplib_t *>( pthread_getspecific( g_bsplib ) );
    if (bsplib == NULL)
        bsplibstd_abort("bsp_hpget: can only be called between bsp_begin() and bsp_end()");
    check( "bsp_get", bsplib_hpget( *bsplib, pid, src, offset, dst, nbytes ) );
}

void bsplibstd_set_tagsize( int * tag_nbytes )
{
    if (tag_nbytes == NULL)
        bsplibstd_abort("bsp_set_tagsize: illegal use of null pointer\n");

    size_t size = static_cast<size_t>(*tag_nbytes);
    mcbsp_set_tagsize( &size );
    if (size > INT_MAX)
        bsplibstd_abort("bsp_set_tagsize: previous tag size was too large\n");
    *tag_nbytes = static_cast<int>(size);
}


void mcbsp_set_tagsize( size_t * tag_nbytes )
{
    pthread_once(&g_init, init_thread_specifics);
    bsplib_t * bsplib 
        = static_cast<bsplib_t *>( pthread_getspecific( g_bsplib ) );
    if (bsplib == NULL)
        bsplibstd_abort("bsp_set_tagsize: can only be called between bsp_begin() and bsp_end()");
    *tag_nbytes = bsplib_set_tagsize( *bsplib, *tag_nbytes);
}



void bsplibstd_send( int pid, const void *tag, const void *payload, int payload_nbytes )
{
    if (pid < 0)
        bsplibstd_abort("bsp_send: pid cannot be negative\n");

    if (payload_nbytes < 0 )
        bsplibstd_abort("bsp_send: payload_nbytes cannot be negative\n");

    mcbsp_send( pid, tag, payload, payload_nbytes );
}


void mcbsp_send( unsigned int pid, const void *tag, const void *payload,
    size_t payload_nbytes )
{
    pthread_once(&g_init, init_thread_specifics);
    bsplib_t * bsplib 
        = static_cast<bsplib_t *>( pthread_getspecific( g_bsplib ) );
    if (bsplib == NULL)
        bsplibstd_abort("bsp_send: can only be called between bsp_begin() and bsp_end()");
    check( "bsp_send", bsplib_send( *bsplib, pid, tag, payload, payload_nbytes));
}

void bsplibstd_hpsend( int pid, const void *tag, const void *payload, int payload_nbytes )
{
    if (pid < 0)
        bsplibstd_abort("bsp_hpsend: pid cannot be negative\n");

    if (payload_nbytes < 0 )
        bsplibstd_abort("bsp_hpsend: payload_nbytes cannot be negative\n");

    mcbsp_hpsend( pid, tag, payload, payload_nbytes );
}

void mcbsp_hpsend( unsigned int pid, const void *tag, const void *payload,
    size_t payload_nbytes )
{
    pthread_once(&g_init, init_thread_specifics);
    bsplib_t * bsplib 
        = static_cast<bsplib_t *>( pthread_getspecific( g_bsplib ) );
    if (bsplib == NULL)
        bsplibstd_abort("bsp_hpsend: can only be called between bsp_begin() and bsp_end()");
    check( "bsp_hpsend",
            bsplib_hpsend( *bsplib, pid, tag, payload, payload_nbytes));
}

void bsplibstd_qsize( int * nmessages, int * accum_nbytes )
{
    unsigned int msgs = -1;
    size_t accum = -1;
    mcbsp_qsize( & msgs, & accum );

    if (nmessages != NULL) {
        if ( msgs > static_cast<unsigned int >(INT_MAX) )
            bsplibstd_abort("bsp_qsize: there are too many messages in the queue\n");
        *nmessages = static_cast<int>( msgs );
    }
    if (accum_nbytes != NULL) {
        if ( accum > INT_MAX )
            bsplibstd_abort("bsp_qsize: the total size of all received messages "
                     "is too large\n");
        *accum_nbytes = static_cast<int>( accum );
    }
}

void mcbsp_qsize( unsigned int * nmessages, size_t * accum_nbytes )
{
    pthread_once(&g_init, init_thread_specifics);
    size_t nmsgs = -1;
    bsplib_t * bsplib 
        = static_cast<bsplib_t *>( pthread_getspecific( g_bsplib ) );
    if (bsplib == NULL)
        bsplibstd_abort("bsp_qsize: can only be called between bsp_begin() and bsp_end()");
    check( "bsp_qsize",
            bsplib_qsize( *bsplib, &nmsgs, accum_nbytes ));

    if (nmessages != NULL) {
        if ( nmsgs > UINT_MAX )
            bsplibstd_abort("bsp_qsize: there are too many messages in the queue\n");
        *nmessages = static_cast<unsigned int>( nmsgs );
    }
}

void bsplibstd_get_tag( int * status, void *tag )
{
    size_t s = -1;
    mcbsp_get_tag( &s, tag );
    if ( s == static_cast<size_t>( -1) )
        *status = -1;
    else
        *status = static_cast<int>( s );
}

void mcbsp_get_tag( size_t * status, void *tag )
{
    pthread_once(&g_init, init_thread_specifics);
    bsplib_t * bsplib 
        = static_cast<bsplib_t *>( pthread_getspecific( g_bsplib ) );
    if (bsplib == NULL)
        bsplibstd_abort("bsp_get_tag: can only be called between bsp_begin() and bsp_end()");
    check( "bsp_get_tag",
            bsplib_get_tag( *bsplib, status, tag ));
}

void bsplibstd_move( void *payload, int reception_bytes )
{
    mcbsp_move( payload, reception_bytes );
}

void mcbsp_move( void *payload, size_t reception_bytes )
{
    pthread_once(&g_init, init_thread_specifics);
    bsplib_t * bsplib 
        = static_cast<bsplib_t *>( pthread_getspecific( g_bsplib ) );
    if (bsplib == NULL)
        bsplibstd_abort("bsp_move: can only be called between bsp_begin() and bsp_end()");
    check( "bsp_move",
            bsplib_move( *bsplib, payload, reception_bytes ));
}

int bsplibstd_hpmove( const void **tag_ptr, const void **payload_ptr )
{
    size_t s = mcbsp_hpmove( const_cast<void **>(tag_ptr), 
            const_cast<void **>(payload_ptr) );
    if (s > INT_MAX)
        bsplibstd_abort("bsp_hpmove: message too big\n");

    return static_cast<int>(s);
}

size_t mcbsp_hpmove( void **tag_ptr, void **payload_ptr )
{
    pthread_once(&g_init, init_thread_specifics);
    bsplib_t * bsplib 
        = static_cast<bsplib_t *>( pthread_getspecific( g_bsplib ) );
    if (bsplib == NULL)
        bsplibstd_abort("bsp_hpmove: can only be called between bsp_begin() and bsp_end()");
    return  bsplib_hpmove( *bsplib, 
            const_cast<const void **>(tag_ptr), 
            const_cast<const void **>(payload_ptr) );
}


