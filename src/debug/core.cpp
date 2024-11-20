
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

#include "debug/lpf/core.h"
#include "lpf/abort.h"

#undef lpf_get
#undef lpf_put
#undef lpf_sync
#undef lpf_register_local
#undef lpf_register_global
#undef lpf_deregister
#undef lpf_probe
#undef lpf_resize_memory_register
#undef lpf_resize_message_queue
#undef lpf_exec
#undef lpf_hook
#undef lpf_rehook

#undef lpf_init_t
#undef lpf_pid_t
#undef lpf_args
#undef lpf_args_t
#undef lpf_memslot_t
#undef lpf_msg_attr_t
#undef lpf_machine
#undef lpf_machine_t
#undef lpf_sync_attr_t
#undef lpf_t
#undef lpf_spmd_t
#undef lpf_func_t
#undef lpf_err_t

#undef LPF_ROOT
#undef LPF_MAX_P
#undef LPF_SUCCESS
#undef LPF_ERR_OUT_OF_MEMORY
#undef LPF_ERR_FATAL
#undef LPF_MSG_DEFAULT
#undef LPF_SYNC_DEFAULT
#undef LPF_INVALID_MEMSLOT
#undef LPF_INVALID_MACHINE
#undef LPF_NONE
#undef LPF_INIT_NONE
#undef LPF_NO_ARGS

#if __cplusplus >= 201103L
#include <unordered_map>
#include <unordered_set>
#else
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#endif
#include <cstdlib>
#include <vector>
#include <limits>
#include <cstring>
#include <algorithm>
#include <stdexcept>


#include "assert.hpp"
#include "linkage.hpp"
#include "log.hpp"
#include "sparseset.hpp"
#include "memreg.hpp"
#include "rwconflict.hpp"

namespace lpf { namespace debug {


class _LPFLIB_LOCAL Interface {

#if __cplusplus >= 201103L
    typedef std::unordered_map< lpf_t, Interface * > CtxStore;
#else
    typedef std::tr1::unordered_map< lpf_t, Interface * > CtxStore;
#endif

    static pthread_once_t s_threadInit ;
    static pthread_key_t s_threadKeyCtxStore;

    static void destroyCtxStore( void * ptr )
    {
        delete static_cast< CtxStore *>( ptr );
    }

    static void threadInit() {
        // in the below we use std::abort as these are critical *internal*
        // errors, not errors in the use of LPF core functionality.
        // By contrast, errors that appear due to misuse of the LPF core primitives
        // should call lpf_abort. This initialiser ensures that the underlying LPF
        // engine has support for lpf_abort.
        // The above logic about when to std::abort and when to lpf_abort is applied
        // consistently in the below implementation. Only (seemingly) exceptions will
        // be documented henceforth.
        int rc = pthread_key_create( &s_threadKeyCtxStore, &destroyCtxStore );
        if (rc) {
            LOG( 0, "Internal error while initializing thread static storage" );
            std::abort();
        }
        if( ! LPF_HAS_ABORT ) {
            LOG( 0, "Debug layer relies on lpf_abort, but selected engine does not support it" );
            std::abort();
        }
    }

public:

    static Interface & root()
    {
       static Interface object( LPF_ROOT, 0, 1 );
       return object;
    }

    static void storeCtx( lpf_t ctx, Interface * obj )
    {
        pthread_once( &s_threadInit, threadInit );

        CtxStore * ctxStore = static_cast< CtxStore *>(
                pthread_getspecific( s_threadKeyCtxStore )
                );

        if (ctxStore == NULL) {
            ctxStore = new CtxStore;
            int rc = pthread_setspecific( s_threadKeyCtxStore, ctxStore );
            if (rc) {
                LOG( 0, "Internal error while initializing thread static storage");
                std::abort();
            }
        }

        ctxStore->insert( std::make_pair( ctx, obj ) );
    }

    static void deleteCtx( lpf_t ctx, Interface * obj)
    {
        pthread_once( &s_threadInit, threadInit );

        if ( ctx == LPF_ROOT )
            return;

        CtxStore * ctxStore = static_cast< CtxStore *>(
                pthread_getspecific( s_threadKeyCtxStore )
                );

        if ( ctxStore == NULL ) {
            LOG( 0, "Internal error while cleaning up thread static storage");
            std::abort();
        }

        CtxStore :: iterator i = ctxStore->find( ctx );
        if ( i == ctxStore->end() || i->second != obj ) {
            LOG( 0, "Internal error while cleaning up thread static storage");
            std::abort();
        }

        ctxStore->erase( i );
    }

    static Interface * lookupCtx( const char * file, int line,  lpf_t ctx )
    {
        pthread_once( &s_threadInit, threadInit );

        if ( ctx == LPF_ROOT )
            return &root();

        CtxStore * ctxStore = static_cast< CtxStore *>(
                pthread_getspecific( s_threadKeyCtxStore )
                );

        if (ctxStore == NULL) {
            LOG( 0, file << ":" << line
                    << ": Invalid LPF context argument: " << ctx );
            std::abort();
        }

        CtxStore::const_iterator it = ctxStore->find( ctx );

        if (it == ctxStore->end()) {
            LOG( 0, file << ":" << line
                    << ": Invalid LPF context argument: " << ctx );
            std::abort();
        }

        return it->second;
    }

    _LPFLIB_API static void launcher( lpf_t ctx, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args ) {
        char * output = static_cast< char *>( args.output );
        bool extraOutput = args.output_size >= sizeof(lpf_err_t);
        if (extraOutput) args.output_size -= sizeof(lpf_err_t);
        try {
            if (args.output_size == 0 ) args.output = NULL;

            lpf_spmd_t spmd = reinterpret_cast<lpf_spmd_t>( args.f_symbols[ args.f_size - 1] );
            args.f_size -= 1;

            Interface newCtx( ctx, pid, nprocs );
            storeCtx( ctx, &newCtx );
            (*spmd)( ctx, pid, nprocs, args );
            newCtx.check_leaks();
            newCtx.cleanup();
            deleteCtx( ctx, &newCtx );

            if (extraOutput) {
                memcpy( output + args.output_size, & LPF_SUCCESS, sizeof(lpf_err_t));
                args.output_size += sizeof(lpf_err_t);
            }
        }
        catch( std::bad_alloc & )
        {
            LOG( 2, "Debug mode ran out of memory" );
             if ( extraOutput ) {
                memcpy( output + args.output_size, & LPF_ERR_OUT_OF_MEMORY, sizeof(lpf_err_t));
                args.output_size += sizeof(lpf_err_t);
             }
        }
    }

    Interface( lpf_t ctx, lpf_pid_t pid, lpf_pid_t nprocs )
        : m_ctx( ctx )
        , m_pid( pid )
        , m_nprocs( nprocs )
        , m_memreg()
        , m_mesgq_reserved(0)
        , m_mesgq_reserved_next(0)
        , m_memreg_size(0)
        , m_memreg_reserved(0)
        , m_memreg_reserved_next(0)
        , m_active_regs()
        , m_new_global_regs()
        , m_new_global_deregs()
        , m_used_regs()
        , m_rwconflict()
        , m_comm_bufs_registered(false)
        , m_memreg_reserved_all( m_nprocs, 0u )
        , m_mesgq_reserved_all( m_nprocs, 0u )
        , m_glob_regs( m_nprocs, 0u )
        , m_glob_deregs( m_nprocs, 0u )
        , m_glob_slots( m_nprocs, LPF_INVALID_MEMSLOT )
        , m_max_out( m_nprocs, 0u )
        , m_max_in( m_nprocs, 0u )
        , m_remote_access_by_me()
        , m_remote_access_by_me_offsets_remote( m_nprocs + 1, 0u)
        , m_remote_access_by_me_offsets_local( m_nprocs + 1, 0u)
        , m_local_access_by_remote()
        , m_local_access_by_remote_offsets( m_nprocs + 1, 0u)
        , m_resized_by_me( false )
        , m_resized( false )
        , m_memreg_reserved_all_slot( LPF_INVALID_MEMSLOT )
        , m_mesgq_reserved_all_slot( LPF_INVALID_MEMSLOT )
        , m_glob_regs_slot( LPF_INVALID_MEMSLOT )
        , m_glob_deregs_slot( LPF_INVALID_MEMSLOT )
        , m_glob_slots_slot( LPF_INVALID_MEMSLOT )
        , m_max_out_slot( LPF_INVALID_MEMSLOT )
        , m_max_in_slot( LPF_INVALID_MEMSLOT )
        , m_remote_access_by_me_slot( LPF_INVALID_MEMSLOT )
        , m_remote_access_by_me_offsets_remote_slot( LPF_INVALID_MEMSLOT )
        , m_remote_access_by_me_offsets_local_slot( LPF_INVALID_MEMSLOT )
        , m_local_access_by_remote_slot( LPF_INVALID_MEMSLOT )
        , m_local_access_by_remote_offsets_slot( LPF_INVALID_MEMSLOT )
        , m_resized_by_me_slot( LPF_INVALID_MEMSLOT )
        , m_resized_slot( LPF_INVALID_MEMSLOT )
    {
    }


    void cleanup()
    {
        if ( m_comm_bufs_registered ) {
            lpf_deregister( m_ctx, m_mesgq_reserved_all_slot);
            lpf_deregister( m_ctx, m_memreg_reserved_all_slot);
            lpf_deregister( m_ctx, m_glob_regs_slot);
            lpf_deregister( m_ctx, m_glob_deregs_slot);
            lpf_deregister( m_ctx, m_glob_slots_slot);
            lpf_deregister( m_ctx, m_max_out_slot);
            lpf_deregister( m_ctx, m_max_in_slot);
            lpf_deregister( m_ctx, m_resized_by_me_slot );
            lpf_deregister( m_ctx, m_resized_slot );
            lpf_deregister( m_ctx, m_remote_access_by_me_offsets_remote_slot);
            lpf_deregister( m_ctx, m_remote_access_by_me_offsets_local_slot);
            lpf_deregister( m_ctx, m_local_access_by_remote_offsets_slot);
            m_comm_bufs_registered = false;
        }
        if ( m_remote_access_by_me_slot != LPF_INVALID_MEMSLOT )
            lpf_deregister( m_ctx, m_remote_access_by_me_slot);
        if ( m_local_access_by_remote_slot != LPF_INVALID_MEMSLOT )
            lpf_deregister( m_ctx, m_local_access_by_remote_slot);

        m_mesgq_reserved_all_slot = LPF_INVALID_MEMSLOT;
        m_memreg_reserved_all_slot = LPF_INVALID_MEMSLOT;
        m_glob_regs_slot = LPF_INVALID_MEMSLOT;
        m_glob_deregs_slot = LPF_INVALID_MEMSLOT;
        m_glob_slots_slot = LPF_INVALID_MEMSLOT;
        m_max_out_slot = LPF_INVALID_MEMSLOT;
        m_max_in_slot = LPF_INVALID_MEMSLOT;
        m_remote_access_by_me_slot = LPF_INVALID_MEMSLOT ;
        m_remote_access_by_me_offsets_remote_slot = LPF_INVALID_MEMSLOT ;
        m_remote_access_by_me_offsets_local_slot = LPF_INVALID_MEMSLOT ;
        m_local_access_by_remote_slot = LPF_INVALID_MEMSLOT ;
        m_local_access_by_remote_offsets_slot = LPF_INVALID_MEMSLOT ;
        m_resized_by_me_slot = LPF_INVALID_MEMSLOT;
        m_resized_slot = LPF_INVALID_MEMSLOT;
    }

    void reg_comm_bufs() {
        if ( !m_comm_bufs_registered) {
            lpf_register_global( m_ctx, m_mesgq_reserved_all.data(),
                    m_nprocs * sizeof(size_t), & m_mesgq_reserved_all_slot);
            lpf_register_global( m_ctx, m_memreg_reserved_all.data(),
                    m_nprocs * sizeof(size_t), & m_memreg_reserved_all_slot);
            lpf_register_global( m_ctx, m_glob_regs.data(),
                    m_nprocs * sizeof(size_t), & m_glob_regs_slot);
            lpf_register_global( m_ctx, m_glob_deregs.data(),
                    m_nprocs * sizeof(size_t), & m_glob_deregs_slot);
            lpf_register_global( m_ctx, m_glob_slots.data(),
                    m_nprocs * sizeof(size_t), & m_glob_slots_slot);
            lpf_register_global( m_ctx, m_max_out.data(),
                    m_nprocs * sizeof(size_t), & m_max_out_slot);
            lpf_register_global( m_ctx, m_max_in.data(),
                    m_nprocs * sizeof(size_t), & m_max_in_slot);
            lpf_register_global( m_ctx,
                    m_remote_access_by_me_offsets_remote.data(),
                    (m_nprocs + 1) * sizeof(Access),
                    & m_remote_access_by_me_offsets_remote_slot );
            lpf_register_global( m_ctx,
                    m_remote_access_by_me_offsets_local.data(),
                    (m_nprocs + 1) * sizeof(Access),
                    & m_remote_access_by_me_offsets_local_slot );
            lpf_register_global( m_ctx,
                    m_local_access_by_remote_offsets.data(),
                    (m_nprocs + 1) * sizeof(Access),
                    & m_local_access_by_remote_offsets_slot );
            lpf_register_global( m_ctx, &m_resized_by_me,
                    sizeof(m_resized_by_me), & m_resized_by_me_slot );
            lpf_register_global( m_ctx, &m_resized, sizeof(m_resized),
                    & m_resized_slot );
            m_comm_bufs_registered = true;
        }
    }

    void check_leaks()
    {
        for (size_t i = 0; i < m_puts.size(); ++i) {
            LOG( 0, m_puts[i].file << ":" << m_puts[i].line << ": pid " << m_pid
                    << ": Warning: A lpf_put to " << m_puts[i].dstPid <<
                       " is still outstanding after the spmd function has finished" );
        }

        for (size_t i = 0; i < m_gets.size(); ++i) {
            LOG( 0, m_gets[i].file << ":" << m_gets[i].line << ": pid " << m_pid
                    << ": Warning: A lpf_get from " << m_gets[i].srcPid <<
                       " is still outstanding after the spmd function has finished" );
        }

        for (Regs::iterator i = m_active_regs.begin();
               i != m_active_regs.end(); ++i ) {
            const Memslot & slot = m_memreg.lookup( *i );
            LOG( 0, slot.file << ":" << slot.line << ": pid " << m_pid
                    << ": Warning: Memory was not deregistered before "
                         "spmd function finished");
        }
    }

    void print_request_queue( std::ostream & out ) {
        for (size_t i = 0; i < m_puts.size(); ++i) {
            Put put = m_puts[i];
            out << "lpf_put( " << put.file << ":" << put.line << ", "
                << "src_slot = " << put.srcSlot  ;

            if ( m_active_regs.find( put.srcSlot ) != m_active_regs.end() ) {
                const Memslot & ss = m_memreg.lookup( put.srcSlot );
                out << "( registered at " << ss.file << ":" << ss.line;
                if (ss.kind == Memslot :: Global)
                    out << " global size = " << ss.size[m_pid] << " bytes ) ";
                else
                    out << " local size = " << ss.size[0] << " bytes )";
            }
            out << ", src_offset = " << put.srcOffset
                << ", dst_pid = " << put.dstPid
                << ", dst_slot = " << put.dstSlot;
            if ( m_active_regs.find( put.dstSlot ) != m_active_regs.end() ) {
                const Memslot & ds = m_memreg.lookup( put.dstSlot );
                out << "( registered at " << ds.file << ":" << ds.line;
                if (ds.kind == Memslot :: Global)
                    out << " global size = " << ds.size[put.dstPid] << " bytes ) ";
                else
                    out << " local size = " << ds.size[0] << " bytes )";
            }
            out << ", dst_offset = " << put.dstOffset
                << ", size = " << put.size  << " );\n" ;
        }

        for (size_t i = 0; i < m_gets.size(); ++i) {
            Get get = m_gets[i];
            out << "lpf_get( " << get.file << ":" << get.line << ", "
                << "src_pid = " << get.srcPid
                << ", src_slot = " << get.srcSlot  ;
            if ( m_active_regs.find( get.srcSlot ) != m_active_regs.end() ) {
                const Memslot & ss = m_memreg.lookup( get.srcSlot );
                out << "( registered at " << ss.file << ":" << ss.line;
                if (ss.kind == Memslot :: Global)
                    out << " global size = " << ss.size[get.srcPid] << " bytes ) ";
                else
                    out << " local size = " << ss.size[0] << " bytes )";
            }
            out << ", src_offset = " << get.srcOffset
                << ", dst_slot = " << get.dstSlot;
            if ( m_active_regs.find( get.dstSlot ) != m_active_regs.end() ) {
                const Memslot & ds = m_memreg.lookup( get.dstSlot );
                out << "( registered at " << ds.file << ":" << ds.line;
                if (ds.kind == Memslot :: Global)
                    out << " global size = " << ds.size[m_pid] << " bytes ) ";
                else
                    out << " local size = " << ds.size[0] << " bytes )";
            }
            out << ", dst_offset = " << get.dstOffset
                << ", size = " << get.size  << " )\n" ;
        }
    }


    lpf_err_t exec( const char * file, int line,
            lpf_pid_t P, lpf_spmd_t spmd, lpf_args_t args )
    {
        if  ( P <= 0 && P != LPF_MAX_P ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": Invalid argument passed to lpf_exec: P = " << P );
            lpf_abort(m_ctx);
        }
        if ( spmd == NULL ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": Invalid argument passed to lpf_exec: NULL spmd argument" );
            lpf_abort(m_ctx);
        }
        if ( args.input_size != 0 && args.input == NULL ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": Invalid argument passed to lpf_spmd_t: NULL input argument while input_size is non-zero" );
            lpf_abort(m_ctx);
        }
        if ( args.output_size != 0 && args.output == NULL ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": Invalid argument passed to lpf_spmd_t: NULL output argument while output_size is non-zero" );
            lpf_abort(m_ctx);
        }
        if ( args.f_size != 0 && args.f_symbols == NULL ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": Invalid argument passed to lpf_spmd_t: NULL f_symbols argument while f_size is non-zero" );
            lpf_abort(m_ctx);
        }

        lpf_args_t new_args;
        new_args.input = args.input;
        new_args.input_size = args.input_size;

        try {
            std::vector< char *> output( args.output_size + sizeof(lpf_err_t));
            memcpy( output.data(), args.output, args.output_size );
            memcpy( output.data() + args.output_size, &LPF_SUCCESS, sizeof(lpf_err_t) );
            new_args.output = output.data();
            new_args.output_size = output.size();

            std::vector< void (*)() > symbols( args.f_size + 1);
            std::copy( args.f_symbols, args.f_symbols + args.f_size, symbols.data() );
            symbols.back() = reinterpret_cast<void (*)()>(spmd);

            new_args.f_symbols = symbols.data();
            new_args.f_size = symbols.size();

            lpf_err_t rc = lpf_exec( m_ctx, P, launcher, new_args );
            memcpy( args.output, output.data(), args.output_size );

            if (rc == LPF_SUCCESS ) {
                memcpy( &rc, output.data() + args.output_size, sizeof(lpf_err_t) );
            }
            return rc;
        }
        catch( std::bad_alloc & ) {
            return LPF_ERR_OUT_OF_MEMORY;
        }
    }

    static lpf_err_t hook( const char * file, int line,
            lpf_init_t init, lpf_spmd_t spmd, lpf_args_t args )
    {
        // the lpf_hook could arise from any non-LPF context -- this is in fact
        // why it exists: hooking from within an LPF context to create a subcontext is
        // provided by lpf_rehook instead.
        // Because the callee context is potentially not controlled by the underlying
        // LPF engine, and because the callee context in the non-trivial case consists
        // of multiple distributed processes, we cannot rely on lpf_abort. The only
        // thing we can do is rely on the standard abort.
        if ( spmd == NULL ) {
            LOG( 0, file << ":" << line
                    << ": Invalid argument passed to lpf_hook: NULL spmd argument" );
            std::abort();
        }
        if ( args.input_size != 0 && args.input == NULL ) {
            LOG( 0, file << ":" << line
                    << ": Invalid argument passed to lpf_spmd_t: NULL input argument while input_size is non-zero" );
            std::abort();
        }
        if ( args.output_size != 0 && args.output == NULL ) {
            LOG( 0, file << ":" << line
                    << ": Invalid argument passed to lpf_spmd_t: NULL output argument while output_size is non-zero" );
            std::abort();
        }
        if ( args.f_size != 0 && args.f_symbols == NULL ) {
            LOG( 0, file << ":" << line
                    << ": Invalid argument passed to lpf_spmd_t: NULL f_symbols argument while f_size is non-zero" );
            std::abort();
        }

        lpf_args_t new_args;
        new_args.input = args.input;
        new_args.input_size = args.input_size;

        try {
            std::vector< char *> output( args.output_size + sizeof(lpf_err_t));
            memcpy( output.data(), args.output, args.output_size );
            memcpy( output.data() + args.output_size, &LPF_SUCCESS, sizeof(lpf_err_t) );
            new_args.output = output.data();
            new_args.output_size = output.size();

            std::vector< void (*)() > symbols( args.f_size + 1);
            std::copy( args.f_symbols, args.f_symbols + args.f_size, symbols.data() );
            symbols.back() = reinterpret_cast<void (*)()>(spmd);

            new_args.f_symbols = symbols.data();
            new_args.f_size = symbols.size();

            lpf_err_t rc = lpf_hook( init, launcher, new_args );
            memcpy( args.output, output.data(), args.output_size );

            if (rc == LPF_SUCCESS ) {
                memcpy( &rc, output.data() + args.output_size, sizeof(lpf_err_t) );
            }
            return rc;
        }
        catch( std::bad_alloc & ) {
            return LPF_ERR_OUT_OF_MEMORY;
        }

    }

    lpf_err_t rehook( const char * file, int line,
            lpf_spmd_t spmd, lpf_args_t args )
    {
        if ( spmd == NULL ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": Invalid argument passed to lpf_rehook: NULL spmd argument" );
            lpf_abort(m_ctx);
        }
        if ( args.input_size != 0 && args.input == NULL ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": Invalid argument passed to lpf_spmd_t: NULL input argument while input_size is non-zero" );
            lpf_abort(m_ctx);
        }
        if ( args.output_size != 0 && args.output == NULL ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": Invalid argument passed to lpf_spmd_t: NULL output argument while output_size is non-zero" );
            lpf_abort(m_ctx);
        }
        if ( args.f_size != 0 && args.f_symbols == NULL ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": Invalid argument passed to lpf_spmd_t: NULL f_symbols argument while f_size is non-zero" );
            lpf_abort(m_ctx);
        }

        lpf_args_t new_args;
        new_args.input = args.input;
        new_args.input_size = args.input_size;

        try {
            std::vector< char *> output( args.output_size + sizeof(lpf_err_t));
            memcpy( output.data(), args.output, args.output_size );
            memcpy( output.data() + args.output_size, &LPF_SUCCESS, sizeof(lpf_err_t) );
            new_args.output = output.data();
            new_args.output_size = output.size();

            std::vector< void (*)() > symbols( args.f_size + 1);
            std::copy( args.f_symbols, args.f_symbols + args.f_size, symbols.data() );
            symbols.back() = reinterpret_cast<void (*)()>(spmd);

            new_args.f_symbols = symbols.data();
            new_args.f_size = symbols.size();

            lpf_err_t rc = lpf_rehook( m_ctx, launcher, new_args );
            memcpy( args.output, output.data(), args.output_size );

            if (rc == LPF_SUCCESS ) {
                memcpy( &rc, output.data() + args.output_size, sizeof(lpf_err_t) );
            }
            return rc;
        }
        catch( std::bad_alloc & ) {
            return LPF_ERR_OUT_OF_MEMORY;
        }
    }

    lpf_err_t probe( const char * file, int line, lpf_machine_t * params )
    {
        (void) file; (void) line;
        return lpf_probe( m_ctx, params );
    }


    lpf_err_t resize_message_queue( const char * file, int line, size_t max_msgs )
    {
        (void) file;
        (void) line;

        // because 10*nprocs are necessary for internal messages
        size_t internal = 10 * m_nprocs;
        LPFLIB_IGNORE_TAUTOLOGIES
        if ( m_nprocs >= std::numeric_limits<size_t>::max() / 10 )
            return LPF_ERR_OUT_OF_MEMORY;
        LPFLIB_RESTORE_WARNINGS

        try {
            m_resized_by_me = true;
            m_remote_access_by_me.reserve( max_msgs );
            m_local_access_by_remote.reserve( max_msgs );
        }
        catch( std::bad_alloc & )
        {
            return LPF_ERR_OUT_OF_MEMORY;
        }
        catch( std::length_error & )
        {
            return LPF_ERR_OUT_OF_MEMORY;
        }

        lpf_err_t rc = lpf_resize_message_queue( m_ctx,
               std::max<size_t>( max_msgs , internal ) );
        if (rc == LPF_SUCCESS) {
            m_mesgq_reserved_next = max_msgs;
        }
        return rc;
    }

    lpf_err_t resize_memory_register( const char * file, int line, size_t max_regs )
    {
        (void) file;
        (void) line;

        size_t internal = INTERNAL_MEMREGS;
        if ( max_regs >= std::numeric_limits<size_t>::max() - internal
                || max_regs >= std::numeric_limits<size_t>::max() / 2 )
            return LPF_ERR_OUT_OF_MEMORY;

        m_resized_by_me = true;
        try {
            if ( max_regs >= m_memreg_reserved
                    && max_regs >= m_memreg_reserved_next ) {

                m_memreg.reserve( 2 * max_regs ); // the factor 2x is
                // necessary to maintain the illusion that mem slots
                // are freed up immediately by a lpf_deregister
            }
        } catch( std::bad_alloc & )
        {
            return LPF_ERR_OUT_OF_MEMORY;
        }

        lpf_err_t rc = lpf_resize_memory_register( m_ctx, max_regs + internal );
        if (rc == LPF_SUCCESS) {
            m_memreg_reserved_next = max_regs;
        }
        return rc;
    }


    lpf_err_t register_global( const char * file, int line,
            void * pointer, size_t size, lpf_memslot_t * memslot )
    {
        if ( m_memreg_size >= m_memreg_reserved ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": Invalid global memory registration, "
                    "which would have taken the " << (m_memreg_size+1)
                    << "-th slot, while only space for " << m_memreg_reserved
                    << " slots has been reserved" );
            lpf_abort(m_ctx);
        }
        Memslot slot;
        slot.file = file;
        slot.line = line;
        slot.pointer = pointer;
        slot.slot = LPF_INVALID_MEMSLOT;
        slot.kind = Memslot::Global;
        slot.size.resize( m_nprocs, size_t(-1));
        slot.size[m_pid] = size;
        slot.active = false;

        *memslot = m_memreg.addGlobalReg( slot );

        m_memreg_size += 1;
        m_glob_regs[ m_pid ] += 1;
        m_new_global_regs.insert( *memslot );
        m_active_regs.insert( *memslot );

        return LPF_SUCCESS;
    }

    lpf_err_t register_local( const char * file, int line,
            void * pointer, size_t size, lpf_memslot_t * memslot )
    {
        if ( m_memreg_size >= m_memreg_reserved ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": Invalid local memory registration, "
                    "which would have taken the " << (m_memreg_size+1)
                    << "-th slot, while only space for " << m_memreg_reserved
                    << " slots has been reserved" );
            lpf_abort(m_ctx);
        }

        Memslot slot;
        slot.file = file;
        slot.line = line;
        slot.pointer = pointer;
        (void) lpf_register_local( m_ctx, pointer, size, &(slot.slot) );
        slot.kind = Memslot::Local;
        slot.size.resize( 1 );
        slot.size[ 0 ] = size;
        slot.active = true;

        *memslot = m_memreg.addLocalReg( slot );
        m_active_regs.insert( *memslot );
        m_memreg_size += 1;
        return LPF_SUCCESS;
    }

    lpf_err_t deregister( const char * file, int line, lpf_memslot_t slot )
    {
        if ( m_active_regs.find( slot ) == m_active_regs.end() ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": Invalid attempt to deregister a memory slot, "
                    "because it has not been registered before" );
            lpf_abort(m_ctx);
        }

        if ( m_used_regs.find( slot ) != m_used_regs.end() ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": Invalid attempt to deregister a memory slot, "
                    "because it is in use by the primitive on "
                    << m_used_regs[slot].first << ":"
                    << m_used_regs[slot].second );
            lpf_abort(m_ctx);
        }

        if ( m_memreg.lookup( slot ).kind == Memslot::Global ) {

            if ( m_new_global_regs.find( slot ) == m_new_global_regs.end() ) {
                m_glob_deregs[ m_pid ] += 1;
                m_new_global_deregs.push_back( slot );
                // wait until sync with removal from m_memreg, because
                // we need the data for the call to lpf_deregister
                // note: this causes the 2x in the resize command
            }
            else {
                m_glob_regs[ m_pid ] -= 1;
                m_new_global_regs.erase( slot );
                m_memreg.removeReg( slot );
            }
        }
        else
        {
            lpf_deregister( m_ctx, m_memreg.lookup( slot ).slot );
            m_memreg.removeReg( slot );
        }
        m_active_regs.erase( slot );
        m_memreg_size -= 1;
        return LPF_SUCCESS;
    }

    lpf_err_t put( const char * file, int line,
            lpf_memslot_t src_slot, size_t src_offset,
                lpf_pid_t dst_pid, lpf_memslot_t dst_slot, size_t dst_offset,
                size_t size, lpf_msg_attr_t attr = LPF_MSG_DEFAULT )
    {
        LPFLIB_IGNORE_TAUTOLOGIES
        if ( dst_pid < 0 || dst_pid >= m_nprocs ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": unknown process ID " << dst_pid << " for data destination " );
            lpf_abort(m_ctx);
        }
        LPFLIB_RESTORE_WARNINGS

        if ( src_offset > std::numeric_limits<size_t>::max() - size ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": numerical overflow while computing src_offset + size = "
                    << src_offset << " + " << size << " > SIZE_MAX" );
            lpf_abort(m_ctx);
        }

        if ( dst_offset > std::numeric_limits<size_t>::max() - size ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": numerical overflow while computing dst_offset + size = "
                    << dst_offset << " + " << size << " > SIZE_MAX" );
            lpf_abort(m_ctx);
        }

        if ( m_active_regs.find( src_slot ) == m_active_regs.end() ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": source memory slot does not exist" );
            lpf_abort(m_ctx);
        }

        if ( m_active_regs.find( dst_slot ) == m_active_regs.end() ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": destination memory slot does not exist" );
            lpf_abort(m_ctx);
        }

        const Memslot & ss = m_memreg.lookup( src_slot );
        const Memslot & ds = m_memreg.lookup( dst_slot );

        if ( ! ss.active ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": source memory slot was not yet active, "
                       " i.e., a preceding lpf_sync was missing."
                       " A synchronisation is necessary to active"
                       " the memory registration at "
                    << ss.file << ":" << ss.line );
            lpf_abort(m_ctx);
        }

        if ( ss.kind == Memslot::Local && ss.size[0] < src_offset + size ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": source memory (locally registered at "
                    << ss.file << ":" << ss.line
                    << " ) is read past the end by "
                    << ( src_offset + size - ss.size[0] ) << " bytes. ");
            lpf_abort(m_ctx);
        }

        if ( ss.kind == Memslot::Global && ss.size[m_pid] < src_offset + size ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": source memory (globally registered at "
                    << ss.file << ":" << ss.line
                    << " ) is read past the end by "
                    << ( src_offset + size - ss.size[m_pid] ) << " bytes");
            lpf_abort(m_ctx);
        }

        if ( ! ds.active ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": destination memory slot was not yet "
                       "active, i.e., a preceding lpf_sync was "
                       "missing. A synchronisation is necessary "
                       "to active the memory registration at "
                    << ds.file << ":" << ds.line );
            lpf_abort(m_ctx);
        }

        if ( ds.kind == Memslot::Local ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": destination memory must be globally registered. "
                    << "Instead, it was only locally registered at "
                    << ds.file << ":" << ds.line );
            lpf_abort(m_ctx);
        }

        if ( ds.kind == Memslot::Global && ds.size[dst_pid] != size_t(-1)
               && ds.size[dst_pid] < dst_offset + size ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": destination memory (globally registered at "
                    << ds.file << ":" << ds.line
                    << " ) is written past the end by "
                    << ( dst_offset + size - ds.size[dst_pid] ) << " bytes");
            lpf_abort(m_ctx);
        }

        if ( m_puts.size() + m_gets.size() >= m_mesgq_reserved ) {
            std::ostringstream t;
            print_request_queue( t );
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": This is the " << (m_puts.size() + m_gets.size() + 1)
                    << "-th message, while space for only " << m_mesgq_reserved
                    << " has been reserved. Request queue follows\n" << t.str() );
            lpf_abort(m_ctx);
        }

        m_used_regs[src_slot] = std::make_pair( file, line );
        m_used_regs[dst_slot] = std::make_pair( file, line );

        Put put = { file, line, dst_pid,
            src_slot, dst_slot, src_offset, dst_offset, size, attr};
        m_puts.push_back( put );

        return LPF_SUCCESS;
    }

    lpf_err_t get( const char * file, int line,
               lpf_pid_t src_pid, lpf_memslot_t src_slot, size_t src_offset,
               lpf_memslot_t dst_slot, size_t dst_offset,
                size_t size, lpf_msg_attr_t attr = LPF_MSG_DEFAULT )
    {
        LPFLIB_IGNORE_TAUTOLOGIES
        if ( src_pid < 0 || src_pid >= m_nprocs ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": unknown process ID " << src_pid << " for data source" );
            lpf_abort(m_ctx);
        }
        LPFLIB_RESTORE_WARNINGS

        if ( src_offset > std::numeric_limits<size_t>::max() - size ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": numerical overflow while computing src_offset + size = "
                    << src_offset << " + " << size << " > SIZE_MAX" );
            lpf_abort(m_ctx);
        }

        if ( dst_offset > std::numeric_limits<size_t>::max() - size ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": numerical overflow while computing dst_offset + size = "
                    << dst_offset << " + " << size << " > SIZE_MAX" );
            lpf_abort(m_ctx);
        }

        if ( m_active_regs.find( src_slot ) == m_active_regs.end() ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": source memory slot does not exist" );
            lpf_abort(m_ctx);
        }

        if ( m_active_regs.find( dst_slot ) == m_active_regs.end() ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": destination memory slot does not exist" );
            lpf_abort(m_ctx);
        }

        const Memslot & ss = m_memreg.lookup( src_slot );
        const Memslot & ds = m_memreg.lookup( dst_slot );

        if ( ! ss.active ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": source memory slot was not yet active, "
                       " i.e., a preceding lpf_sync was missing."
                       " A synchronisation is necessary to active"
                       " the memory registration at "
                    << ss.file << ":" << ss.line );
            lpf_abort(m_ctx);
        }

        if ( ss.kind == Memslot::Local ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": source memory must be globally registered. "
                       "Instead, it was registered only locally at "
                      << ss.file << ":" << ss.line );

            lpf_abort(m_ctx);
        }

        if ( ss.kind == Memslot::Global && ss.size[src_pid] != size_t(-1)
                && ss.size[src_pid] < src_offset + size ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": source memory (globally registered at "
                    << ss.file << ":" << ss.line
                    << " ) is read past the end by "
                    << ( src_offset + size - ss.size[src_pid] ) << " bytes");
            lpf_abort(m_ctx);
        }

        if ( ! ds.active ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": destination memory slot was not yet"
                       "active, i.e., a preceding lpf_sync was"
                       "missing. A synchronisation is necessary"
                       "to active the memory registration at "
                    << ds.file << ":" << ds.line );
            lpf_abort(m_ctx);
        }

        if ( ds.kind == Memslot::Local && ds.size[0] < dst_offset + size ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": destination memory (locally registered at "
                    << ds.file << ":" << ds.line
                    << " ) is written past the end by "
                    << ( dst_offset + size - ds.size[0] ) << " bytes");
            lpf_abort(m_ctx);
        }

        if ( ds.kind == Memslot::Global && ds.size[m_pid] < dst_offset + size ) {
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": destination memory (globally registered at "
                    << ds.file << ":" << ds.line
                    << " ) is written past the end by "
                    << ( dst_offset + size - ds.size[m_pid] ) << " bytes");
            lpf_abort(m_ctx);
        }

        if ( m_puts.size() + m_gets.size() >= m_mesgq_reserved ) {
            std::ostringstream t;
            print_request_queue( t );
            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": This is the " << (m_puts.size() + m_gets.size() + 1)
                    << "-th message, while space for only " << m_mesgq_reserved
                    << " has been reserved. Request queue follows\n" << t.str() );
            lpf_abort(m_ctx);
        }

        m_used_regs[src_slot] = std::make_pair( file, line );
        m_used_regs[dst_slot] = std::make_pair( file, line );

        Get get = { file, line, src_pid,
            src_slot, dst_slot, src_offset, dst_offset, size, attr};
        m_gets.push_back( get );

        return LPF_SUCCESS;
    }

    lpf_err_t abort(const char * file, int line) {
        (void) file;
        (void) line;
        lpf_abort(m_ctx);
        return LPF_SUCCESS;
    }

    lpf_err_t sync( const char * file, int line,
            lpf_sync_attr_t attr = LPF_SYNC_DEFAULT )
    {
        if (! m_comm_bufs_registered) {
            // During the very first sync we know that no other memory areas
            // have been registered, because a sync is required to effectuate
            // a resize of the memory register, which has 0 as initial size.
            ASSERT( m_mesgq_reserved == 0);
            if ( m_mesgq_reserved_next == 0 ) {
                if (lpf_resize_message_queue( m_ctx, 10*m_nprocs) != LPF_SUCCESS ) {
                    LOG( 0, file << ":" << line << ": pid " << m_pid
                            << ": Could not allocate extra debug messages in message queue"
                       );
                    lpf_abort(m_ctx);
                }
            }

            ASSERT( m_memreg_reserved == 0);
            if ( m_memreg_reserved_next == 0) {
                if ( lpf_resize_memory_register( m_ctx, INTERNAL_MEMREGS ) != LPF_SUCCESS ) {
                    LOG( 0, file << ":" << line << ": pid " << m_pid
                           << ": Could not allocate extra debug memory slots in memory registration table"
                       );
                    lpf_abort(m_ctx);
                }
            }

            if ( lpf_sync( m_ctx, LPF_SYNC_DEFAULT ) != LPF_SUCCESS )
                return LPF_ERR_FATAL;

            reg_comm_bufs();

            //NOTE: unnecessary because lpf_sync is called below as well,
            //      before any use of the registered slots, but we're being
            //      defensive here
            if ( lpf_sync( m_ctx, LPF_SYNC_DEFAULT ) != LPF_SUCCESS )
                return LPF_ERR_FATAL;
        }

        const size_t max_h = m_mesgq_reserved;
        // communicate state of allocated buffers
        m_memreg_reserved_all[ m_pid ] = m_memreg_reserved_next;
        m_mesgq_reserved_all[ m_pid ] = m_mesgq_reserved_next;
        m_resized = false;
        if ( LPF_SUCCESS != lpf_sync( m_ctx, LPF_SYNC_DEFAULT ) )
            return LPF_ERR_FATAL;

        for (lpf_pid_t p = 0 ; p < m_nprocs ; ++p ) {
            if (p != m_pid ) {
                lpf_put( m_ctx, m_memreg_reserved_all_slot, sizeof(size_t)*m_pid,
                        p, m_memreg_reserved_all_slot, sizeof(size_t)*m_pid,
                        sizeof(size_t), LPF_MSG_DEFAULT );
                lpf_put( m_ctx, m_mesgq_reserved_all_slot, sizeof(size_t)*m_pid,
                        p, m_mesgq_reserved_all_slot, sizeof(size_t)*m_pid,
                        sizeof(size_t), LPF_MSG_DEFAULT );
                lpf_put( m_ctx, m_glob_regs_slot, sizeof(size_t)*m_pid,
                        p, m_glob_regs_slot, sizeof(size_t)*m_pid,
                        sizeof(size_t), LPF_MSG_DEFAULT );
                lpf_put( m_ctx, m_glob_deregs_slot, sizeof(size_t)*m_pid,
                        p, m_glob_deregs_slot, sizeof(size_t)*m_pid,
                        sizeof(size_t), LPF_MSG_DEFAULT );
            }
            if (m_resized_by_me) {
                lpf_put( m_ctx, m_resized_by_me_slot, 0,
                        p, m_resized_slot, 0, sizeof(m_resized),
                        LPF_MSG_DEFAULT );
            }
        }
        if ( LPF_SUCCESS != lpf_sync( m_ctx, LPF_SYNC_DEFAULT ) )
            return LPF_ERR_FATAL;

        m_memreg_reserved = *std::min_element( m_memreg_reserved_all.begin(), m_memreg_reserved_all.end() );
        m_mesgq_reserved = *std::min_element( m_mesgq_reserved_all.begin(), m_mesgq_reserved_all.end() );

        // check number of global registrations and deregistrations
        size_t globregs = m_glob_regs[m_pid];
        size_t globderegs = m_glob_deregs[m_pid];
        for ( lpf_pid_t p = 0 ; p < m_nprocs; ++p ) {
            if (globregs != m_glob_regs[p] ) {
                LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": Number of global registrations does not match."
                    " I have " << globregs << ", while pid " << p <<
                    " has " << m_glob_regs[p] << " global registrations" );
                lpf_abort(m_ctx);
            }

            if (globderegs != m_glob_deregs[p] ) {
                LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": Number of deregistrations of global slots does not match."
                    " I have " << globderegs << ", while pid " << p <<
                    " has " << m_glob_deregs[p] << " deregistrations" );
                lpf_abort(m_ctx);
            }
        }

        ASSERT( globderegs == m_new_global_deregs.size() );

        // iterate over global deregistrations: check slot numbers
        // and deregister
        for ( size_t i = 0; i < globderegs; ++i ) {

            m_glob_slots[ m_pid ] = m_new_global_deregs[ i ];

            if (LPF_SUCCESS != lpf_sync( m_ctx, LPF_SYNC_DEFAULT ) )
                return LPF_ERR_FATAL;

            // allgather
            for (lpf_pid_t p = 0 ; p < m_nprocs ; ++p )
                if (p != m_pid ) {
                    lpf_put( m_ctx, m_glob_slots_slot, sizeof(lpf_memslot_t)*m_pid,
                            p, m_glob_slots_slot, sizeof(lpf_memslot_t)*m_pid,
                            sizeof(lpf_memslot_t), LPF_MSG_DEFAULT );
                }

            if (LPF_SUCCESS != lpf_sync( m_ctx, LPF_SYNC_DEFAULT ) )
                return LPF_ERR_FATAL;

            for (lpf_pid_t p = 0; p < m_nprocs; ++p ) {
                if ( m_glob_slots[ m_pid ] != m_glob_slots[p] ) {
                    LOG( 0, file << ":" << line << ": pid " << m_pid
                            << ": the " << i << "-th global deregistration mismatches "
                            " on pid " << p << " and " << m_pid );
                    lpf_abort(m_ctx);
                }
            }

            // slot number of this deregistrations are consistent, so we
            // can remove slot
            Memslot & slot = m_memreg.update( m_new_global_deregs[i] );
            ASSERT( slot.kind == Memslot :: Global );
            lpf_deregister( m_ctx, slot.slot );
            m_memreg.removeReg( m_new_global_deregs[i] );
        }
        m_new_global_deregs.clear();

        ASSERT( globregs == m_new_global_regs.size() );
        // now iterate over all global registrations and register memory
        // areas and exchange sizes
        for ( Regs::const_iterator reg = m_new_global_regs.begin();
                reg != m_new_global_regs.end(); ++reg)
        {
            ASSERT( m_active_regs.find( *reg ) != m_active_regs.end() );
            Memslot & slot = m_memreg.update(*reg) ;
            ASSERT( slot.kind == Memslot ::Global );
            ASSERT( slot.slot == LPF_INVALID_MEMSLOT );

            m_glob_regs[ m_pid ] = slot.size[ m_pid ];
            m_glob_slots[ m_pid ] = *reg;

            if (LPF_SUCCESS != lpf_sync( m_ctx, LPF_SYNC_DEFAULT ) )
                return LPF_ERR_FATAL;

            // allgather
            for (lpf_pid_t p = 0 ; p < m_nprocs ; ++p )
                if (p != m_pid ) {
                    lpf_put( m_ctx, m_glob_regs_slot, sizeof(size_t)*m_pid,
                            p, m_glob_regs_slot, sizeof(size_t)*m_pid,
                            sizeof(size_t), LPF_MSG_DEFAULT );
                    lpf_put( m_ctx, m_glob_slots_slot, sizeof(lpf_memslot_t)*m_pid,
                            p, m_glob_slots_slot, sizeof(lpf_memslot_t)*m_pid,
                            sizeof(lpf_memslot_t), LPF_MSG_DEFAULT );
                }

            if (LPF_SUCCESS != lpf_sync( m_ctx, LPF_SYNC_DEFAULT ) )
                return LPF_ERR_FATAL;

            for (lpf_pid_t p = 0; p < m_nprocs; ++p ) {
                slot.size[ p ] = m_glob_regs[p];

                if ( m_glob_slots[ m_pid ] != m_glob_slots[ p ] ) {
                    LOG( 0, slot.file << ":" << slot.line << ": pid " << m_pid
                            << ": This global registration got messed up "
                               "somehow (other confused pid " << p << " )"
                               ", which makes me think that this is "
                               "an internal error in the debug layer. Sorry!");
                    lpf_abort(m_ctx);
                }
            }

            // everything was OK! So, we can safely register
            lpf_register_global( m_ctx,
                    slot.pointer, slot.size[m_pid], & slot.slot );
            slot.active = true;
        }
        m_new_global_regs.clear();
        std::fill( m_glob_regs.begin(), m_glob_regs.end(), 0u);
        std::fill( m_glob_deregs.begin(), m_glob_deregs.end(), 0u);

        //activate new registrations
        if (LPF_SUCCESS != lpf_sync( m_ctx, LPF_SYNC_DEFAULT ) )
            return LPF_ERR_FATAL;

        // exchange max h & prepare for read-write conflict detection
        //
        // - m_max_out => Number of outbound requests (per destination pid)
        // - m_max_in  => Number of incoming requests (per origin pid)
        // - m_remote_access_by_me_offsets_local
        //             => The offsets into the local m_remote_access_by_me array
        // - m_remote_access_by_me_offsets_local
        //             => The offsets into the remote m_remote_access_by_me
        //                array.
        // - m_local_access_by_remote_offsets
        //             => The offsets into the local m_local_access_by_remote
        //                array
        std::fill( m_max_out.begin(), m_max_out.end(), 0u );
        std::fill( m_max_in.begin(), m_max_in.end(), 0u );
        for ( size_t i = 0; i < m_puts.size(); ++i ) {
            m_max_out[ m_puts[i].dstPid ] += 1;
        }
        for ( size_t i = 0; i < m_gets.size(); ++i ) {
            m_max_out[ m_gets[i].srcPid ] += 1;
        }

        if (LPF_SUCCESS != lpf_sync( m_ctx, LPF_SYNC_DEFAULT ) )
            return LPF_ERR_FATAL;


        // all-to-all and compute offsets in local buffers
        size_t offset= 0;
        for (lpf_pid_t p = 0; p < m_nprocs; ++p )  {
            m_remote_access_by_me_offsets_local[ p ] = offset;
            offset += m_max_out[p];

            lpf_put( m_ctx, m_max_out_slot, sizeof(size_t)*p,
                    p, m_max_in_slot, sizeof(size_t)*m_pid,
                    sizeof(size_t), LPF_MSG_DEFAULT);
        }
        m_remote_access_by_me_offsets_local[ m_nprocs ] = offset;

        if (LPF_SUCCESS != lpf_sync( m_ctx, LPF_SYNC_DEFAULT ) )
            return LPF_ERR_FATAL;

        // count incoming requests. Also communicate the offsets into
        // the m_local_access_by_remote array
        offset = 0;
        for (lpf_pid_t p = 0; p < m_nprocs; ++p )  {
            m_local_access_by_remote_offsets[p] = offset;
            offset += m_max_in[p];

            lpf_put( m_ctx, m_local_access_by_remote_offsets_slot, sizeof(size_t)*p,
                    p, m_remote_access_by_me_offsets_remote_slot, sizeof(size_t)*m_pid,
                    sizeof(size_t), LPF_MSG_DEFAULT );
        }
        m_local_access_by_remote_offsets[m_nprocs] = offset;

        if (LPF_SUCCESS != lpf_sync( m_ctx, LPF_SYNC_DEFAULT ) )
            return LPF_ERR_FATAL;

        // check whether the number of incoming requests is OK
        const size_t total_reqs = offset + m_puts.size() + m_gets.size();
        if ( total_reqs > max_h ) {
            std::stringstream s, t;

            for (lpf_pid_t p = 0; p < m_nprocs; ++p) {
                s << m_max_in[p] << ' ';
            }

            print_request_queue( t );

            LOG( 0, file << ":" << line << ": pid " << m_pid
                    << ": Too many messages on pid " << m_pid
              << ". Reserved was " << max_h << " while there were actually "
             << total_reqs << " in total. "
                "Incoming requests from PIDs 0.." << m_nprocs << " = " << s.str()
                << ". Local request queue follows (" << m_puts.size() << " puts "
                << "and " << m_gets.size() << " gets )\n" << t.str() );
            lpf_abort(m_ctx);
        }

        // reallocate access buffers if they were resized.
        if (m_resized) { // (m_resized is the result of a global OR)
            // reregister
            if ( LPF_INVALID_MEMSLOT != m_remote_access_by_me_slot )
                lpf_deregister( m_ctx, m_remote_access_by_me_slot );

            if ( LPF_INVALID_MEMSLOT != m_local_access_by_remote_slot )
                lpf_deregister( m_ctx, m_local_access_by_remote_slot );

            lpf_register_global( m_ctx, m_remote_access_by_me.data(),
                    m_remote_access_by_me.capacity() * sizeof(Access),
                    & m_remote_access_by_me_slot );

            lpf_register_global( m_ctx, m_local_access_by_remote.data(),
                    m_local_access_by_remote.capacity() * sizeof(Access),
                    & m_local_access_by_remote_slot );

            if (LPF_SUCCESS != lpf_sync( m_ctx, LPF_SYNC_DEFAULT ) )
                return LPF_ERR_FATAL;
        }

        if (max_h > 0) {
            m_remote_access_by_me.resize( max_h );
            m_local_access_by_remote.resize( max_h );

            // Check for read-write conflicts & read/write bounds
            for (size_t i = 0; i < m_puts.size(); ++i) {
                Put put = m_puts[i];
                ASSERT( m_active_regs.find( put.dstSlot ) != m_active_regs.end() );
                ASSERT( m_active_regs.find( put.srcSlot ) != m_active_regs.end() );
                const Memslot & ss = m_memreg.lookup( put.srcSlot );
                const Memslot & ds = m_memreg.lookup( put.dstSlot );
                ASSERT( ds.kind == Memslot::Global );

                if ( ds.size[put.dstPid] < put.dstOffset + put.size ) {
                    LOG( 0, put.file << ":" << put.line << ": pid " << m_pid
                            << ": destination memory (globally registered at "
                            << ds.file << ":" << ds.line
                            << " ) is written past the end by "
                            << ( put.dstOffset + put.size - ds.size[m_pid] )
                            << " bytes");
                    lpf_abort(m_ctx);
                }

                m_rwconflict.insertRead(
                        static_cast<char *>( ss.pointer ) + put.srcOffset,
                        put.size
                );

                size_t & index = m_remote_access_by_me_offsets_local[ put.dstPid ];
                Access a = { Access::WRITE, put.dstSlot, put.dstOffset, put.size };
                m_remote_access_by_me[ index ] = a;
                index += 1; // index is an alias of the array element
            }

            for (size_t i = 0; i < m_gets.size(); ++i) {
                Get get = m_gets[i];
                ASSERT( m_active_regs.find( get.dstSlot ) != m_active_regs.end() );
                ASSERT( m_active_regs.find( get.srcSlot ) != m_active_regs.end() );
                const Memslot & ss = m_memreg.lookup( get.srcSlot );
                ASSERT( ss.kind == Memslot::Global );

                if ( ss.size[get.srcPid] < get.srcOffset + get.size ) {
                    LOG( 0, get.file << ":" << get.line << ": pid " << m_pid
                            << ": source memory (globally registered "
                            << ss.file << ":" << ss.line
                            << " ) is read past the end by "
                            << ( get.srcOffset + get.size - ss.size[m_pid] )
                            << " bytes");
                    lpf_abort(m_ctx);
                }

                size_t & index = m_remote_access_by_me_offsets_local[ get.srcPid ];
                Access a = { Access::READ, get.srcSlot, get.srcOffset, get.size };
                m_remote_access_by_me[ index ] = a;
                index += 1;
            }

            // all-to-all
            for ( lpf_pid_t p = 0 ; p < m_nprocs; ++p) {
                lpf_put( m_ctx, m_remote_access_by_me_slot,
                        sizeof(Access) * (p?m_remote_access_by_me_offsets_local[p-1]:0),
                        p, m_local_access_by_remote_slot,
                        sizeof(Access) * m_remote_access_by_me_offsets_remote[p],
                        sizeof(Access) * m_max_out[p],
                        LPF_MSG_DEFAULT );
            }

            if (LPF_SUCCESS != lpf_sync( m_ctx, LPF_SYNC_DEFAULT ) )
                return LPF_ERR_FATAL;

            // now we have all requests in the m_local_access_by_remote array
            // Now also enter the read requests by lpf_gets
            const size_t n_remote_requests = m_local_access_by_remote_offsets[m_nprocs];
            for ( size_t i = 0; i < n_remote_requests; ++i ) {
                Access & a = m_local_access_by_remote[i];
                if ( a.access == Access::READ ) {
                    char * ptr = static_cast<char *>(m_memreg.lookup(a.slot).pointer);
                    m_rwconflict.insertRead( ptr + a.offset, a.size );
                }
            }

            // Now we can check all writes for read-write conflicts
            for ( size_t i = 0; i < n_remote_requests; ++i ) {
                Access & a = m_local_access_by_remote[i];
                if ( a.access == Access::WRITE ) {
                    const Memslot & ds = m_memreg.lookup(a.slot);
                    char * ptr = static_cast<char *>(ds.pointer);
                    if ( m_rwconflict.checkWrite( ptr + a.offset, a.size ) ) {
                        std::ostringstream s;
                        m_rwconflict.printReadMap(s);

                        LOG( 0, file << ":" << line << ": pid " << m_pid
                                << " Read-write conflict detected. The write"
                                " is caused by a lpf_put from a remote process"
                                " and is targeted at a memory area registered on "
                                << ds.file << ":" << ds.line
                                << "\nViolating write is ["
                                << static_cast<void *>(ptr+a.offset)
                                << ","
                                << static_cast<void *>(ptr+a.offset+a.size)
                                << "); Reads are " << s.str() ;
                           );
                        lpf_abort(m_ctx);
                    }
                }
            }

            for ( size_t i = 0; i < m_gets.size(); ++i) {
                Get get = m_gets[i];
                const Memslot & ds = m_memreg.lookup( get.dstSlot );
                char * ptr = static_cast<char *>(ds.pointer);
                if ( m_rwconflict.checkWrite( ptr + get.dstOffset, get.size ) ) {
                    std::ostringstream s;
                    m_rwconflict.printReadMap(s);

                    LOG( 0, file << ":" << line << ": pid " << m_pid
                            << " Read-write conflict detected. The write "
                            " is caused by a lpf_get from this process: "
                            << get.file << ":" << get.line << " on a memory "
                            "area registered on " << ds.file << ":" << ds.line
                            << "\nViolating write is ["
                            << static_cast<void *>(ptr+get.dstOffset)
                            << ","
                            << static_cast<void *>(ptr+get.dstOffset+get.size)
                            << "); Reads are " << s.str() ;
                       );
                    lpf_abort(m_ctx);
                }
            }

            // now we can safely execute the puts and gets
            for (size_t i = 0; i < m_puts.size(); ++i) {
                Put put = m_puts[i];
                const Memslot & ss = m_memreg.lookup( put.srcSlot );
                const Memslot & ds = m_memreg.lookup( put.dstSlot );

                lpf_put( m_ctx, ss.slot, put.srcOffset,
                        put.dstPid, ds.slot, put.dstOffset,
                        put.size, put.attr );
            }
            m_puts.clear();

            for (size_t i = 0; i < m_gets.size(); ++i) {
                Get get = m_gets[i];
                const Memslot & ss = m_memreg.lookup( get.srcSlot );
                const Memslot & ds = m_memreg.lookup( get.dstSlot );

                lpf_get( m_ctx, get.srcPid, ss.slot, get.srcOffset,
                        ds.slot, get.dstOffset, get.size, get.attr );
            }
            m_gets.clear();
            m_used_regs.clear();
            m_rwconflict.clear();
        }
        ASSERT( m_puts.empty() );
        ASSERT( m_gets.empty() );
        ASSERT( m_used_regs.empty() );
        ASSERT( m_rwconflict.empty() );

        // and do the final sync
        return lpf_sync( m_ctx, attr );
    }

private:



    struct Memslot {
        const char * file; int line;

        void *                 pointer;
        lpf_memslot_t          slot;
        enum { Local, Global } kind;
        std::vector< size_t >  size;
        bool                   active;
    };

    typedef CombinedMemoryRegister< Memslot > Memreg;

#if __cplusplus >= 201103L
    typedef std::unordered_set< lpf_memslot_t > Regs;
#else
    typedef std::tr1::unordered_set< lpf_memslot_t > Regs;
#endif

#if __cplusplus >= 201103L
    typedef std::unordered_map< lpf_memslot_t,
            std::pair< const char *, int > > UsedRegs;
#else
    typedef std::tr1::unordered_map< lpf_memslot_t,
            std::pair< const char *, int > > UsedRegs;
#endif

    struct Put {
        const char * file; int line;

        lpf_pid_t dstPid;
        lpf_memslot_t srcSlot, dstSlot;
        size_t srcOffset, dstOffset;
        size_t size ;
        lpf_msg_attr_t attr;
    };

    struct Get {
        const char * file; int line;

        lpf_pid_t srcPid;
        lpf_memslot_t srcSlot, dstSlot;
        size_t srcOffset, dstOffset;
        size_t size;
        lpf_msg_attr_t attr;
    };

    struct Access {
        enum { READ, WRITE } access;
        lpf_memslot_t slot;
        size_t offset, size;
    };

    static const size_t INTERNAL_MEMREGS = 15;


    const lpf_t  m_ctx;
    const lpf_pid_t m_pid, m_nprocs;
    Memreg m_memreg;
    size_t m_mesgq_reserved;
    size_t m_mesgq_reserved_next;
    size_t m_memreg_size;
    size_t m_memreg_reserved;
    size_t m_memreg_reserved_next;
    Regs m_active_regs;
    Regs m_new_global_regs;
    std::vector< lpf_memslot_t > m_new_global_deregs;
    UsedRegs m_used_regs;
    ReadWriteConflict m_rwconflict;

    bool m_comm_bufs_registered;
    std::vector< size_t > m_memreg_reserved_all;
    std::vector< size_t > m_mesgq_reserved_all;
    std::vector< size_t > m_glob_regs;
    std::vector< size_t > m_glob_deregs;
    std::vector< lpf_memslot_t > m_glob_slots;
    std::vector< size_t > m_max_out;
    std::vector< size_t > m_max_in;
    std::vector< Access > m_remote_access_by_me;
    std::vector< size_t > m_remote_access_by_me_offsets_remote;
    std::vector< size_t > m_remote_access_by_me_offsets_local;
    std::vector< Access > m_local_access_by_remote;
    std::vector< size_t > m_local_access_by_remote_offsets;
    bool                  m_resized_by_me;
    bool                  m_resized;
    lpf_memslot_t m_memreg_reserved_all_slot;
    lpf_memslot_t m_mesgq_reserved_all_slot;
    lpf_memslot_t m_glob_regs_slot;
    lpf_memslot_t m_glob_deregs_slot;
    lpf_memslot_t m_glob_slots_slot;
    lpf_memslot_t m_max_out_slot;
    lpf_memslot_t m_max_in_slot;
    lpf_memslot_t m_remote_access_by_me_slot;
    lpf_memslot_t m_remote_access_by_me_offsets_remote_slot;
    lpf_memslot_t m_remote_access_by_me_offsets_local_slot;
    lpf_memslot_t m_local_access_by_remote_slot;
    lpf_memslot_t m_local_access_by_remote_offsets_slot;
    lpf_memslot_t m_resized_by_me_slot;
    lpf_memslot_t m_resized_slot;

    std::vector< Put > m_puts;
    std::vector< Get > m_gets;
};

const size_t Interface :: INTERNAL_MEMREGS;

pthread_once_t Interface::s_threadInit= PTHREAD_ONCE_INIT;
pthread_key_t Interface::s_threadKeyCtxStore;


extern "C" {


extern _LPFLIB_API
lpf_err_t lpf_debug_exec( const char * file, int line,
    lpf_t ctx, lpf_pid_t P, lpf_spmd_t spmd, lpf_args_t args
)
{ return Interface::lookupCtx( file, line, ctx )->exec( file, line, P, spmd, args );
}

extern _LPFLIB_API
lpf_err_t lpf_debug_hook( const char * file, int line,
    lpf_init_t init, lpf_spmd_t spmd, lpf_args_t args
)
{ return Interface::hook( file, line, init, spmd, args ); }


extern _LPFLIB_API
lpf_err_t lpf_debug_rehook( const char * file, int line,
    lpf_t ctx,
    lpf_spmd_t spmd,
    lpf_args_t args
)
{ return Interface::lookupCtx( file, line, ctx )->rehook( file, line, spmd, args ); }

extern _LPFLIB_API
lpf_err_t lpf_debug_register_global( const char * file, int line,
    lpf_t ctx, void * pointer, size_t size, lpf_memslot_t * memslot
)
{ return Interface::lookupCtx( file, line, ctx )->register_global( file, line, pointer, size, memslot); }

extern _LPFLIB_API
lpf_err_t lpf_debug_register_local( const char * file, int line,
    lpf_t ctx, void * pointer, size_t size, lpf_memslot_t * memslot
)
{ return Interface::lookupCtx( file, line, ctx )->register_local( file, line, pointer, size, memslot); }

extern _LPFLIB_API
lpf_err_t lpf_debug_abort( const char * file, int line, lpf_t ctx)
{ return Interface::lookupCtx( file, line, ctx )->abort( file, line); }

extern _LPFLIB_API
lpf_err_t lpf_debug_deregister( const char * file, int line,
    lpf_t ctx, lpf_memslot_t memslot
)
{ return Interface::lookupCtx( file, line, ctx )->deregister( file, line, memslot ); }

extern _LPFLIB_API
lpf_err_t lpf_debug_put( const char * file, int line,
    lpf_t ctx, lpf_memslot_t src_slot, size_t src_offset,
    lpf_pid_t dst_pid, lpf_memslot_t dst_slot, size_t dst_offset,
    size_t size, lpf_msg_attr_t attr
)
{ return Interface::lookupCtx( file, line, ctx )->put( file, line, src_slot, src_offset,
        dst_pid, dst_slot, dst_offset, size, attr );
}

extern _LPFLIB_API
lpf_err_t lpf_debug_get( const char * file, int line,
    lpf_t ctx, lpf_pid_t src_pid, lpf_memslot_t src_slot, size_t src_offset,
    lpf_memslot_t dst_slot, size_t dst_offset, size_t size, lpf_msg_attr_t attr
)
{ return Interface::lookupCtx( file, line, ctx )->get( file, line, src_pid, src_slot, src_offset,
        dst_slot, dst_offset, size, attr);
}

extern _LPFLIB_API
lpf_err_t lpf_debug_probe( const char * file, int line,
        lpf_t ctx, lpf_machine_t * params )
{ return Interface::lookupCtx( file, line, ctx )->probe( file, line, params );
}


extern _LPFLIB_API
lpf_err_t lpf_debug_resize_memory_register( const char * file, int line,
        lpf_t ctx, size_t max_regs )
{ return Interface::lookupCtx( file, line, ctx )->resize_memory_register( file, line, max_regs ); }

extern _LPFLIB_API
lpf_err_t lpf_debug_resize_message_queue( const char * file, int line,
        lpf_t ctx, size_t max_msgs )
{ return Interface::lookupCtx( file, line, ctx )->resize_message_queue( file, line, max_msgs); }

extern _LPFLIB_API
lpf_err_t lpf_debug_sync( const char * file, int line,
        lpf_t ctx, lpf_sync_attr_t attr )
{ return Interface::lookupCtx( file, line, ctx )->sync( file, line, attr ); }

} // extern "C"



} }


