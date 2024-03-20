
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

#ifndef LPF_CORE_HYBRID_DISPATCH_HPP
#define LPF_CORE_HYBRID_DISPATCH_HPP

#undef LPFLIB_CORE_H
#define LPF_CORE_STATIC_DISPATCH
#define LPF_CORE_STATIC_DISPATCH_ID pthread
#define LPF_CORE_STATIC_DISPATCH_CONFIG LPF_CORE_IMPL_CONFIG
#include <lpf/core.h>
#undef LPF_CORE_STATIC_DISPATCH_ID
#undef LPF_CORE_STATIC_DISPATCH_CONFIG

#undef LPFLIB_CORE_H
#define LPF_CORE_STATIC_DISPATCH_ID LPF_CORE_MULTI_NODE_ENGINE
#define LPF_CORE_STATIC_DISPATCH_CONFIG LPF_CORE_IMPL_CONFIG
#include <lpf/core.h>
#undef LPF_CORE_STATIC_DISPATCH_ID
#undef LPF_CORE_STATIC_DISPATCH_CONFIG

#undef LPFLIB_CORE_H
#undef LPF_CORE_STATIC_DISPATCH
#include <lpf/core.h>

#define USE_THREAD( symbol ) \
       LPF_RENAME_PRIMITIVE4( lpf, pthread, LPF_CORE_IMPL_CONFIG, symbol )

#define USE_MPI( symbol ) \
    LPF_RENAME_PRIMITIVE4( lpf, LPF_CORE_MULTI_NODE_ENGINE, LPF_CORE_IMPL_CONFIG, symbol )

#define GET_THREAD( symbol ) \
       LPF_RENAME_PRIMITIVE4( LPF, pthread, LPF_CORE_IMPL_CONFIG, symbol )

#define GET_MPI( symbol ) \
    LPF_RENAME_PRIMITIVE4( LPF, LPF_CORE_MULTI_NODE_ENGINE, LPF_CORE_IMPL_CONFIG, symbol )

#include <vector>
#include <algorithm>
#include <numeric>


#include "assert.hpp"
#include "linkage.hpp"

namespace lpf { namespace hybrid {

    class _LPFLIB_LOCAL Thread {
    public:
        typedef USE_THREAD( err_t ) err_t;
        typedef USE_THREAD( args_t ) args_t;
        typedef USE_THREAD( sync_attr_t) sync_attr_t;
        typedef USE_THREAD( msg_attr_t) msg_attr_t;
        typedef USE_THREAD( pid_t ) pid_t;
        typedef USE_THREAD( memslot_t) memslot_t;
        typedef USE_THREAD( _t ) ctx_t;
        typedef USE_THREAD( init_t ) init_t;
        typedef USE_THREAD( machine_t) machine_t;
        typedef USE_THREAD( spmd_t ) spmd_t;

        static const err_t SUCCESS;
        static const err_t ERR_OUT_OF_MEMORY;
        static const err_t ERR_FATAL;

        static const args_t NO_ARGS;
        static const sync_attr_t  SYNC_DEFAULT;
        static const msg_attr_t   MSG_DEFAULT;
        static const pid_t  MAX_P;

        static const memslot_t INVALID_MEMSLOT;
        static const machine_t INVALID_MACHINE;
        static const ctx_t NONE;
        static const init_t INIT_NONE;
        static const ctx_t & ROOT;

        Thread( ctx_t ctx, pid_t pid, pid_t nprocs ) 
            : m_ctx( ctx ), m_pid(pid), m_nprocs(nprocs)
        {}

        pid_t pid() const { return m_pid; }
        pid_t nprocs() const { return m_nprocs; }

        err_t hook( init_t init, spmd_t spmd, args_t args) 
        { return USE_THREAD(hook)(init, spmd, args); }

        err_t rehook( spmd_t spmd, args_t args)
        { return USE_THREAD(rehook)(m_ctx, spmd, args); }

        err_t exec( pid_t P, spmd_t spmd, args_t args)
        { return USE_THREAD( exec )(m_ctx, P, spmd, args ); }

        err_t register_global( void * pointer, size_t size, memslot_t * memslot)
        { return USE_THREAD( register_global)(m_ctx, pointer, size, memslot ); }

        err_t register_local( void * pointer, size_t size, memslot_t * memslot)
        { return USE_THREAD( register_local)(m_ctx, pointer, size, memslot ); }

        err_t deregister( memslot_t memslot) 
        { return USE_THREAD( deregister)(m_ctx, memslot); }

        err_t get_rcvd_msg_count_per_slot( size_t * rcvd_msgs, lpf_memslot_t slot) 
        { return USE_THREAD( get_rcvd_msg_count_per_slot)(m_ctx, rcvd_msgs, slot); }

        err_t get_sent_msg_count_per_slot( size_t * sent_msgs, lpf_memslot_t slot) 
        { return USE_THREAD( get_sent_msg_count_per_slot)(m_ctx, sent_msgs, slot); }

        err_t get_rcvd_msg_count( size_t * rcvd_msgs) 
        { return USE_THREAD( get_rcvd_msg_count)(m_ctx, rcvd_msgs); }

        err_t flush_sent()
        { return USE_THREAD(flush_sent)(m_ctx); }

        err_t flush_received()
        { return USE_THREAD(flush_received)(m_ctx); }

        err_t put( memslot_t src_slot, size_t src_offset, 
                pid_t dst_pid, memslot_t dst_slot, size_t dst_offset, 
                size_t size, msg_attr_t attr = MSG_DEFAULT )
        { return USE_THREAD(put)( m_ctx, src_slot, src_offset,
                dst_pid, dst_slot, dst_offset, size, attr); }

        err_t get( pid_t pid, memslot_t src, size_t src_offset, 
                memslot_t dst, memslot_t dst_offset,
                size_t size, msg_attr_t attr = MSG_DEFAULT )
        { return USE_THREAD(get)( m_ctx, pid, src, src_offset, 
                dst, dst_offset, size, attr ); }

        err_t sync( sync_attr_t attr = SYNC_DEFAULT )
        { return USE_THREAD(sync)( m_ctx, attr ); }

        err_t sync_per_slot( sync_attr_t attr = SYNC_DEFAULT, memslot_t slot = LPF_INVALID_MEMSLOT)
        { return USE_THREAD(sync_per_slot)( m_ctx, attr, slot); }

        err_t counting_sync_per_slot( sync_attr_t attr = SYNC_DEFAULT, lpf_memslot_t slot = LPF_INVALID_MEMSLOT, size_t expected_sent = 0, size_t expected_recvd = 0)
        { return USE_THREAD(counting_sync_per_slot)(m_ctx, attr, slot, expected_sent, expected_recvd); }

        err_t probe( machine_t * params )
        { return USE_THREAD(probe)(m_ctx, params ); }

        err_t resize_message_queue( size_t max_msgs )
        { return USE_THREAD(resize_message_queue)(m_ctx, max_msgs); }

        err_t resize_memory_register( size_t max_regs )
        { return USE_THREAD(resize_memory_register)(m_ctx, max_regs); }

        static bool is_linked_correctly() 
        { 
            return SUCCESS != ERR_OUT_OF_MEMORY
                 && SUCCESS != ERR_FATAL
                 && ERR_FATAL != ERR_OUT_OF_MEMORY;
        }

    private:
        ctx_t m_ctx ;
        pid_t m_pid, m_nprocs;        
    };

    class _LPFLIB_LOCAL MPI {
    public:
        typedef USE_MPI( err_t ) err_t;
        typedef USE_MPI( args_t ) args_t;
        typedef USE_MPI( sync_attr_t) sync_attr_t;
        typedef USE_MPI( msg_attr_t) msg_attr_t;
        typedef USE_MPI( pid_t ) pid_t;
        typedef USE_MPI( memslot_t) memslot_t;
        typedef USE_MPI( _t ) ctx_t;
        typedef USE_MPI( init_t ) init_t;
        typedef USE_MPI( machine_t) machine_t;
        typedef USE_MPI( spmd_t ) spmd_t;

        static const err_t SUCCESS;
        static const err_t ERR_OUT_OF_MEMORY;
        static const err_t ERR_FATAL;

        static const args_t NO_ARGS;
        static const sync_attr_t  SYNC_DEFAULT;
        static const msg_attr_t   MSG_DEFAULT;
        static const pid_t  MAX_P;

        static const memslot_t INVALID_MEMSLOT;
        static const machine_t INVALID_MACHINE;
        static const ctx_t NONE;
        static const init_t INIT_NONE;
        static const ctx_t & ROOT;

        MPI( ctx_t ctx, pid_t pid, pid_t nprocs ) 
            : m_ctx( ctx )
            , m_pid(pid), m_nprocs(nprocs)
        {}

        pid_t pid() const { return m_pid; }
        pid_t nprocs() const { return m_nprocs; }

        err_t hook( init_t init, spmd_t spmd, args_t args) 
        { return USE_MPI(hook)( init, spmd, args); }

        err_t rehook( spmd_t spmd, args_t args)
        { return USE_MPI(rehook)(m_ctx, spmd, args); }

        err_t exec( pid_t P, spmd_t spmd, args_t args)
        { return USE_MPI( exec )(m_ctx, P, spmd, args ); }

        err_t register_global( void * pointer, size_t size, memslot_t * memslot)
        { return USE_MPI( register_global)(m_ctx, pointer, size, memslot ); }

        err_t register_local( void * pointer, size_t size, memslot_t * memslot)
        { return USE_MPI( register_local)(m_ctx, pointer, size, memslot ); }

        err_t deregister( memslot_t memslot) 
        { return USE_MPI( deregister)(m_ctx, memslot); }

        err_t get_rcvd_msg_count_per_slot(size_t *rcvd_msgs, lpf_memslot_t slot) 
        { return USE_MPI( get_rcvd_msg_count_per_slot)( m_ctx, rcvd_msgs, slot); }

        err_t get_sent_msg_count_per_slot(size_t *sent_msgs, lpf_memslot_t slot) 
        { return USE_MPI( get_sent_msg_count_per_slot)( m_ctx, sent_msgs, slot); }

        err_t get_rcvd_msg_count( size_t * rcvd_msgs) 
        { return USE_MPI( get_rcvd_msg_count)(m_ctx, rcvd_msgs); }

        err_t flush_sent()
        {return USE_MPI( flush_sent)(m_ctx);}

        err_t flush_received()
        {return USE_MPI( flush_received)(m_ctx);}

        err_t put( memslot_t src_slot, size_t src_offset, 
                pid_t dst_pid, memslot_t dst_slot, size_t dst_offset, 
                size_t size, msg_attr_t attr = MSG_DEFAULT )
        { return USE_MPI(put)( m_ctx, src_slot, src_offset,
                dst_pid, dst_slot, dst_offset, size, attr); }

        err_t get( pid_t pid, memslot_t src, size_t src_offset, 
                memslot_t dst, memslot_t dst_offset,
                size_t size, msg_attr_t attr = MSG_DEFAULT)
        { return USE_MPI(get)( m_ctx, pid, src, src_offset, 
                dst, dst_offset, size, attr ); }

        err_t sync( sync_attr_t attr = SYNC_DEFAULT )
        { return USE_MPI(sync)( m_ctx, attr ); }

        err_t sync_per_slot( sync_attr_t attr = SYNC_DEFAULT, lpf_memslot_t slot = LPF_INVALID_MEMSLOT )
        { return USE_MPI(sync_per_slot)( m_ctx, attr, slot); }

        err_t counting_sync_per_slot( sync_attr_t attr = SYNC_DEFAULT, lpf_memslot_t slot = LPF_INVALID_MEMSLOT, size_t expected_sent = 0, size_t expected_recvd = 0)
        { return USE_MPI(counting_sync_per_slot)(m_ctx, attr, slot, expected_sent, expected_recvd); }

        err_t probe( machine_t * params )
        { return USE_MPI(probe)(m_ctx, params ); }

        err_t resize_message_queue( size_t max_msgs )
        { return USE_MPI(resize_message_queue)(m_ctx, max_msgs); }

        err_t resize_memory_register( size_t max_regs )
        { return USE_MPI(resize_memory_register)(m_ctx, max_regs); }

        static bool is_linked_correctly() 
        { 
            return SUCCESS != ERR_OUT_OF_MEMORY
                 && SUCCESS != ERR_FATAL
                 && ERR_FATAL != ERR_OUT_OF_MEMORY;
        }

    private:
        ctx_t m_ctx ;
        pid_t m_pid, m_nprocs;        
    };


    template <class LPF, typename T>
    typename LPF::err_t broadcast( LPF lpf, typename LPF::pid_t root, T & pod )
    {
        typename LPF::err_t rc = LPF::SUCCESS;
        rc = lpf.resize_memory_register( 1 );
        if (rc != LPF::SUCCESS) return rc;

        rc = lpf.resize_message_queue( lpf.nprocs() );
        if (rc != LPF::SUCCESS) return rc;

        rc = lpf.sync();
        if (rc != LPF::SUCCESS) return rc;

        typename LPF::memslot_t slot = LPF::INVALID_MEMSLOT;
        rc = lpf.register_global( &pod, sizeof(pod), &slot);
        ASSERT( rc == LPF::SUCCESS );

        if (lpf.pid() != root)
            lpf.get( root, slot, 0, slot, 0, sizeof(pod));

        rc = lpf.sync();

        lpf.deregister( slot );
        return rc;
    }

    template <class LPF, typename T>
    typename LPF::err_t 
    broadcast( LPF lpf, typename LPF::pid_t root, std::vector<T> & array )
    {
        typename LPF::err_t rc = LPF::SUCCESS;
        rc = lpf.resize_memory_register( 1 );
        if (rc != LPF::SUCCESS) return rc;

        rc = lpf.resize_message_queue( lpf.nprocs() );
        if (rc != LPF::SUCCESS) return rc;

        rc = lpf.sync();
        if (rc != LPF::SUCCESS) return rc;

        size_t size = array.size();
        typename LPF::memslot_t sizeSlot = LPF::INVALID_MEMSLOT;
        rc = lpf.register_global( &size, sizeof(size), &sizeSlot);
        ASSERT( rc == LPF::SUCCESS );

        if (lpf.pid() != root)
            lpf.get( root, sizeSlot, 0, sizeSlot, 0, sizeof(size));

        rc = lpf.sync();
        if (rc != LPF::SUCCESS) return rc;
        lpf.deregister( sizeSlot );

        array.resize( size );

        typename LPF::memslot_t arraySlot = LPF::INVALID_MEMSLOT;
        rc = lpf.register_global( array.data(), array.size()*sizeof(T), &arraySlot);
        ASSERT( rc == LPF::SUCCESS );

        if ( lpf.pid() != root)
            lpf.get( root, arraySlot, 0, arraySlot, 0, size*sizeof(T));

        rc = lpf.sync();

        lpf.deregister( arraySlot );
        return rc;
    }

    template <class LPF>
    typename LPF::err_t reduceOr( LPF lpf, typename LPF::pid_t root, bool &  pod )
    {
        typename LPF::err_t rc = LPF::SUCCESS;
        rc = lpf.resize_memory_register( 1 );
        if (rc != LPF::SUCCESS) return rc;

        rc = lpf.resize_message_queue( lpf.nprocs() );
        if (rc != LPF::SUCCESS) return rc;

        rc = lpf.sync();
        if (rc != LPF::SUCCESS) return rc;

        typename LPF::memslot_t slot = LPF::INVALID_MEMSLOT;
        rc = lpf.register_global( &pod, sizeof(pod), &slot);
        ASSERT( rc == LPF::SUCCESS );

        if (lpf.pid() != 0 && pod)
            lpf.put( root, slot, 0, slot, 0, sizeof(pod));

        rc = lpf.sync();

        lpf.deregister( slot );
        return rc;
    }

    template <class LPF, typename T>
    typename LPF::err_t 
    gather( LPF lpf, typename LPF::pid_t root, T & pod, std::vector<T> & array )
    {
        try {
            if (lpf.pid() == root)
                array.resize( lpf.nprocs() );
        } 
        catch( std::bad_alloc & e)
        {
            return LPF::ERR_OUT_OF_MEMORY;
        }

        typename LPF::err_t rc = LPF::SUCCESS;
        rc = lpf.resize_memory_register( 2 );
        if (rc != LPF::SUCCESS) return rc;

        rc = lpf.resize_message_queue( lpf.nprocs() );
        if (rc != LPF::SUCCESS) return rc;

        rc = lpf.sync();
        if (rc != LPF::SUCCESS) return rc;

        typename LPF::memslot_t src = LPF::INVALID_MEMSLOT;
        typename LPF::memslot_t dst = LPF::INVALID_MEMSLOT;
        rc = lpf.register_local( &pod, sizeof(T), &src);
        ASSERT( rc == LPF::SUCCESS );
        rc = lpf.register_global( array.data(), array.size()*sizeof(T), &dst);
        ASSERT( rc == LPF::SUCCESS );

        lpf.put( root, src, 0, dst, sizeof(T)*lpf.pid(), sizeof(pod));

        rc = lpf.sync();

        lpf.deregister( src );
        lpf.deregister( dst );
        return rc;
    }

    template <class LPF, typename T>
    typename LPF::err_t 
    gather( LPF lpf, typename LPF::pid_t root, T & pod, T * array, size_t n )
    {
        if ( lpf.pid() == root && n < lpf.nprocs() )
            return LPF::ERR_FATAL;

        typename LPF::err_t rc = LPF::SUCCESS;
        rc = lpf.resize_memory_register( 2 );
        if (rc != LPF::SUCCESS) return rc;

        rc = lpf.resize_message_queue( lpf.nprocs() );
        if (rc != LPF::SUCCESS) return rc;

        rc = lpf.sync();
        if (rc != LPF::SUCCESS) return rc;

        typename LPF::memslot_t src = LPF::INVALID_MEMSLOT;
        typename LPF::memslot_t dst = LPF::INVALID_MEMSLOT;
        rc = lpf.register_local( &pod, sizeof(T), &src);
        ASSERT( rc == LPF::SUCCESS );
        rc = lpf.register_global( array, n*sizeof(T), &dst);
        ASSERT( rc == LPF::SUCCESS );

        lpf.put( src, 0, root, dst, sizeof(T)*lpf.pid(), sizeof(pod));

        rc = lpf.sync();

        lpf.deregister( src );
        lpf.deregister( dst );
        return rc;
    }

    template <class LPF, typename T>
    typename LPF::err_t 
    scatter( LPF lpf, typename LPF::pid_t root, std::vector<T> & array, T & pod )
    {
        typename LPF::err_t rc = LPF::SUCCESS;
        rc = lpf.resize_memory_register( 2 );
        if (rc != LPF::SUCCESS) return rc;

        rc = lpf.resize_message_queue( lpf.nprocs() );
        if (rc != LPF::SUCCESS) return rc;

        rc = lpf.sync();
        if (rc != LPF::SUCCESS) return rc;

        typename LPF::memslot_t src = LPF::INVALID_MEMSLOT;
        typename LPF::memslot_t dst = LPF::INVALID_MEMSLOT;
        rc = lpf.register_local( &pod, sizeof(T), &dst);
        ASSERT( rc == LPF::SUCCESS );
        rc = lpf.register_global( array.data(), array.size()*sizeof(T), &src);
        ASSERT( rc == LPF::SUCCESS );

        lpf.get( root, src, sizeof(T)*lpf.pid(), dst, 0, sizeof(pod));

        rc = lpf.sync();

        lpf.deregister( src );
        lpf.deregister( dst );
        return rc;
    }

    template <class LPF, typename T>
    typename LPF::err_t 
    prefixsum( LPF lpf, std::vector<T> & array )
    {
        std::vector< T > offsets;
        try {
            offsets.resize( lpf.nprocs() );
        } 
        catch( std::bad_alloc & e)
        {
            return LPF::ERR_OUT_OF_MEMORY;
        }

        typename LPF::err_t rc = LPF::SUCCESS;
        rc = lpf.resize_memory_register( 1 );
        if (rc != LPF::SUCCESS) return rc;

        rc = lpf.resize_message_queue( lpf.nprocs() );
        if (rc != LPF::SUCCESS) return rc;

        rc = lpf.sync();
        if (rc != LPF::SUCCESS) return rc;

        typename LPF::memslot_t slot = LPF::INVALID_MEMSLOT;
        typename LPF::memslot_t dst = LPF::INVALID_MEMSLOT;
        rc = lpf.register_global( offsets.data(), offsets.size()*sizeof(T), &slot);
        ASSERT( rc == LPF::SUCCESS );

        offsets[ lpf.pid() ] = std::accumulate( array.begin(), array.end(), 0);

        for (typename LPF::pid_t p = 0, P = lpf.nprocs(); p < P; ++p) {
            if ( p != lpf.pid() ) {
                lpf.put( p, slot, sizeof(T)*lpf.pid(), 
                        slot, sizeof(T)*lpf.pid(), sizeof(T));
            }
        }

        rc = lpf.sync();
        if (rc != LPF::SUCCESS) return rc;

        lpf.deregister( slot );

        T myOffset = 0;
        for (typename LPF::pid_t p = 1, P = lpf.pid(); p < P; ++p)
            myOffset += offsets[p];

        if ( !array.empty() )
            array[0] += myOffset;

        for (size_t i = 1; i < array.size(); ++i)
            array[i] += array[i-1];

        return rc;
    }

} } // namespace lpf , hybrid

#endif
