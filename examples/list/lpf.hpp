
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

#ifndef LPF_CPP
#define LPF_CPP

#include <lpf/core.h>
#include <lpf/collectives.h>

#include "lpf_random.h"

#include <vector>
#include <stdexcept>
#include <cstring>
#include <cassert>
#include <limits>
#include <algorithm>

namespace lpf {

    class machine
    {
    public:
        static machine & root()
        {
            static machine m( LPF_ROOT, 0, 1, LPF_NO_ARGS); 
            return m;
        }

        machine( lpf_t ctx, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args )
            : m_ctx( ctx )
            , m_coll( LPF_INVALID_COLL )
            , m_pid( pid )
            , m_nprocs( nprocs )
            , m_args( args )
            , m_mesgq_cap(0), m_mesgq_next_cap(0)
            , m_max_mesgq_use(0), m_mesgq_use(0)
            , m_memreg_cap(0), m_memreg_use(0)
            , m_coll_max_elem_size( 8 )
            , m_coll_max_byte_size( 8 )
            , m_rnd()
            , m_count_array( nprocs )
            , m_count_array_slot (LPF_INVALID_MEMSLOT )
            , m_reduce_array( align( m_coll_max_elem_size * m_nprocs )+16 )
            , m_reduce_array_slot(LPF_INVALID_MEMSLOT )
            , m_mesgq_resize_required_me( 0 )
            , m_mesgq_resize_required_me_slot( LPF_INVALID_MEMSLOT )
            , m_mesgq_resize_required_all( 0 )
            , m_mesgq_resize_required_all_slot( LPF_INVALID_MEMSLOT )
        {
            lpf_rand_create( &m_rnd, 0, m_pid, m_nprocs );

            memreg_resize( 5 ); // reserve 1 for collectives_init
                                // reserve 1 for m_count_array
                                // reserve 1 for m_reduce_array
                                // reserve 1 for m_mesgq_resize_required
            // an two allgathers
            if (lpf_resize_message_queue( m_ctx, 4*nprocs ) != LPF_SUCCESS)
                throw std::bad_alloc(); 
            m_mesgq_next_cap = 4*nprocs;
            m_mesgq_cap = 4*nprocs;
            fence();
            if (LPF_SUCCESS != lpf_collectives_init( m_ctx, 
                        m_pid, m_nprocs, 0,
                        m_coll_max_elem_size, m_coll_max_byte_size,
                        &m_coll ))
                throw std::bad_alloc();
            lpf_register_global( m_ctx, m_count_array.data(),
                    sizeof(size_t) * m_nprocs, &m_count_array_slot);
            lpf_register_global( m_ctx, m_reduce_array.data(),
                    m_reduce_array.capacity(), &m_reduce_array_slot);
            lpf_register_global( m_ctx, &m_mesgq_resize_required_me,
                    sizeof(int), &m_mesgq_resize_required_me_slot );
            lpf_register_global( m_ctx, &m_mesgq_resize_required_all,
                    sizeof(int), &m_mesgq_resize_required_all_slot );
            m_max_mesgq_use = 4*nprocs;
            m_memreg_use += 5;
            fence();
        }


        ~machine() {
            lpf_collectives_destroy( m_coll );
            m_coll = LPF_INVALID_COLL;
            lpf_deregister( m_ctx, m_count_array_slot );
            m_count_array_slot = LPF_INVALID_MEMSLOT;
            lpf_deregister( m_ctx, m_reduce_array_slot );
            m_reduce_array_slot = LPF_INVALID_MEMSLOT;
            lpf_deregister( m_ctx, m_mesgq_resize_required_me_slot );
            m_mesgq_resize_required_me_slot = LPF_INVALID_MEMSLOT;
            lpf_deregister( m_ctx, m_mesgq_resize_required_all_slot );
            m_mesgq_resize_required_all_slot = LPF_INVALID_MEMSLOT;
        }

        typedef void (*spmd_t)( machine & );

        void exec( lpf_pid_t nprocs, spmd_t spmd, 
                const void * input = NULL, size_t input_size=0,
                void * output = NULL, size_t output_size=0 ,
                lpf_func_t * funcs = NULL, size_t funcs_size = 0)
        {
            std::vector< lpf_func_t > fs( funcs_size + 1);
            std::copy( funcs, funcs + funcs_size, fs.begin() );
            fs.back() = reinterpret_cast<lpf_func_t>(spmd);

            lpf_args_t args;
            args.input = input;
            args.input_size = input_size;
            args.output = output;
            args.output_size = output_size;
            args.f_symbols = fs.data();
            args.f_size = fs.size();
            lpf_err_t rc = lpf_exec( m_ctx, nprocs, launch, args );
            if ( LPF_ERR_OUT_OF_MEMORY == rc )
                throw std::bad_alloc();
            else if ( LPF_SUCCESS != rc ) 
                throw std::runtime_error("fatal error returned by lpf_exec");
        }

        lpf_pid_t pid() const { return m_pid; }
        lpf_pid_t nprocs() const { return m_nprocs; }

        const void * input() const 
        { return m_args.input; }

        size_t input_size() const
        { return m_args.input_size; }

        size_t output_size() const
        { return m_args.output_size; }

        size_t set_output( const void * buf, size_t size )
        {
            if (m_pid == 0) {
                size_t n = std::min( size, m_args.output_size);
                std::memcpy( m_args.output, buf, n  );
                return n;
            }
            return 0;
        }

        size_t symbols_count() const
        { return m_args.f_size; }

        const lpf_func_t * symbols() const
        { return m_args.f_symbols; }

        double random()
        { return lpf_rand_next( &m_rnd ) /
            (1.0 + static_cast<double>(std::numeric_limits<uint64_t>::max()));
        }

        void fence() {

            if ( m_mesgq_next_cap > m_mesgq_cap ) {
                m_mesgq_resize_required_me = 1;
                for (lpf_pid_t p = 0; p < m_nprocs; ++p) {
                    lpf_put( m_ctx, m_mesgq_resize_required_me_slot, 0,
                            p, m_mesgq_resize_required_all_slot, 0, 
                            sizeof(int), LPF_MSG_DEFAULT);
                }
            }

            if ( LPF_SUCCESS != lpf_sync( m_ctx, LPF_SYNC_DEFAULT ))
                throw std::runtime_error("fatal error during lpf_sync");

            m_mesgq_use = 0;

            if (m_mesgq_resize_required_all) {
                // then somebody did a resize; we're out of sync now

                std::fill( m_count_array.begin(), m_count_array.end(),
                        m_mesgq_next_cap );

                if ( LPF_SUCCESS != lpf_sync( m_ctx, LPF_SYNC_DEFAULT ))
                    throw std::runtime_error("fatal error during lpf_sync");

                // allgather m_mesg_next_cap
                for (lpf_pid_t p = 0; p < m_nprocs; ++p) {
                    if (p != m_pid)
                        lpf_put( m_ctx,
                                m_count_array_slot, sizeof(size_t)*m_pid,
                                p, m_count_array_slot, sizeof(size_t)*m_pid,
                                sizeof(size_t), LPF_MSG_DEFAULT);
                }

                /* S3 */
                if ( LPF_SUCCESS != lpf_sync( m_ctx, LPF_SYNC_DEFAULT ))
                    throw std::runtime_error("fatal error during lpf_sync");
                
                size_t new_size = *std::max_element( m_count_array.begin(), 
                        m_count_array.end() );

                size_t newcap = std::max( m_mesgq_cap, new_size ); 
                lpf_err_t rc = lpf_resize_message_queue( m_ctx, newcap );
                if (rc == LPF_ERR_OUT_OF_MEMORY)
                    throw std::bad_alloc();
                else if (rc != LPF_SUCCESS )
                    throw std::runtime_error("fatal error by lpf_resize_message_queue");
                m_mesgq_cap = newcap;

                /* S2 */
                if ( LPF_SUCCESS != lpf_sync( m_ctx, LPF_SYNC_DEFAULT ))
                 throw std::runtime_error("fatal error during lpf_sync");

                m_mesgq_resize_required_me = 0;
                m_mesgq_resize_required_all = 0;
            }
        }

        size_t mesgq_capacity() const { return m_mesgq_cap; }
        void   mesgq_resize( size_t n ) {
            m_mesgq_next_cap = n;
        }
        size_t memreg_capacity() const { return m_memreg_cap; }
        void   memreg_resize( size_t n ){
            size_t newcap = std::max( m_memreg_use, n );
            lpf_err_t rc = lpf_resize_memory_register( m_ctx, newcap );
            if (rc == LPF_ERR_OUT_OF_MEMORY)
                throw std::bad_alloc();
            else if (rc != LPF_SUCCESS )
                throw std::runtime_error("fatal error by lpf_resize_memory_register");
            m_memreg_cap = newcap;
        }

        enum comm_pattern { zero_rel, one_rel, p_rel, n_rel } ;  

        // collective & synchronizing
        lpf_memslot_t register_global_memory( void * mem, size_t size )
        {
            // allocate a place in the memory register
            if ( m_memreg_cap == m_memreg_use ) {
                memreg_resize( std::max(1ul, 2 * m_memreg_cap) );
            }
            fence();

            assert( m_memreg_cap > m_memreg_use );
            lpf_memslot_t result = LPF_INVALID_MEMSLOT;
            lpf_register_global( m_ctx, mem, size, &result);
            lpf_sync( m_ctx, LPF_SYNC_DEFAULT );
            m_memreg_use += 1;
            return result;
        }

        void deregister( lpf_memslot_t slot ) {
            assert( m_memreg_use >= 1 );
            lpf_deregister( m_ctx, slot );
            m_memreg_use -= 1;
        }


        void alloc_comm(size_t size, size_t elem_size, comm_pattern pat )
        {
            if ( pat == p_rel ) {
#ifndef NDEBUG
                fence();
                m_count_array[ m_pid ] = size;
                fence();
                for ( lpf_pid_t p = 0; p < m_nprocs; ++p ) {
                    if (p != m_pid )
                      lpf_put( m_ctx, m_count_array_slot, m_pid * sizeof(size_t),
                            p, m_count_array_slot, m_pid * sizeof(size_t),
                            sizeof(size_t), LPF_MSG_DEFAULT );

                }
                fence();
                
                for ( lpf_pid_t p = 0; p < m_nprocs; ++p)
                    assert( m_count_array[p] == size );
#endif
                // make sure the collectives interface has enough resources
                if ( elem_size > m_coll_max_elem_size 
                            || size > m_coll_max_byte_size ) {
                    lpf_collectives_destroy( m_coll ); 

                    m_coll_max_elem_size = std::max( m_coll_max_elem_size, elem_size );
                    m_coll_max_byte_size = std::max( m_coll_max_byte_size, size );
                    if ( LPF_SUCCESS != lpf_collectives_init( m_ctx, 
                            m_pid, m_nprocs, 0,
                            m_coll_max_elem_size, m_coll_max_byte_size,
                            &m_coll )) 
                    {
                        fence();
                        throw std::bad_alloc();
                    }

                    if (m_reduce_array_slot != LPF_INVALID_MEMSLOT )
                      lpf_deregister( m_ctx, m_reduce_array_slot );

                    m_reduce_array.resize(align(m_coll_max_elem_size * m_nprocs)+16);

                    lpf_register_global( m_ctx, m_reduce_array.data(),
                        m_reduce_array.capacity(), &m_reduce_array_slot);

                    fence();
                }
            }
            // Every global memory can be used for collectives, so allocate if
            // necessary
            switch ( pat ) {
                case zero_rel: break;
                case one_rel: m_max_mesgq_use += 2; break;
                case p_rel:   m_max_mesgq_use += 2*m_nprocs; break;
                case n_rel:   m_max_mesgq_use += 2*(size/elem_size); break;
            }
            
            if ( m_max_mesgq_use > m_mesgq_cap ) {
                size_t new_size = std::max(m_max_mesgq_use, 2*m_mesgq_cap);
                mesgq_resize( new_size  );
            }
        }

        void dealloc_comm( size_t size, size_t elem_size, comm_pattern pat ) {
            switch ( pat ) {
                case zero_rel: break;
                case one_rel:
                    assert( m_max_mesgq_use >= 2 );
                    m_max_mesgq_use -= 2;
                    break;
                case p_rel:  
                    assert( m_max_mesgq_use >= 2*m_nprocs );
                    m_max_mesgq_use -= 2*m_nprocs;
                    break;
                case n_rel:   
                    assert( m_max_mesgq_use >= 2*(size/elem_size)) ;
                    m_max_mesgq_use -= 2*(size/elem_size);
                    break;
            }
        }

        void put( comm_pattern pat, lpf_memslot_t src_slot, size_t src_offset, 
                lpf_pid_t dst_pid, lpf_memslot_t dst_slot, size_t dst_offset,
                size_t size )
        {
            if ( m_mesgq_cap < m_mesgq_use + patsize(pat, 1) )
                throw std::bad_alloc();

            m_mesgq_use += patsize(pat, 1);
            lpf_put( m_ctx, src_slot, src_offset, dst_pid, dst_slot, dst_offset,
                    size, LPF_MSG_DEFAULT );
        }

        void get( comm_pattern pat, lpf_pid_t src_pid, lpf_memslot_t src_slot, size_t src_offset, 
                lpf_memslot_t dst_slot, size_t dst_offset, size_t size )
        {
            if ( m_mesgq_cap < m_mesgq_use + patsize(pat, 1))
                throw std::bad_alloc();

            m_mesgq_use += patsize(pat, 1);
            lpf_get( m_ctx, src_pid, src_slot, src_offset, dst_slot, dst_offset,
                    size, LPF_MSG_DEFAULT );
        }

        template <class T>
        void any( T & obj, lpf_memslot_t src, size_t src_offset,
                lpf_memslot_t dst, size_t dst_offset, size_t size, lpf_pid_t root )
        {
            size_t nmsgs =  m_nprocs - 1;
            if ( m_mesgq_use + nmsgs > m_mesgq_cap ) throw std::bad_alloc();
            m_mesgq_use += nmsgs;

            if ( m_pid != root && obj )
                lpf_put( m_ctx, src, src_offset, root, dst, dst_offset, size, LPF_MSG_DEFAULT );
        }

        template <class T>
        void any( const T & srcobj, lpf_memslot_t src, size_t src_offset,
                lpf_memslot_t dst, size_t dst_offset, size_t size )
        {
            size_t nmsgs =  2*m_nprocs;
            if ( m_mesgq_use + nmsgs > m_mesgq_cap ) throw std::bad_alloc();
            m_mesgq_use += nmsgs;

            if ( srcobj ) {
                for ( lpf_pid_t p = 0; p < m_nprocs; ++p )
                    lpf_put( m_ctx, src, src_offset, p, dst, dst_offset, size, LPF_MSG_DEFAULT );
            }
        }

        void bcast( lpf_memslot_t src, lpf_memslot_t dst, size_t size, lpf_pid_t root) 
        {
            size_t nmsgs =  m_nprocs >= 4 ? 2*m_nprocs - 3 : m_nprocs + 1;
            if ( m_mesgq_use + nmsgs > m_mesgq_cap ) throw std::bad_alloc();
            m_mesgq_use += nmsgs;

            if (LPF_SUCCESS != lpf_broadcast( m_coll, src ,dst, size, root ))
                throw std::runtime_error("lpf_broadcast");
        }   

        void gather( lpf_memslot_t src, lpf_memslot_t dst, size_t size, lpf_pid_t root)
        {
            size_t nmsgs =  m_nprocs - 1;
            if ( m_mesgq_use + nmsgs > m_mesgq_cap ) throw std::bad_alloc();
            m_mesgq_use += nmsgs;

            if (LPF_SUCCESS != lpf_gather( m_coll, src ,dst, size, root ))
                throw std::runtime_error("lpf_gather");
        }

        void scatter( lpf_memslot_t src, lpf_memslot_t dst, size_t size, lpf_pid_t root)
        {
            size_t nmsgs =  m_nprocs - 1;
            if ( m_mesgq_use + nmsgs > m_mesgq_cap ) throw std::bad_alloc();
            m_mesgq_use += nmsgs;

            if (LPF_SUCCESS != lpf_scatter( m_coll, src ,dst, size, root ))
                throw std::runtime_error("lpf_scatter");
        }


        void allgather( lpf_memslot_t src, lpf_memslot_t dst, size_t size)
        {
            size_t nmsgs =  2*m_nprocs;
            if ( m_mesgq_use + nmsgs > m_mesgq_cap ) throw std::bad_alloc();
            m_mesgq_use += nmsgs;

            bool exclude_myself = false;
            if (LPF_SUCCESS != lpf_allgather( m_coll, src ,dst, size, exclude_myself ))
                throw std::runtime_error("lpf_allgather");
        }

        void allgather( lpf_memslot_t src, size_t src_offset, 
                lpf_memslot_t dst, size_t dst_offset, size_t size)
        {
            size_t nmsgs =  2*m_nprocs;
            if ( m_mesgq_use + nmsgs > m_mesgq_cap ) throw std::bad_alloc();
            m_mesgq_use += nmsgs;

            for ( lpf_pid_t p = 0; p < m_nprocs; ++p )
            {
                lpf_put( m_ctx, src, src_offset,
                       p, dst, dst_offset + size * m_pid, size,
                       LPF_MSG_DEFAULT);
            }
        }

        void allgatherv( lpf_memslot_t src, size_t src_offset, 
                lpf_memslot_t dst, size_t dst_offset, size_t size)
        {
            size_t nmsgs =  2*m_nprocs;
            if ( m_mesgq_use + nmsgs > m_mesgq_cap ) throw std::bad_alloc();
            m_mesgq_use += nmsgs;

            // Now 'size' is not globally replicated. Do an allgather of 'size'
            fence();
            m_count_array[ m_pid ] = size;
            fence();
            for ( lpf_pid_t p = 0; p < m_nprocs; ++p ) {
                if (p != m_pid )
                  lpf_put( m_ctx, m_count_array_slot, m_pid * sizeof(size_t),
                        p, m_count_array_slot, m_pid * sizeof(size_t),
                        sizeof(size_t), LPF_MSG_DEFAULT );

            }
            fence();

            size_t nbytes = m_count_array[m_pid];
            for ( lpf_pid_t p = 0; p < m_pid; ++p ) 
                dst_offset += m_count_array[p];
             
            // Now we can send the messages
            for ( lpf_pid_t p = 0; p < m_nprocs; ++p ) {
                lpf_put( m_ctx, src, src_offset, p, dst, dst_offset,
                        nbytes, LPF_MSG_DEFAULT );
            }
        }

        void alltoall( lpf_memslot_t src, lpf_memslot_t dst, size_t size)
        {
            size_t nmsgs =  2*m_nprocs - 2;
            if ( m_mesgq_use + nmsgs > m_mesgq_cap ) throw std::bad_alloc();
            m_mesgq_use += nmsgs;

            if (LPF_SUCCESS != lpf_alltoall( m_coll, src ,dst, size ))
                throw std::runtime_error("lpf_alltoall");
        }

        void reduce( void * element, lpf_memslot_t slot, size_t size, 
                lpf_reducer_t reducer, lpf_pid_t root )
        {
            size_t nmsgs =  m_nprocs - 1;
            if ( m_mesgq_use + nmsgs > m_mesgq_cap ) throw std::bad_alloc();
            m_mesgq_use += nmsgs;

            if (LPF_SUCCESS != lpf_reduce( m_coll, element, slot, size, reducer, root ))
                throw std::runtime_error("lpf_reduce");
        }

        void allreduce( void * element, lpf_memslot_t slot, size_t size, 
                lpf_reducer_t reducer )
        {
            size_t nmsgs =  2*m_nprocs - 2;
            if ( m_mesgq_use + nmsgs > m_mesgq_cap ) throw std::bad_alloc();
            m_mesgq_use += nmsgs;

            if (LPF_SUCCESS != lpf_allreduce( m_coll, element, slot, size, reducer ))
                throw std::runtime_error("lpf_allreduce");
        }

        void allreduce( void * element, lpf_memslot_t slot, size_t offset,
                size_t size, lpf_reducer_t reducer )
        {
            size_t nmsgs =  2*m_nprocs;
            if ( m_mesgq_use + nmsgs > m_mesgq_cap ) throw std::bad_alloc();
            m_mesgq_use += nmsgs;

            // allgather all data
            for (lpf_pid_t p = 0 ; p < m_nprocs ; ++p ){
                lpf_put( m_ctx, slot, offset, 
                    p, m_reduce_array_slot, size * m_pid, size,
                    LPF_MSG_DEFAULT );
            }

            fence();
            assert( m_reduce_array.size() >= size * m_nprocs );
            void * x = align( m_reduce_array.data() );
            void * xs = static_cast<char *>(x) + size;
            (*reducer)( m_nprocs, xs, x);
            memcpy( element, x, size );
            
            fence();
        }

    private:
        machine (const machine & ) ; //copying prohibited
        machine & operator=(const machine &); // assignment prohibited

        static void launch( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args )
        {
            assert( args.f_size >= 1 );
            args.f_size -= 1;
            machine m( lpf, pid, nprocs, args );
            spmd_t spmd = reinterpret_cast<spmd_t>( args.f_symbols[ args.f_size ] );
            (*spmd)( m );
        }

        size_t align( size_t n ) // 16-byte alignment
        { size_t a = 16; return (n + a - 1) / a * a ; }

        void * align( void * xs ) 
        { return static_cast<char *>(0) + align(
                size_t( static_cast<char *>(xs) - static_cast<char *>(0))
                );
        }

        size_t patsize( comm_pattern pat, size_t n )
        {
            switch ( pat ) {
                case zero_rel: return 0;
                case one_rel:  return 2;
                case p_rel:    return 2*m_nprocs;
                case n_rel:    return 2*n;
            }
            assert(!"unreachable code");
            return 0;
        }

        lpf_t m_ctx;
        lpf_coll_t m_coll;
        lpf_pid_t m_pid;
        lpf_pid_t m_nprocs;
        lpf_args_t m_args;
        size_t    m_mesgq_cap, m_mesgq_next_cap, m_max_mesgq_use, m_mesgq_use;
        size_t    m_memreg_cap, m_memreg_use;
        size_t    m_coll_max_elem_size;
        size_t    m_coll_max_byte_size;
        lpf_rand_state_t m_rnd;

        std::vector< size_t > m_count_array;
        lpf_memslot_t m_count_array_slot;
        std::vector< char > m_reduce_array;
        lpf_memslot_t m_reduce_array_slot;
        int m_mesgq_resize_required_me;
        lpf_memslot_t m_mesgq_resize_required_me_slot;
        int m_mesgq_resize_required_all;
        lpf_memslot_t m_mesgq_resize_required_all_slot;
    };

    class temporary_space {
    public:
        temporary_space( machine & ctx, size_t alignment = (1<<12) )
            : m_ctx( ctx )
            , m_space( NULL )
            , m_aligned_space( NULL )
            , m_used( 0 )
            , m_number_of_objects(0)
            , m_reserved( 0 )
            , m_alignment( alignment )
            , m_slot( LPF_INVALID_MEMSLOT )
        {

        }

        ~temporary_space()
        {
            assert( m_number_of_objects == 0 && "Memory leak???");
            if (m_slot != LPF_INVALID_MEMSLOT ) {
                m_ctx.deregister( m_slot );
                m_slot = LPF_INVALID_MEMSLOT;
            }

            std::free( m_space );
        }

        size_t allocate( size_t bytes, size_t alignment )
        {
            assert( m_alignment % alignment == 0);

            size_t offset = align( m_used, alignment );
            if (offset + bytes > m_reserved)
                reallocate( offset + bytes );

            m_used = offset + bytes;
            m_number_of_objects += 1;
            
            return offset;
        }

        template <class T>
        size_t allocate( size_t number )
        {
            size_t object_size = sizeof(T);
#if __cplusplus >= 201103L    
            size_t alignment = alignof(T);
#else
            // work around lack of alignof operator:
            // finding largest power of 2 less or equal than object size
            size_t x = sizeof(T);
            int power = 0;

            while (x >> power) power++;

            size_t alignment = 1ul << (power > 0 ? (power-1) : 0 );
#endif
            return allocate( object_size * number, alignment );
        }

        void deallocate( size_t offset ) 
        {
            (void) offset;
            assert( m_number_of_objects > 0 );
            m_number_of_objects -= 1;

            if (m_number_of_objects == 0)
                m_used = 0; // clear space automatically
        }
        
        void clear()
        {
            m_used = 0;
            m_number_of_objects = 0;
        }

        void * memory( size_t offset) 
        { return static_cast<void *>(& m_aligned_space[offset]); }

        const void * memory( size_t offset) const
        { return static_cast<void *>(& m_aligned_space[offset]); }

        lpf_memslot_t slot() const 
        { return m_slot; }

        machine & ctx() 
        { return m_ctx; }

    private:
        temporary_space(const temporary_space & ); // copying prohibited
        temporary_space & operator=(const temporary_space & ); // assignment prohibited

        void reallocate(size_t new_size) 
        {
            new_size = std::max( 2 * m_reserved, new_size );
            // allocate new memory
            char * new_space = static_cast< char * >( 
                    std::malloc( align( new_size , m_alignment )+m_alignment) 
                    );
            if (!new_space)
                throw std::bad_alloc();

            // align it
            char * new_aligned_space 
                = null() + align( new_space - null(), m_alignment ); 

            // register it
            lpf_memslot_t new_slot = m_ctx.register_global_memory(
                       new_aligned_space, new_size );

            // copy old data
            memcpy( new_aligned_space, m_aligned_space, m_reserved );

            // deregister old memory
            if (m_slot != LPF_INVALID_MEMSLOT)
                m_ctx.deregister( m_slot );
            // free old memory
            std::free( m_space );

            // point to new memory block
            m_slot = new_slot;
            m_space = new_space;
            m_aligned_space = new_aligned_space;
            m_reserved = new_size;
        }

        template <class X>
        static X align( X x, size_t a ) 
        { return ( x + a - 1) / a * a ; }

        static char * null() { return static_cast<char *>(0); }

        
        machine & m_ctx;
        char * m_space;
        char * m_aligned_space;
        size_t m_used;
        size_t m_number_of_objects;
        size_t m_reserved;
        size_t m_alignment;
        lpf_memslot_t m_slot;
    };

    template <class T>
    class temporary 
    {   // FIXME: limit T explicitly to POD only
    public:
        temporary( temporary_space & space, const T & obj = T() )
            : m_space(space)
            , m_offset( m_space.allocate<T>(1) )
        {
            get() = obj;
        }

        ~temporary()
        {
            m_space.deallocate( m_offset );
        }

        T & get() 
        { return * static_cast<T *>( m_space.memory( m_offset ) ); }

        const T & get() const 
        { return * static_cast<T *>( m_space.memory( m_offset ) ); }

        size_t offset() const 
        { return m_offset; }

        size_t slot() const
        { return m_space.slot(); }    

        machine * ctx() 
        { return & m_space.ctx(); }

    private:
        temporary( const temporary & ); // copying prohibited

        temporary_space & m_space; 
        size_t m_offset;
    };

    template <class T>
    class temporary< std::vector< T > >  
    {   // FIXME: limit T explicitly to POD only
    public:
        temporary( temporary_space & space, 
                const std::vector<T> & obj = std::vector<T>() )
            : m_space(space)
            , m_offset( m_space.allocate<T>( obj.size() ) )
            , m_size( obj.size() )
            , m_reserved( obj.size() )
        {
            std::copy( obj.begin(), obj.end(), data() ); 
        }

        temporary( temporary_space & space, size_t n, const T & val = T() )
            : m_space(space)
            , m_offset( m_space.allocate<T>( n ) )
            , m_size( n )
            , m_reserved( n )
        {
            std::fill_n( data(), n, val  ); 
        }

        ~temporary()
        {
            m_space.deallocate( m_offset );
        }

        T * data() 
        { return static_cast<T *>( m_space.memory( m_offset ) ); }

        const T * data() const 
        { return static_cast<T *>( m_space.memory( m_offset ) ); }

        size_t size() const
        { return m_size; }

        size_t capacity() const
        { return m_reserved; }

        bool empty() const
        { return m_size == 0; }

        void pop_back() 
        {
            assert( m_size > 0 );
            m_size -= 1;
        }

        void push_back(const T & val)
        {
            if ( m_size >= m_reserved )
                throw std::bad_alloc();

            data()[ m_size ] = val;
            m_size += 1;
        }

        void resize( size_t n, const T & val = T())
        {
            assert( n <= m_reserved );
            if (n > m_size)
                std::fill( data() + m_size, data() + n, val);
            m_size = n;
        }

        void reserve( size_t n ) 
        {
            if ( n > m_reserved )
                throw std::bad_alloc();
        }

        size_t offset() const 
        { return m_offset; }

        size_t slot() const
        { return m_space.slot(); }    

        machine * ctx() 
        { return & m_space.ctx(); }

    private:
        temporary( const temporary & ); // copying prohibited

        temporary_space & m_space; 
        size_t m_offset; // in bytes
        size_t m_size;   // in number of objects of T
        size_t m_reserved; // in number of objects
    };

    template <class T>
    class persistent 
    {
    public:
        persistent()
            : m_ctx(NULL)
            , m_object()
            , m_slot( LPF_INVALID_MEMSLOT )
        {}

        persistent(machine & ctx , const T & obj = T())
            : m_ctx(&ctx)
            , m_object(obj)
            , m_slot( m_ctx->register_global_memory( &m_object, sizeof(T) ))
        {
             
        }

        persistent( const persistent & other )
            : m_ctx( other.m_ctx )
            , m_object( other.m_object)
            , m_slot( m_ctx->register_global_memory(
                        &m_object, sizeof(T)) )
        {}

        ~persistent() 
        {
            if (m_slot != LPF_INVALID_MEMSLOT)
                m_ctx->deregister( m_slot );
        }

        void swap( persistent & other )
        {
            using std::swap;
            swap( m_object, other.m_object );
            swap( m_slot, other.m_slot );
        }

        persistent & operator=( const persistent & other )
        {
            T copy(other);
            swap( copy );
            return *this;
        }

        T & get() 
        { return m_object; }

        const T & get() const 
        { return m_object; }

        size_t offset() const 
        { return 0ul; }

        size_t slot() const
        { return m_slot; }

        machine * ctx() 
        { return m_ctx; }

    private:
        machine    *  m_ctx;
        T             m_object;
        lpf_memslot_t m_slot;
    };

    template <class T>
    class persistent< std::vector< T > > 
    {
    public:
        persistent()
            : m_ctx(NULL)
            , m_array()
            , m_slot( LPF_INVALID_MEMSLOT )
        {}

        persistent(machine & ctx)
            : m_ctx(&ctx)
            , m_array()
            , m_slot( LPF_INVALID_MEMSLOT )
        {
             
        }

        persistent(machine & ctx, 
                const std::vector<T> & obj = std::vector<T>())
            : m_ctx(&ctx)
            , m_array(obj)
            , m_slot( m_ctx->register_global_memory(
                        m_array.data(), m_array.capacity() * sizeof(T)
                    ) )
        {
             
        }

        persistent(machine & ctx, size_t n, const T & val = T() )
            : m_ctx(&ctx)
            , m_array(n, val)
            , m_slot( m_ctx->register_global_memory(
                        m_array.data(), m_array.capacity() * sizeof(T)
                    ) )
        {
             
        }

        persistent( const persistent & other )
            : m_ctx( other.m_ctx )
            , m_array( other.m_array )
            , m_slot( m_ctx->register_global_memory(
                        m_array.data(), m_array.capacity() * sizeof(T)
                        ) )
        {}

        ~persistent() 
        {
            if (m_slot != LPF_INVALID_MEMSLOT)
                m_ctx->deregister( m_slot );
        }

        void swap( persistent & other )
        {
            using std::swap;
            m_array.swap( other.m_array );
            swap( m_slot, other.m_slot );
        }

        persistent & operator=( const persistent & other )
        {
            T copy(other);
            swap( copy );
            return *this;
        }

        T * data() 
        { return m_array.data(); }

        const T * data() const 
        { return m_array.data(); }

        size_t size() const
        { return m_array.size(); }

        size_t capacity() const
        { return m_array.capacity(); }

        void resize( size_t size, const T & val = T() ) 
        {
            assert( size <= m_array.capacity() );
            m_array.resize( size, val );
        }

        void reserve( size_t size) 
        {
            m_array.reserve( size );
            if (m_slot != LPF_INVALID_MEMSLOT)
                m_ctx->deregister( m_slot );

            m_slot = m_ctx->register_global_memory( 
                    m_array.data(), m_array.capacity() * sizeof(T));
        }

        void clear() 
        { m_array.clear(); }

        void push_back( const T & val )
        {
            assert( m_array.size() < m_array.capacity() );
            m_array.push_back( val );
        }

        void pop_back()
        {   assert( ! m_array.empty() );
            m_array.pop_back();
        }

        size_t offset() const 
        { return 0ul; }

        size_t slot() const
        { return m_slot; }

        machine * ctx() 
        { return m_ctx; }

    private:
        machine    *  m_ctx;
        std::vector<T> m_array;
        lpf_memslot_t m_slot;
    };

    template <class T, template <class X> class storage > class global;

    template <class T, template <class X> class S >
    machine & getctx( global< T , S> & obj ) 
    { return * obj.m_storage.ctx(); }

    template <class T, template <class X> class S1, template <class X> class S2>
    void copy( machine::comm_pattern pat,
            const global< std::vector<T>, S1 > & from_array,
            size_t from_begin, size_t from_end, 
            lpf_pid_t to, global< std::vector<T>, S2 > & to_array,
            size_t to_begin, size_t to_end )
    {
        (void) from_end;
        assert( from_end >= from_begin );
        assert( to_end >= to_begin );
        assert( from_end - from_begin == to_end - to_begin );
        getctx(to_array).put( pat, from_array.slot(), from_array.offset() + sizeof(T)*from_begin, 
                to, to_array.slot(), to_array.offset() + sizeof(T)*to_begin, 
                (to_end - to_begin)*sizeof(T) );
    }

    template <class T, template <class X> class S1, template <class X> class S2>
    void copy( machine::comm_pattern pat,
             lpf_pid_t from,
            const global< std::vector<T>, S1 > & from_array,
            size_t from_begin, size_t from_end, 
            global< std::vector<T>, S2 > & to_array,
            size_t to_begin, size_t to_end )
    {
        (void) from_end;
        assert( from_end >= from_begin );
        assert( to_end >= to_begin );
        assert( from_end - from_begin == to_end - to_begin );
        getctx(to_array).get( pat, from, 
                from_array.slot(), from_array.offset() + sizeof(T)*from_begin, 
                to_array.slot(), to_array.offset() + sizeof(T)*to_begin, 
                (to_end - to_begin)*sizeof(T) );
    }


    template <class T, template <class X> class S1, template <class X> class S2>
    void put( machine::comm_pattern pat, const global< T, S1 > & src_obj,
            lpf_pid_t dst_pid, global< std::vector< T >, S2 > & dst_array, size_t dst_pos )
    {
        getctx(dst_array).put( pat, src_obj.slot(), src_obj.offset(),
                dst_pid, dst_array.slot(), dst_array.offset() + sizeof(T)*dst_pos, sizeof(T) );
    }

    template <class T, template <class X> class S1, template <class X> class S2>
    void get( machine::comm_pattern pat,  lpf_pid_t src_pid, const global< std::vector< T >, S1 > & src_array, size_t src_pos,
            global< T, S2 > & dst_obj )
    {
        getctx(dst_obj).get( pat, src_pid, src_array.slot(), src_array.offset() + sizeof(T)*src_pos, 
                dst_obj.slot(), dst_obj.offset(), sizeof(T) );
    }

    template <class T, template <class X> class S1, template <class X> class S2>
    void get( machine::comm_pattern pat,  lpf_pid_t src_pid, const global< std::vector< T >, S1 > & src_array, size_t src_pos,
            global< std::vector< T >, S2 > & dst_array, size_t dst_pos )
    {
        getctx(dst_array).get( pat, src_pid, src_array.slot(), src_array.offset() + sizeof(T)*src_pos, 
                dst_array.slot(), dst_array.offset() + sizeof(T)*dst_pos, sizeof(T) );
    }

    template <class T, template <class X> class S1, template <class X> class S2>
    void put(  machine::comm_pattern pat, const global< std::vector< T >, S1 > & src_array, size_t src_pos,
            lpf_pid_t dst_pid, global< std::vector< T >, S2 > & dst_array, size_t dst_pos )
    {
        getctx(dst_array).put( pat, src_array.slot(), src_array.offset() + sizeof(T)*src_pos, 
                dst_pid, dst_array.slot(), dst_array.offset() + sizeof(T)*dst_pos, sizeof(T) );
    }

    template <class T>
    void broadcast( global<T, temporary > & obj, lpf_pid_t root ) {
        if (getctx(obj).pid() != root)
            getctx(obj).get( machine::p_rel, root, obj.slot(), obj.offset(), 
                    obj.slot(), obj.offset(), sizeof(T) );
    }


    template <class T>
    void broadcast( global<T, persistent > & obj, lpf_pid_t root ) {
        getctx(obj).bcast( obj.slot(), obj.slot(), sizeof(T), root );
    }

    template <class T>
    void broadcast( global<std::vector<T>, persistent >  & obj, lpf_pid_t root ) {
        getctx(obj).bcast( obj.slot(), obj.slot(), obj.size() * sizeof(T), root );
    }

    namespace detail {
         template <class T, class Functor > struct Reducer {
            static void f( size_t n, const void * array, void * value) {
                Functor op;
                const T * xs = static_cast<const T *>( array);
                T * x = static_cast<T *>(value);
                T result = *x;

                for (size_t i = 0; i < n; ++i)
                    result = op(result, xs[i]);
                
                *x = result;
            }
         };
    }

    template <class T, template <class X> class S>
    void any( global<T, S> & obj, lpf_pid_t root )
    {
        getctx(obj).any( *obj, obj.slot(), obj.offset(),
                obj.slot(), obj.offset(), sizeof(T), root);
    }

    template <class T, template <class X> class S1, template <class X> class S2>
    void any( const global<T, S1 > & src, global<T, S2> & dst )
    {
        getctx(dst).any( *src, src.slot(), src.offset(),
                dst.slot(), dst.offset(), sizeof(T) );
    }

    template <class T>
    struct min_op : std::binary_function<T, T, T> {
        T operator()(const T & x, const T & y ) { return std::min(x,y); }
    };

    template <class T>
    struct max_op : std::binary_function<T, T, T> {
        T operator()(const T & x, const T & y ) { return std::max(x,y); }
    };


    template <class T, class Functor>
    void reduce( global<T, persistent > & obj, Functor , lpf_pid_t root) {
        getctx(obj).reduce( & *obj, obj.slot(), sizeof(T),
                detail::Reducer<T, Functor>::f, root);
    }

    template <class T, class Functor>
    void allreduce( global<T, persistent > & obj, Functor ) {
        getctx(obj).allreduce( & *obj, obj.slot(), sizeof(T),
                detail::Reducer<T, Functor>::f);
    }

    template <class T, template <class X> class S, class Functor>
    void allreduce( global<T, S > & obj, Functor ) {

        getctx(obj).allreduce( & *obj, obj.slot(), obj.offset(),
                sizeof(T), detail::Reducer<T, Functor>::f);
    }

    template <class T, template <class X> class S1, 
             template <class X> class S2, class Functor >
    void scan( const global< T, S1 > & src, 
            global< std::vector< typename Functor::result_type >, 
                    S2 > & dst, 
            Functor op, T x0 ) 
    {
        allgather( src, dst );
        getctx(dst).fence();

        for ( size_t i = 0 ; i < dst.size(); ++i )  
            dst[i] = x0 = op(x0, dst[i]);
    }

    template <class T >
    void gather( const global<T, persistent > & src, 
            global< std::vector<T >, persistent > & dst,
            lpf_pid_t root) {
        getctx(dst).gather( src.slot(), dst.slot(), sizeof(T), root);
        if ( getctx(dst).pid() == root ) dst[ root ] = *src;
    }

    template <class T>
    void scatter( const global< std::vector< T >, persistent > & src, 
            global<T, persistent > & dst, lpf_pid_t root) {
        getctx(dst).scatter( src.slot(), dst.slot(), sizeof(T), root);
        if ( getctx(dst).pid() == root ) *dst = src[ root ];
    }

    template <class T>
    void allgather( const global<T, persistent > & src, 
            global< std::vector< T >, persistent > & dst) {
        getctx(dst).allgather( src.slot(), dst.slot(), sizeof(T) );
    }

    template <class T, template <class> class S1, template <class> class S2>
    void allgather( const global< std::vector< T >, S1> & src, 
            global< std::vector< T >, S2 > & dst ) {
        getctx(dst).allgatherv( src.slot(), src.offset(),
                dst.slot(), dst.offset(), src.size() * sizeof(T) );
    }

    template <class T, template <class X> class S1, template <class X> class S2>
    void allgather( const global<T, S1 > & src, 
            global< std::vector< T >, S2 > & dst) {
        getctx(dst).allgather( src.slot(), src.offset(), dst.slot(), dst.offset(), sizeof(T) );
        
    }

    template <class T>
    void alltoall( const global< std::vector< T >, persistent > & src, 
            global< std::vector< T >, persistent > & dst ) {
        getctx(dst).alltoall( src.slot(), dst.slot(), sizeof(T) );
    }

    template <class T, template <class> class S1, template <class> class S2>
    void alltoall( const global< std::vector< T >, S1 > & src, 
            global< std::vector< T >, S2> & dst ) {
        machine & ctx = getctx(dst);
        lpf_pid_t P = ctx.nprocs();
        lpf_pid_t s = ctx.pid();
        machine::comm_pattern one_rel = machine::one_rel;

        for ( lpf_pid_t p = 0; p < P; ++p ){
            ctx.put( one_rel, src.slot(), src.offset() + p * sizeof(T),
                    p, dst.slot(), dst.offset() + s * sizeof(T),
                    sizeof(T) );
        }
    }

    template <class T, template <class X> class S >
        class global {
            friend machine & getctx<>( global & obj ); 
            typedef S<T> storage;
        public:

            global()
                : m_storage()
                , m_pat( machine::zero_rel )
            {}

            template <class Alloc>
            explicit global( Alloc & alloc , const T & val = T(),
                    machine::comm_pattern pat = machine::p_rel )
                : m_storage( alloc, val)
                , m_pat( pat )
            {
               m_storage.ctx()->
                   alloc_comm( sizeof(T), sizeof(T), m_pat );
            }
            
            global( const global & other )
                : m_storage(other.m_storage)
                , m_pat( other.m_pat )
            {
               m_storage.ctx()->
                   alloc_comm( sizeof(T), sizeof(T), m_pat );
            }

            ~global()
            {
                if (m_storage.ctx()) {
                    m_storage.ctx()->dealloc_comm(sizeof(T), sizeof(T), m_pat );
                }
            }

            T & operator *()
            { return m_storage.get(); }

            const T & operator *() const
            { return m_storage.get(); }

            T * operator ->()
            { return &m_storage.get(); }

            const T * operator ->() const
            { return &m_storage.get(); }


            lpf_memslot_t slot() const { return m_storage.slot(); }
            size_t offset() const { return m_storage.offset(); } 

        private:
            storage        m_storage;
            machine::comm_pattern m_pat;
        };

        template <class T, template <class X> class S>
        class global< std::vector< T >, S > 
        {
            friend machine & getctx<>( global & obj ); 
            typedef S< std::vector< T > > storage;
        public:
            typedef T * iterator;
            typedef const T * const_iterator;
            typedef T value_type;

            global()
                : m_storage()
                , m_pat( machine::zero_rel )
            {}

            template <class Alloc>
            explicit global( Alloc & alloc, const std::vector<T> & obj,
                    machine::comm_pattern pat )
                : m_storage( alloc, obj )
                , m_pat( pat )
            {
               m_storage.ctx()->
                   alloc_comm( m_storage.capacity() * sizeof(T), sizeof(T), m_pat );
            }

            template <class Alloc>
            explicit global( Alloc & alloc, size_t n, const T & val, 
                    machine::comm_pattern pat  )
                : m_storage( alloc, n, val )
                , m_pat( pat )
            {
               m_storage.ctx()->
                   alloc_comm( m_storage.capacity() * sizeof(T), sizeof(T), m_pat );
            }


            global( const global & other )
                : m_storage( other.m_storage )
                , m_pat( other.m_pat )
            {
               m_storage.ctx()->
                   alloc_comm( m_storage.capacity() * sizeof(T), sizeof(T), m_pat );
            }

            ~global()
            {
                if (m_storage.ctx()) {
                    m_storage.ctx()->dealloc_comm( 
                            m_storage.capacity() * sizeof(T), sizeof(T), m_pat );
                }
            }

            iterator begin() 
            { return m_storage.data(); }

            iterator end() 
            { return m_storage.data() + m_storage.size(); }

            const_iterator begin() const
            { return m_storage.data(); }

            const_iterator end() const
            { return m_storage.data() + m_storage.size(); }

            T & operator[](size_t i)
            { return m_storage.data()[i]; }

            const T & operator[](size_t i) const
            { return m_storage.data()[i]; }

            T * data()
            { return m_storage.data(); }

            const T * data() const
            { return m_storage.data(); }

            T & back() 
            { return * (this->end() - 1); }

            T & front()
            { return * this->begin(); }

            const T & back() const
            { return * (this->end() - 1); }

            const T & front() const
            { return * this->begin(); }

            size_t size() const
            { return m_storage.size(); }

            size_t capacity() const
            { return m_storage.capacity(); }

            void clear() 
            { return m_storage.clear(); }

            void push_back( const T & val )
            {
                assert( m_storage.size() < m_storage.capacity() );
                m_storage.push_back( val );
            }

            void pop_back()
            {   assert( ! m_storage.empty() );
                m_storage.pop_back();
            }

            bool empty() const
            { return m_storage.size() == 0; }

            // collective
            void reserve( size_t n ) 
            {
                n = std::max( m_storage.capacity(), n );
                m_storage.ctx()->dealloc_comm( 
                    m_storage.capacity() * sizeof(T), sizeof(T), m_pat );
                m_storage.reserve( n );
                m_storage.ctx()->alloc_comm( 
                    n * sizeof(T), sizeof(T), m_pat );

            }

            void resize( size_t n, const T & deflt = T() )
            {
                assert( m_storage.capacity() >= n );
                m_storage.resize( n, deflt );
            }

            void swap( global & other )
            {
                using std::swap;
                this->m_storage.swap( other.m_storage );
                swap( this->m_pat, other.m_pat );
            }

            // collective
            global & operator=(const global & other ) 
            {
                if ( this == &other) return *this;

                if ( other.m_pat == this->m_pat &&
                        other.m_storage.size() <= this->m_storage.capacity() ){
                    size_t n = other.m_storage.size() ;
                    this->m_storage.resize( n  );
                    std::copy(other.m_storage.data(),
                              other.m_storage.data() + n,
                              this->m_storage.data());
                    return *this;
                }
                global copy( other );
                this->swap( copy );
                return *this;
            }

            lpf_memslot_t slot() const { return m_storage.slot(); }
            size_t offset() const { return m_storage.offset(); } 

        private:

            storage        m_storage;
            machine::comm_pattern m_pat;
        };


    template < class T, class U >
    std::ostream & operator<<( std::ostream & out, 
           const std::pair< T, U > & p ) {
        return out << "( " << p.first << ", " << p.second << " )";
    } 
    
    template <class T, template <class> class S>
    void print( std::ostream & out, global< std::vector< T >, S > & xs)
    {
        machine & ctx = getctx(xs);
        for (lpf_pid_t p = 0 ; p < ctx.nprocs(); ++p)
        {
            if (p == ctx.pid()) {
                out << "pid " << p << ": ";
                for (size_t i = 0; i < xs.size(); ++i)
                    out << xs[i] << ' ';
                out << '\n';
            }
            ctx.fence();
        }
    }

}

#endif

