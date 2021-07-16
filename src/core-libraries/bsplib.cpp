
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

#include "lpf/core.h"
#include "lpf/bsplib.h"
#include <time.hpp>
#include <assert.hpp>
#include <micromsg.hpp>
#include <linkage.hpp>

#if __cplusplus >= 201103L    
#include <unordered_map>
#include <unordered_set>
#else
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#endif

#include <cstdlib>
#include <stdexcept>
#include <vector>
#include <list>
#include <cstring>
#include <limits>
#include <algorithm>

class _LPFLIB_LOCAL BSPlib 
{
public:
    BSPlib( lpf_t ctx, lpf_pid_t pid, lpf_pid_t nprocs, bool safemode, size_t max_hp_regs )
        : m_ctx( ctx )
        , m_pid( pid )
        , m_nprocs( nprocs )
        , m_start( lpf::Time::now() )
        , m_safemode( safemode )
        , m_mrqueue()
        , m_mpqueue()
        , m_mpqueue_slot( LPF_INVALID_MEMSLOT )
        , m_memreg()
        , m_curregs(EXTRA_REGS)
        , m_maxregs(EXTRA_REGS)
        , m_maxhpregs( max_hp_regs )
        , m_maxmsgs(4*nprocs)
        , m_slot_addrs()
        , m_slot_sizes()
        , m_slot_sizes_slot( LPF_INVALID_MEMSLOT )
        , m_send()
        , m_send_tags()
        , m_send_slot(LPF_INVALID_MEMSLOT)
        , m_send_tags_slot( LPF_INVALID_MEMSLOT )
        , m_recv()
        , m_recv_tags()
        , m_recv_slot(LPF_INVALID_MEMSLOT)
        , m_recv_tags_slot( LPF_INVALID_MEMSLOT )
        , m_put_queue()
        , m_get_queue()
        , m_hpget_queue()
        , m_hpput_queue()
        , m_send_queue()
        , m_hpsend_queue()
        , m_hpget_queue_is_heap(false)
        , m_hpput_queue_is_heap(false)
        , m_hpsend_queue_is_heap(false)
        , m_local_slots()
        , m_outbound_send_payload_remote_offsets(1, 0ul)
        , m_outbound_send_payload_remote_offsets_slot( LPF_INVALID_MEMSLOT )
        , m_inbound_send_payload_local_offsets(1, 0ul)
        , m_inbound_send_payload_local_offsets_slot( LPF_INVALID_MEMSLOT )
        , m_comvol_prev_outbound( nprocs )
        , m_comvol_outbound( 2*nprocs )
        , m_comvol_inbound( 2*nprocs )
        , m_resize(2*m_nprocs)
        , m_comvol_outbound_slot( LPF_INVALID_MEMSLOT )
        , m_comvol_inbound_slot( LPF_INVALID_MEMSLOT )
        , m_resize_slot( LPF_INVALID_MEMSLOT )
        , m_index( m_nprocs )
        , m_send_prev_capacity(0)
        , m_send_tags_prev_capacity(0)
        , m_recv_prev_capacity(0)
        , m_recv_tags_prev_capacity(0)
        , m_outbound_send_payload_remote_offsets_prev_capacity(0)
        , m_inbound_send_payload_local_offsets_prev_capacity(0)
        , m_reregister(true)
        , m_reregister_read(true)
        , m_reregister_slot(LPF_INVALID_MEMSLOT)
        , m_reregister_read_slot(LPF_INVALID_MEMSLOT)
        , m_prev_tag_size(0)
        , m_cur_tag_size(0)
        , m_next_tag_size(0)
        , m_error_mine(0)
        , m_error_mine_slot( LPF_INVALID_MEMSLOT )
        , m_error(0)
        , m_error_slot( LPF_INVALID_MEMSLOT )
        , m_exit(m_nprocs)
        , m_exit_slot( LPF_INVALID_MEMSLOT )
    {
        lpf_err_t rc = lpf_resize_memory_register( m_ctx, m_maxregs );
        if (rc == LPF_ERR_OUT_OF_MEMORY)
            throw std::bad_alloc();
        else if (rc == LPF_ERR_FATAL )
            throw std::runtime_error("Could not initialize");

        rc = lpf_resize_message_queue( m_ctx, m_maxmsgs );
        if (rc == LPF_ERR_OUT_OF_MEMORY)
            throw std::bad_alloc();
        else if (rc == LPF_ERR_FATAL )
            throw std::runtime_error("Could not initialize");

        rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );
        if ( rc != LPF_SUCCESS )
            throw std::runtime_error("Could not initialize");

        m_comvol_outbound_slot = reg_glob_vec( m_comvol_outbound );
        m_comvol_inbound_slot = reg_glob_vec( m_comvol_inbound );
        m_resize_slot = reg_glob_vec( m_resize );
        m_error_mine_slot = reg_glob_pod( m_error_mine );
        m_error_slot = reg_glob_pod( m_error );
        m_exit_slot = reg_glob_vec( m_exit );
        m_reregister_slot = reg_glob_pod( m_reregister );
        m_reregister_read_slot = reg_glob_pod( m_reregister_read );

        rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );
        if( rc != LPF_SUCCESS )
            throw std::runtime_error("Could not initialize");
    }

    ~BSPlib()
    {
        m_ctx = NULL;
    }

    bsplib_err_t end()
    {
        lpf_err_t rc = LPF_SUCCESS;
        if (!has_anyone_else_exited()) 
        {
            m_exit[m_pid] = 1;
            for ( lpf_pid_t i = 0; i < m_nprocs; ++i) {
                if (i != m_pid ) {
                    rc = lpf_put( m_ctx, m_exit_slot, m_pid*sizeof(int), 
                            i, m_exit_slot, m_pid*sizeof(int),
                            sizeof(int), LPF_MSG_DEFAULT);
                    ASSERT( LPF_SUCCESS == rc );
                }
            }
            rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );

            if ( rc == LPF_SUCCESS && has_everyone_exited() )
            {
                // clean-up
                if ( m_send_slot != LPF_INVALID_MEMSLOT ) 
                    lpf_deregister( m_ctx, m_send_slot );
               
                if ( m_send_tags_slot != LPF_INVALID_MEMSLOT ) 
                    lpf_deregister( m_ctx, m_send_tags_slot );
                
                if ( m_outbound_send_payload_remote_offsets_slot != LPF_INVALID_MEMSLOT ) 
                    lpf_deregister( m_ctx, m_outbound_send_payload_remote_offsets_slot );
                
                if ( m_recv_slot != LPF_INVALID_MEMSLOT ) 
                    lpf_deregister( m_ctx, m_recv_slot );

                if ( m_recv_tags_slot != LPF_INVALID_MEMSLOT ) 
                    lpf_deregister( m_ctx, m_recv_tags_slot );

                if ( m_inbound_send_payload_local_offsets_slot != LPF_INVALID_MEMSLOT ) 
                    lpf_deregister( m_ctx, m_inbound_send_payload_local_offsets_slot );

                lpf_deregister( m_ctx, m_comvol_outbound_slot );
                lpf_deregister( m_ctx, m_comvol_inbound_slot );
                lpf_deregister( m_ctx, m_resize_slot );
                lpf_deregister( m_ctx, m_error_mine_slot );
                lpf_deregister( m_ctx, m_error_slot );
                lpf_deregister( m_ctx, m_exit_slot );
                lpf_deregister( m_ctx, m_reregister_slot );
                lpf_deregister( m_ctx, m_reregister_read_slot );

                return BSPLIB_SUCCESS;
            }
        }
        // else don't clean up, because there might still be messages in flight!
        return BSPLIB_ERR_FATAL;
    }

    lpf_pid_t pid() const
    { return m_pid; }

    lpf_pid_t nprocs() const
    { return m_nprocs; }

    double time() const
    { return (lpf::Time::now() - m_start).toSeconds(); }

    bsplib_err_t pushreg( const void * ident, size_t size)
    { 
        if ( ident == NULL && size != 0 )
            return BSPLIB_ERR_NULL_POINTER;

        try {
            std::vector< Memslot > slots( m_safemode?m_nprocs:1 );
            slots[m_safemode?m_pid:0] 
                = Memslot( const_cast<void *>(ident), size);
            m_mrqueue.push_back( slots ); 
        }
        catch (std::bad_alloc & ) {
            return BSPLIB_ERR_OUT_OF_MEMORY;
        }
        return BSPLIB_SUCCESS;
    }

    bsplib_err_t popreg( const void * ident )
    { 
        Memreg::iterator i = m_memreg.find( const_cast<void *>(ident) );
        if ( i == m_memreg.end() 
             || i->second.is_empty() )
            return BSPLIB_ERR_MEMORY_NOT_REGISTERED;

        try {
            m_popreg_dirty.insert( const_cast<void *>(ident) );
            lpf_memslot_t slot 
                = (i->second.back())[m_safemode?m_pid:0].slot;
            m_mpqueue.push_back( slot );
        }
        catch( std::bad_alloc & ) {
            return BSPLIB_ERR_OUT_OF_MEMORY;
        }

        i->second.pop_back();
        return BSPLIB_SUCCESS;
    }

    bsplib_err_t put( lpf_pid_t dst_pid, const void * src, 
            void * dst, size_t offset, size_t nbytes)
    {
        if ( nbytes == 0 )
            return BSPLIB_SUCCESS;

        if ( src == NULL || dst == NULL )
            return BSPLIB_ERR_NULL_POINTER;

        if  ( m_memreg.find(dst) == m_memreg.end() )
            return BSPLIB_ERR_MEMORY_NOT_REGISTERED;

LPFLIB_IGNORE_TAUTOLOGIES
        if ( dst_pid < 0 || dst_pid >= m_nprocs )
            return BSPLIB_ERR_PID_OUT_OF_RANGE;
LPFLIB_RESTORE_WARNINGS

        const std::vector< Memslot > & slot = m_memreg[dst].active_back();

        if ( m_safemode && nbytes > slot[dst_pid].size - 
                std::min( offset, slot[dst_pid].size) )
            return BSPLIB_ERR_MEMORY_ACCESS_OUT_OF_RANGE;

        size_t src_offset = m_send.size();
        const char * bytes = static_cast<const char *>(src);
        try {
            m_send.insert( m_send.end(), bytes, bytes + nbytes );
            queue_put(src_offset, 
                    dst_pid, slot[m_safemode?m_pid:0].slot, offset,
                    nbytes ) ;
        } catch( std::bad_alloc & ) {
            return BSPLIB_ERR_OUT_OF_MEMORY;
        }

        m_comvol_outbound[ dst_pid ].puts += 1;
        return BSPLIB_SUCCESS;
    }

    bsplib_err_t hpput( lpf_pid_t dst_pid, const void * src, 
            void * dst, size_t offset, size_t nbytes)
    {
        if ( nbytes == 0 )
            return BSPLIB_SUCCESS;

        if ( src == NULL || dst == NULL )
            return BSPLIB_ERR_NULL_POINTER;

        if  ( m_memreg.find(dst) == m_memreg.end() )
            return BSPLIB_ERR_MEMORY_NOT_REGISTERED;

LPFLIB_IGNORE_TAUTOLOGIES
        if ( dst_pid < 0 || dst_pid >= m_nprocs )
            return BSPLIB_ERR_PID_OUT_OF_RANGE;
LPFLIB_RESTORE_WARNINGS

        const std::vector< Memslot > & slot = m_memreg[dst].active_back();

        if ( m_safemode && nbytes > slot[dst_pid].size - 
                std::min( offset, slot[dst_pid].size) )
            return BSPLIB_ERR_MEMORY_ACCESS_OUT_OF_RANGE;

        if ( dst_pid == m_pid )
        {
            std::memcpy( static_cast<char *>(dst) + offset, src, nbytes);
            return BSPLIB_SUCCESS;
        }

        UnbufMsg msg;
        msg.pid = dst_pid;
        msg.offset = offset;
        msg.size = nbytes ;
        msg.addr = const_cast<void *>(src);
        msg.slot = slot[m_safemode?m_pid:0].slot;
        try {
            // add the new message to the queue
            m_hpput_queue.push_back( msg );
        } catch ( std::bad_alloc & ) {
            return BSPLIB_ERR_OUT_OF_MEMORY ;
        }
        m_comvol_outbound[ dst_pid ].hpputs += 1;

        // if the hp queue is full
        ASSERT( !m_hpput_queue_is_heap || m_hpput_queue.size() + m_hpget_queue.size() 
                + m_hpsend_queue.size() > m_maxhpregs );
        if ( m_hpput_queue.size() + m_hpget_queue.size() 
                + m_hpsend_queue.size() > m_maxhpregs )
        {
            UnbufMsgQueue::iterator smallest
                = get_smallest_hp_msg( m_hpput_queue, m_hpput_queue_is_heap );

            size_t src_offset = m_send.size();
            const char * bytes = static_cast<const char *>(smallest->addr);
            try {
                m_send.insert( m_send.end(), bytes, bytes + smallest->size );
                queue_put(src_offset, smallest->pid, smallest->slot, 
                        smallest->offset, smallest->size );
            } catch ( std::bad_alloc & ) {
                m_hpput_queue.pop_back();
                std::push_heap( m_hpput_queue.begin(), m_hpput_queue.end(),
                    Smallest() );
                m_comvol_outbound[ dst_pid ].hpputs -= 1;
                return BSPLIB_ERR_OUT_OF_MEMORY;
            }
            m_comvol_outbound[ smallest->pid ].puts += 1;
            m_comvol_outbound[ smallest->pid ].hpputs -= 1;

            remove_hp_msg( m_hpput_queue, m_hpput_queue_is_heap, smallest );
        }

        return BSPLIB_SUCCESS;
    }

    bsplib_err_t get( lpf_pid_t src_pid, const void * src, 
            size_t offset, void * dst, size_t nbytes )
    {
        if ( nbytes == 0 )
            return BSPLIB_SUCCESS;

        if ( src == NULL || dst == NULL )
            return BSPLIB_ERR_NULL_POINTER;

        if  ( m_memreg.find(const_cast<void *>(src)) == m_memreg.end() )
            return BSPLIB_ERR_MEMORY_NOT_REGISTERED;

LPFLIB_IGNORE_TAUTOLOGIES
        if ( src_pid < 0 || src_pid >= m_nprocs )
            return BSPLIB_ERR_PID_OUT_OF_RANGE;
LPFLIB_RESTORE_WARNINGS

        const std::vector< Memslot > & slot 
            = m_memreg[const_cast<void*>(src)].active_back();

        if ( m_safemode && nbytes > slot[src_pid].size - 
                std::min( offset, slot[src_pid].size) )
            return BSPLIB_ERR_MEMORY_ACCESS_OUT_OF_RANGE;

        UnbufMsg msg;
        msg.pid = src_pid;
        msg.offset = offset;
        msg.size = nbytes;
        msg.slot = slot[m_safemode?m_pid:0].slot;
        msg.addr = dst;
        try {
            m_get_queue.push_back( msg );
        } catch ( std::bad_alloc & ) {
            return BSPLIB_ERR_OUT_OF_MEMORY ;
        }
        m_comvol_outbound[ src_pid ].gets += 1;
        m_comvol_outbound[ src_pid ].get_payload += nbytes;
        return BSPLIB_SUCCESS;
    }

    bsplib_err_t hpget( lpf_pid_t src_pid, const void * src, 
            size_t offset, void * dst, size_t nbytes )
    { 
        if ( nbytes == 0 )
            return BSPLIB_SUCCESS;

        if ( src == NULL || dst == NULL )
            return BSPLIB_ERR_NULL_POINTER;

        if  ( m_memreg.find(const_cast<void *>(src)) == m_memreg.end() )
            return BSPLIB_ERR_MEMORY_NOT_REGISTERED;

LPFLIB_IGNORE_TAUTOLOGIES
        if ( src_pid < 0 || src_pid >= m_nprocs )
            return BSPLIB_ERR_PID_OUT_OF_RANGE;
LPFLIB_RESTORE_WARNINGS

        const std::vector< Memslot > & slot 
            = m_memreg[const_cast<void*>(src)].active_back();

        if ( m_safemode && nbytes > slot[src_pid].size - 
                std::min( offset, slot[src_pid].size) )
            return BSPLIB_ERR_MEMORY_ACCESS_OUT_OF_RANGE;

        UnbufMsg msg;
        msg.pid = src_pid;
        msg.offset = offset;
        msg.size = nbytes;
        msg.slot = slot[m_safemode?m_pid:0].slot;
        msg.addr = dst;
        try {
            m_hpget_queue.push_back( msg );
        } catch ( std::bad_alloc & ) {
            return BSPLIB_ERR_OUT_OF_MEMORY ;
        }
        m_comvol_outbound[ src_pid ].hpgets += 1;

        // if the hp queue is full
        ASSERT( !m_hpget_queue_is_heap || m_hpput_queue.size() + m_hpget_queue.size() 
                + m_hpsend_queue.size() > m_maxhpregs );
        if ( m_hpput_queue.size() + m_hpget_queue.size() 
                + m_hpsend_queue.size() > m_maxhpregs )
        {
            UnbufMsgQueue::iterator smallest 
                = get_smallest_hp_msg( m_hpget_queue, m_hpget_queue_is_heap );

            try {
                m_get_queue.push_back( *smallest );
            } catch ( std::bad_alloc & ) {
                m_hpget_queue.pop_back();
                std::push_heap( m_hpget_queue.begin(), m_hpget_queue.end(),
                    Smallest() );
                m_comvol_outbound[ src_pid ].hpgets -= 1;
                return BSPLIB_ERR_OUT_OF_MEMORY;
            }
            m_comvol_outbound[ smallest->pid ].gets += 1;
            m_comvol_outbound[ smallest->pid ].get_payload += smallest->size;
            m_comvol_outbound[ smallest->pid ].hpgets -= 1;

            remove_hp_msg( m_hpget_queue, m_hpget_queue_is_heap, smallest );
        }
        return BSPLIB_SUCCESS;

    }

    size_t set_tagsize( size_t tagsize )
    { m_next_tag_size = tagsize; return m_cur_tag_size; }

    bsplib_err_t send( lpf_pid_t dst_pid, const void * tag, 
            const void * payload, size_t nbytes )
    {
        if ( m_cur_tag_size != 0 && tag == NULL  )
            return BSPLIB_ERR_NULL_POINTER;

        if ( nbytes != 0 && payload == NULL )
            return BSPLIB_ERR_NULL_POINTER;

LPFLIB_IGNORE_TAUTOLOGIES
        if ( dst_pid < 0 || dst_pid >= m_nprocs )
            return BSPLIB_ERR_PID_OUT_OF_RANGE;
LPFLIB_RESTORE_WARNINGS

        Send msg;
        msg.pid = dst_pid;
        msg.tag_offset = m_send_tags.size();
        msg.payload_offset =  m_send.size();
        msg.size = nbytes;

        const char * tag_bytes = static_cast<const char *>(tag);
        const char * payload_bytes  = static_cast<const char *>(payload);

        try {
            m_send_tags.insert( m_send_tags.end(),
                    tag_bytes, tag_bytes + m_cur_tag_size );
            m_send.insert( m_send.end(),
                    payload_bytes, payload_bytes + nbytes );

            m_send_queue.push_back( msg );
        } catch ( std::bad_alloc & ) {
            return BSPLIB_ERR_OUT_OF_MEMORY ;
        }

        m_comvol_outbound[ dst_pid ].sends += 1;
        m_comvol_outbound[ dst_pid ].send_payload += nbytes;
        return BSPLIB_SUCCESS;
    }

    bsplib_err_t hpsend( lpf_pid_t dst_pid, const void * tag, 
            const void * payload, size_t nbytes )
    {
        if ( m_cur_tag_size != 0 && tag == NULL  )
            return BSPLIB_ERR_NULL_POINTER;

        if ( nbytes != 0 && payload == NULL )
            return BSPLIB_ERR_NULL_POINTER;

LPFLIB_IGNORE_TAUTOLOGIES
        if ( dst_pid < 0 || dst_pid >= m_nprocs )
            return BSPLIB_ERR_PID_OUT_OF_RANGE;
LPFLIB_RESTORE_WARNINGS

        HpSend msg;
        msg.pid = dst_pid;
        msg.tag_offset = m_send_tags.size();
        const char * tag_bytes = static_cast<const char *>(tag);
        msg.payload = const_cast<void *>(payload);
        msg.size = nbytes;
        try {
            m_send_tags.insert( m_send_tags.end(), 
                    tag_bytes, tag_bytes + m_cur_tag_size );
            m_hpsend_queue.push_back( msg );
        } catch ( std::bad_alloc & ) {
            return BSPLIB_ERR_OUT_OF_MEMORY ;
        }
        m_comvol_outbound[ dst_pid ].hpsends += 1;
        m_comvol_outbound[ dst_pid ].send_payload += nbytes;

        // if the hp queue is full
        ASSERT( !m_hpsend_queue_is_heap || m_hpput_queue.size() + m_hpget_queue.size() 
                + m_hpsend_queue.size() > m_maxhpregs );
        if ( m_hpput_queue.size() + m_hpget_queue.size() 
                + m_hpsend_queue.size() > m_maxhpregs )
        {
            HpSendQueue::iterator smallest 
                = get_smallest_hp_msg( m_hpsend_queue, m_hpsend_queue_is_heap );

            Send s;
            s.pid = smallest->pid;
            s.tag_offset = smallest->tag_offset;
            s.payload_offset = m_send.size();
            s.size = smallest->size;
            const char * payload_bytes
                = static_cast<const char *>(smallest->payload);
            try {
                m_send.insert( m_send.end(),
                        payload_bytes, payload_bytes + s.size );
                m_send_queue.push_back( s );
            }
            catch( std::bad_alloc & )
            {
                m_hpsend_queue.pop_back();
                std::push_heap( m_hpsend_queue.begin(), m_hpsend_queue.end(),
                    Smallest()  );
                m_comvol_outbound[ dst_pid ].hpsends -= 1;
                m_comvol_outbound[ dst_pid ].send_payload -= nbytes;
                return BSPLIB_ERR_OUT_OF_MEMORY;
            }
            m_comvol_outbound[ smallest->pid ].sends += 1;
            m_comvol_outbound[ smallest->pid ].hpsends -= 1;

            remove_hp_msg( m_hpsend_queue, m_hpsend_queue_is_heap, smallest );
        }
                
        return BSPLIB_SUCCESS;
    }


    bsplib_err_t qsize( size_t * nmessages, size_t *accum_bytes )
    {   
        if (nmessages != NULL)
            *nmessages = m_inbound_send_payload_local_offsets.size() - 1;
        
        if (accum_bytes != NULL)
            *accum_bytes = m_inbound_send_payload_local_offsets.back();
        return BSPLIB_SUCCESS;
    }

    bsplib_err_t get_tag( size_t * status, void * tag )
    {
        if (m_inbound_send_payload_local_offsets.size() == 1 )
        {
            if (status != NULL)
                *status = static_cast<size_t>(-1);
        }
        else
        {
            ASSERT( m_inbound_send_payload_local_offsets.size() >= 2);
            size_t msg = m_inbound_send_payload_local_offsets.size() - 2;
            if (m_prev_tag_size > 0 && tag != NULL) {
                size_t tag_offset = m_prev_tag_size * msg;
                const char * ptr = &m_recv_tags[ tag_offset ];
                memcpy( tag, ptr, m_prev_tag_size);
            }

            if (status != NULL ) {
                size_t offset = m_inbound_send_payload_local_offsets[msg];
                size_t size = m_inbound_send_payload_local_offsets[msg+1] - offset;

                *status = size;
            }
        }
        return BSPLIB_SUCCESS;
    }

    bsplib_err_t move( void * payload, size_t reception_bytes )
    {
        if ( m_inbound_send_payload_local_offsets.size() == 1 )
            return BSPLIB_ERR_EMPTY_MESSAGE_QUEUE;

        if ( payload == NULL && reception_bytes != 0 )
            return BSPLIB_ERR_NULL_POINTER;

        ASSERT( m_inbound_send_payload_local_offsets.size() >= 2);

        size_t msg = m_inbound_send_payload_local_offsets.size() - 2;
        size_t offset = m_inbound_send_payload_local_offsets[msg];
        size_t size = m_inbound_send_payload_local_offsets[msg+1] - offset;
        size = std::min( size, reception_bytes );

        if ( size > 0) {
            const char * buf = & m_recv[offset];
            memcpy( payload, buf, size );
        }
        
        m_inbound_send_payload_local_offsets.pop_back();
        return BSPLIB_SUCCESS;
    }

    size_t hpmove( const void ** tag_ptr, const void **payload_ptr )
    {
        if ( m_inbound_send_payload_local_offsets.size() == 1 )
            return static_cast<size_t>(-1);

        ASSERT( m_inbound_send_payload_local_offsets.size() >= 2);

        size_t msg = m_inbound_send_payload_local_offsets.size() - 2;
        size_t offset = m_inbound_send_payload_local_offsets[msg];
        size_t size = m_inbound_send_payload_local_offsets[msg+1] - offset;

        if ( tag_ptr != NULL ) {
            if ( m_prev_tag_size > 0 ) {
                *tag_ptr = static_cast<const void *>( 
                                &m_recv_tags[ msg * m_prev_tag_size ]
                        );
            }
            else {
                *tag_ptr = NULL;
            }
        }

        if ( payload_ptr != NULL)
            *payload_ptr = static_cast<const void *>( &m_recv[ offset ] );

        m_inbound_send_payload_local_offsets.pop_back();

        return size;
    }

    bsplib_err_t sync() 
    {
        lpf_err_t rc = LPF_SUCCESS;

        /// 0. Allocate memory before we start
        const size_t newregs = m_mrqueue.size();
        const size_t delregs = m_mpqueue.size();
        try { 
            if ( m_outbound_send_payload_remote_offsets.capacity()
                    < m_send_queue.size() + m_hpsend_queue.size() )
            {
                m_outbound_send_payload_remote_offsets.reserve(
                   std::max( 2* m_outbound_send_payload_remote_offsets.capacity()
                           , m_send_queue.size() + m_hpsend_queue.size() )
                );
            }

            m_outbound_send_payload_remote_offsets.resize( 
                        m_send_queue.size() + m_hpsend_queue.size()
            );

            if ( m_local_slots.capacity() < m_hpput_queue.size()
                    + m_hpget_queue.size() + m_hpsend_queue.size() )
            {
                m_local_slots.reserve( 
                        std::max( 2* m_local_slots.capacity(),
                                  m_hpput_queue.size() + m_hpget_queue.size() 
                                  + m_hpsend_queue.size() 
                            ) );
            }

            if ( m_memreg.bucket_count() * m_memreg.max_load_factor()
                    < m_memreg.size() + newregs )
            {
                size_t buckets = m_memreg.bucket_count();
                float load_factor = m_memreg.max_load_factor();

                size_t required = static_cast<size_t>(
                        1 + m_memreg.size() + newregs / load_factor
                        );
                
                m_memreg.rehash( std::max( 2*buckets, required ) );
            }

            if (m_safemode) {
                if ( m_slot_sizes.capacity() < newregs * m_nprocs )
                {
                    m_slot_sizes.reserve( std::max( 2*m_slot_sizes.capacity(),
                                newregs * m_nprocs ) );
                }
                m_slot_sizes.resize( newregs * m_nprocs );
            

                if ( m_slot_addrs.capacity() < newregs )
                {
                    m_slot_addrs.reserve( 
                            std::max( 2*m_slot_addrs.capacity(), newregs ) 
                    );
                }
                m_slot_addrs.resize(newregs);

                if ( m_mpqueue.capacity() < m_mpqueue.size() * m_nprocs )
                {
                    m_mpqueue.reserve( std::max( 2*m_mpqueue.capacity(),
                                delregs * m_nprocs ) );
                }
            }
            
        } catch ( std::bad_alloc & ) {
            if (m_safemode)
                broadcast_error();
            else
                throw;
        }
            
        if (m_safemode)
        {
            ////// SAFETY BARRIER 0a /////
            rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );
            if ( LPF_SUCCESS != rc || has_anyone_else_exited() )
                return BSPLIB_ERR_FATAL;

            if ( m_error )
                return BSPLIB_ERR_OUT_OF_MEMORY;
        }
 
        // 1. exchange info on communication volume in order to compute
        // the global maximum on inbound and outbound messages 
        // This also allows us to allocate memory before we make
        // irreversible changes.


        // compute total outbound
        ComVol total_outbound;
        for ( lpf_pid_t i = 0; i < m_nprocs; ++i) {
            ComVol & out = m_comvol_outbound[i];
            const ComVol & prev = m_comvol_prev_outbound[i];

            if (newregs > 0 || !(out < prev) ) out.growth = 1;

            total_outbound += out;
            out.newregs = newregs;
            out.delregs = delregs;
            out.next_tag_size = m_next_tag_size;
            out.compress();
        }
        total_outbound.compress();

        m_comvol_outbound[m_nprocs] = total_outbound;
        for ( lpf_pid_t i = 0; i < m_nprocs; ++i )
        {
            size_t src_offset = m_comvol_outbound[i].micro_msg 
                - reinterpret_cast<char *>(m_comvol_outbound.data());
            size_t size = m_comvol_outbound[i].micro_msg_size;
            size_t dst_offset = m_comvol_inbound[m_pid].micro_msg
                - reinterpret_cast<char *>(m_comvol_inbound.data());

            // communicate volume to receiving process
            rc = lpf_put( m_ctx, m_comvol_outbound_slot, src_offset,
                i, m_comvol_inbound_slot, dst_offset,
                size, LPF_MSG_DEFAULT );

            ASSERT( LPF_SUCCESS == rc );

            // broadcast total outbound
            src_offset = m_comvol_outbound[m_nprocs].micro_msg 
                - reinterpret_cast<char *>(m_comvol_outbound.data());
            size = m_comvol_outbound[m_nprocs].micro_msg_size;
            dst_offset = m_comvol_inbound[m_nprocs + m_pid].micro_msg
                - reinterpret_cast<char *>(m_comvol_inbound.data());
            rc = lpf_put( m_ctx, m_comvol_outbound_slot, src_offset,
                i, m_comvol_inbound_slot, dst_offset,
                size, LPF_MSG_DEFAULT );

            ASSERT( LPF_SUCCESS == rc );
        }
///////////////// BARRIER 1 //////////////////////////////////////        
        rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );

        if ( LPF_SUCCESS != rc || has_anyone_else_exited() )
            return BSPLIB_ERR_FATAL;

        // uncompress 
        for ( lpf_pid_t i = 0; i < 2*m_nprocs; ++i)
            m_comvol_inbound[i].uncompress();

        // check whether the number of push and popregs are the same
        for ( lpf_pid_t i = 0; i < m_nprocs; ++i)
        {
            if ( newregs != m_comvol_inbound[i].newregs )
                return BSPLIB_ERR_PUSHREG_MISMATCH;
        }
        for ( lpf_pid_t i = 0; i < m_nprocs; ++i)
        {
            if ( delregs != m_comvol_inbound[i].delregs )
                return BSPLIB_ERR_POPREG_MISMATCH;
        }
        for ( lpf_pid_t i = 0; i < m_nprocs; ++i)
        {
            if ( m_next_tag_size != m_comvol_inbound[i].next_tag_size )
                return BSPLIB_ERR_TAGSIZE_MISMATCH;
        }


        // compute total inbound
        ComVol total_inbound;
        bool any_gets = false;
        bool any_sends = false;
        bool any_growth = false;
        for ( lpf_pid_t i = 0; i < m_nprocs; ++i) {
            total_inbound += m_comvol_inbound[i];

            const ComVol & total = m_comvol_inbound[m_nprocs + i];
            any_gets   = any_gets || (total.gets > 0u);
            any_sends  = any_sends || (total.sends > 0u || total.hpsends > 0u );
            any_growth = any_growth || total.growth;
        }

        if ( m_safemode || any_growth || any_sends )  {
            // compute the maximum outbound traffic over all processes
            {                   
                // compute how many messages this process should at least allocate
                size_t maxmsgs = 4*m_nprocs + total_outbound.nmsgs() + total_inbound.nmsgs();

                // compute how many local registers this process should at least allocate
                size_t maxregs = total_outbound.hpputs + total_outbound.hpsends
                    + total_outbound.hpgets;
                ASSERT( maxregs <= m_maxhpregs );

               // broadcast this process's needs for messages and registers
               // communicate where sending processes can put BSMP messages
               //
               // store the received 'sends' after the data from the gets
                size_t send_offset = total_outbound.get_payload;
                size_t tag_offset = 0;
                for ( size_t i = 0 ; i < m_nprocs; ++i )
                {
                    m_resize[m_nprocs + i].msgs = maxmsgs;
                    m_resize[m_nprocs + i].regs = maxregs;
                    m_resize[m_nprocs + i].send_offset = send_offset;
                    m_resize[m_nprocs + i].tag_offset = tag_offset;
                    m_resize[m_nprocs + i].compress();
                    send_offset += m_comvol_inbound[i].send_payload;
                    tag_offset += m_comvol_inbound[i].tags(); 
                }

                for ( lpf_pid_t i = 0; i < m_nprocs; ++i)
                {
                    size_t src_offset = m_resize[m_nprocs + i].micro_msg 
                        - reinterpret_cast<char *>(m_resize.data());
                    size_t size = m_resize[m_nprocs + i].micro_msg_size;
                    size_t dst_offset = m_resize[m_pid].micro_msg
                        - reinterpret_cast<char *>(m_resize.data());

                    rc = lpf_put( m_ctx, m_resize_slot, src_offset, 
                            i, m_resize_slot, dst_offset, size,
                            LPF_MSG_DEFAULT );
                    ASSERT( rc == LPF_SUCCESS );
                }

    ///////////////// BARRIER 2 //////////////////////////////////////        
                rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );

                if ( LPF_SUCCESS != rc )
                    return BSPLIB_ERR_FATAL;

                // uncompress m_resize
                for ( lpf_pid_t i = 0; i < m_nprocs; ++i)
                    m_resize[i].uncompress(); 

                // the sending process computes the offsets of bsmp messages 
                // on the receiver
                size_t m = 0;
                std::fill( m_index.begin(), m_index.end(), 0u );
                for ( size_t i = 0; i < m_send_queue.size(); ++i, ++m)
                {
                    Send msg = m_send_queue[i];
                    m_outbound_send_payload_remote_offsets[ m ] = 
                        m_index[ msg.pid ] + m_resize[ msg.pid ].send_offset ;
                    m_index[ msg.pid ] += msg.size;
                }
                for ( size_t i = 0; i < m_hpsend_queue.size(); ++i, ++m)
                {
                    HpSend msg = m_hpsend_queue[i];
                    m_outbound_send_payload_remote_offsets[ m ] = 
                        m_index[ msg.pid ] + m_resize[ msg.pid ].send_offset ;
                    m_index[ msg.pid ] += msg.size;
                }
            }

            // compute the global maximum of required messages and registers.
            size_t maxregs = 0;
            size_t maxmsgs = 0;
            for  (lpf_pid_t i = 0; i < m_nprocs; ++i )
            {
                maxregs = std::max( maxregs, m_resize[i].regs);
                maxmsgs = std::max( maxmsgs, m_resize[i].msgs);
            }
            maxregs += m_curregs + newregs;
            // maxmsgs and maxregs have same values on all processes

            /// 2. Now do resize the various communication buffers
            /// if the current allocation is insufficient
            if ( m_maxregs < maxregs || m_maxmsgs < maxmsgs )
            {
                if ( m_maxregs < maxregs ) {
                    // make the number of registers grows only as much as is needed, because
                    // they are a scarce resource
                    m_maxregs = maxregs;
                }

                rc = lpf_resize_memory_register( m_ctx, m_maxregs );
                if ( LPF_SUCCESS != rc ) {
                    if (m_safemode)
                        broadcast_error();
                    else
                        throw std::bad_alloc();
                }

                if ( m_maxmsgs < maxmsgs )
                    m_maxmsgs = std::max( 2 * m_maxmsgs, maxmsgs );

                rc = lpf_resize_message_queue( m_ctx, m_maxmsgs );
                if ( LPF_SUCCESS != rc ) {
                    if (m_safemode)
                        broadcast_error();
                    else
                        throw std::bad_alloc();
                }
            
                // effectuate the resize of buffers of PlatformBSP
                ////// MEMORY EXPANSION BARRIER 2a /////
                rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );
                if ( LPF_SUCCESS != rc )
                    return BSPLIB_ERR_FATAL;

                if ( m_safemode && m_error ) // someone went ouf of memory
                    return BSPLIB_ERR_OUT_OF_MEMORY;
            }
        }

        try { 
            if ( m_recv.size() < total_outbound.get_payload
                    +  total_inbound.send_payload )
            {
                m_recv.resize( std::max<size_t>( 2*m_recv.size(),
                  total_outbound.get_payload +  total_inbound.send_payload 
                ));
            }
            if ( m_recv_tags.size() < total_inbound.tags() * m_cur_tag_size )
            {
                m_recv_tags.resize( 
                        std::max( 2*m_recv_tags.size(), 
                            total_inbound.tags() * m_cur_tag_size ) 
                        );
            }

            if ( m_inbound_send_payload_local_offsets.capacity()
                    < total_inbound.tags() + 1) 
            {

                m_inbound_send_payload_local_offsets.reserve(
                     std::max( 2*m_inbound_send_payload_local_offsets.capacity(),
                               total_inbound.tags() + 1 ) );
            }
                
            m_inbound_send_payload_local_offsets.resize( 
                    total_inbound.tags() + 1 );
            m_inbound_send_payload_local_offsets[total_inbound.tags()]
                = total_outbound.get_payload + total_inbound.send_payload;

        } catch ( std::bad_alloc & ) {
            if (m_safemode)
                broadcast_error();
            else
                throw;
        }

        if (m_safemode)
        {
            ////// SAFETY BARRIER 2b////
            rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );
            if ( LPF_SUCCESS != rc )
                return BSPLIB_ERR_FATAL;

            if ( m_error ) // someone went out of memory
                return BSPLIB_ERR_OUT_OF_MEMORY;
        }


        // 3. check bsp_pop_reg's
        if (m_safemode)
        {
            m_mpqueue.resize( delregs * m_nprocs );
            if (m_pid != 0)
            {
                memcpy( m_mpqueue.data() + delregs * m_pid,
                        m_mpqueue.data(), sizeof(lpf_memslot_t) * delregs );
            }
      
            m_mpqueue_slot = reg_glob_vec( m_mpqueue );

            ////// SAFETY BARRIER 2c////
            rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );
            if ( LPF_SUCCESS != rc )
                return BSPLIB_ERR_FATAL;

            for ( lpf_pid_t i = 0; i < m_nprocs; ++i )
            {
                if (i != m_pid ) {
                    rc = lpf_put( m_ctx, 
                            m_mpqueue_slot, sizeof(size_t) * delregs * m_pid,
                            i, m_mpqueue_slot, sizeof(size_t) * delregs * m_pid,
                            sizeof(size_t) * delregs, LPF_MSG_DEFAULT );
                    ASSERT( rc == LPF_SUCCESS );
                }
            }

            ////// SAFETY BARRIER 2d////
            rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );
            if ( LPF_SUCCESS != rc )
                return BSPLIB_ERR_FATAL;

            for ( size_t r = 0; r < delregs ; ++r )
            {
                lpf_memslot_t slot = m_mpqueue[r];
                for (size_t p = 0; p < m_nprocs; ++p) 
                {
                    if (slot != m_mpqueue[r + p*delregs])
                        m_error = 1;
                }
            }
            if ( m_error )
            {
                broadcast_error();
            }

            ////// SAFETY BARRIER 2e////
            rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );
            if ( LPF_SUCCESS != rc )
                return BSPLIB_ERR_FATAL;

            rc = lpf_deregister( m_ctx, m_mpqueue_slot );
            ASSERT( LPF_SUCCESS == rc );

            m_mpqueue.resize( delregs );

            if ( m_error )
                return BSPLIB_ERR_POPREG_MISMATCH;
        }


        /////// Check whether we need reregistration of communication buffers
        // FIXME: Reduce the number of syncs here
        m_reregister_read 
            = m_send.capacity() > m_send_prev_capacity
           || m_send_tags.capacity() > m_send_tags_prev_capacity 
           || m_recv.capacity() > m_recv_prev_capacity
           || m_recv_tags.capacity() > m_recv_tags_prev_capacity
           || m_outbound_send_payload_remote_offsets.capacity()
                > m_outbound_send_payload_remote_offsets_prev_capacity
           || m_inbound_send_payload_local_offsets.capacity() 
                > m_inbound_send_payload_local_offsets_prev_capacity;

        m_reregister = false;

        ////////////// BARRIER ///////////////////
        rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );
        if ( LPF_SUCCESS != rc )
            return BSPLIB_ERR_FATAL;

        if (m_reregister_read) {
            for ( lpf_pid_t i = 0; i < m_nprocs; ++i)
                lpf_put( m_ctx, m_reregister_read_slot, 0, 
                        i, m_reregister_slot, 0, sizeof(m_reregister), 
                        LPF_MSG_DEFAULT );
        }
        ////////////// BARRIER ///////////////////
        rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );
        if ( LPF_SUCCESS != rc )
            return BSPLIB_ERR_FATAL;
        
        if (m_reregister) {

            if ( m_send_slot != LPF_INVALID_MEMSLOT ) {
                rc = lpf_deregister( m_ctx, m_send_slot );
                ASSERT( LPF_SUCCESS == rc );
                m_send_slot = LPF_INVALID_MEMSLOT;
            }

            if ( m_send_tags_slot != LPF_INVALID_MEMSLOT ) {
                rc = lpf_deregister( m_ctx, m_send_tags_slot );
                ASSERT( LPF_SUCCESS == rc );
                m_send_tags_slot = LPF_INVALID_MEMSLOT;
            }

            if ( m_outbound_send_payload_remote_offsets_slot != LPF_INVALID_MEMSLOT ) {
                rc = lpf_deregister( m_ctx, m_outbound_send_payload_remote_offsets_slot );
                ASSERT( LPF_SUCCESS == rc );
                m_outbound_send_payload_remote_offsets_slot = LPF_INVALID_MEMSLOT ;
            }

            if ( m_recv_slot != LPF_INVALID_MEMSLOT ) {
                rc = lpf_deregister( m_ctx, m_recv_slot );
                ASSERT( LPF_SUCCESS == rc );
                m_recv_slot = LPF_INVALID_MEMSLOT;
            }

            if ( m_recv_tags_slot != LPF_INVALID_MEMSLOT ) { 
                rc = lpf_deregister( m_ctx, m_recv_tags_slot );
                ASSERT( LPF_SUCCESS == rc );
                m_recv_tags_slot = LPF_INVALID_MEMSLOT;
            }

            if ( m_inbound_send_payload_local_offsets_slot != LPF_INVALID_MEMSLOT ) {
                rc = lpf_deregister( m_ctx, m_inbound_send_payload_local_offsets_slot );
                ASSERT( LPF_SUCCESS == rc );
                m_inbound_send_payload_local_offsets_slot = LPF_INVALID_MEMSLOT ;
            }

            m_send_slot = reg_glob_vec( m_send );
            m_send_prev_capacity = m_send.capacity();

            m_send_tags_slot = reg_glob_vec( m_send_tags );
            m_send_tags_prev_capacity = m_send_tags.capacity();

            m_outbound_send_payload_remote_offsets_slot
                = reg_glob_vec( m_outbound_send_payload_remote_offsets );
            m_outbound_send_payload_remote_offsets_prev_capacity
                = m_outbound_send_payload_remote_offsets.capacity();

            m_recv_slot = reg_glob_vec( m_recv );
            m_recv_prev_capacity = m_recv.capacity();

            m_recv_tags_slot = reg_glob_vec( m_recv_tags );
            m_recv_tags_prev_capacity = m_recv_tags.capacity();

            m_inbound_send_payload_local_offsets_slot
                = reg_glob_vec( m_inbound_send_payload_local_offsets );
            m_inbound_send_payload_local_offsets_prev_capacity
                = m_inbound_send_payload_local_offsets.capacity();

            rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );
            if ( LPF_SUCCESS != rc )
                return BSPLIB_ERR_FATAL;
        }

 
        if ( m_safemode || any_gets ) {
             // 4a. Handle lpf_get's
            {   size_t offset = 0;
                for ( size_t i = 0 ; i < m_get_queue.size(); ++i )
                {
                    const UnbufMsg msg = m_get_queue[i];
                   
                    rc = lpf_get( m_ctx, msg.pid, msg.slot, msg.offset,
                            m_recv_slot, offset, msg.size, LPF_MSG_DEFAULT );
                    ASSERT( rc == LPF_SUCCESS );

                    offset += msg.size;
                }
            }
     
    ///////////////// BARRIER 3 //////////////////////////////////////        
            rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );
            if ( LPF_SUCCESS != rc )
                return BSPLIB_ERR_FATAL;

            {   size_t offset = 0;
                for ( size_t i = 0 ; i < m_get_queue.size(); ++i )
                {
                    const UnbufMsg msg = m_get_queue[i];
                    memcpy( msg.addr, &m_recv[offset], msg.size );
                    offset += msg.size;
                }
            }
        }

      
        // 4b. Handle bsp_hpget's
        for ( size_t i = 0 ; i < m_hpget_queue.size(); ++i )
        {
            const UnbufMsg msg = m_hpget_queue[i];
            
            lpf_memslot_t dst_slot = LPF_INVALID_MEMSLOT;
            rc = lpf_register_local( m_ctx, msg.addr, msg.size, &dst_slot );
            ASSERT( rc == LPF_SUCCESS );
            m_curregs += 1;

            m_local_slots.push_back( dst_slot );

            rc = lpf_get( m_ctx, msg.pid, msg.slot, msg.offset,
                    dst_slot, 0, msg.size, LPF_MSG_DEFAULT );
            ASSERT( rc == LPF_SUCCESS );
        }

                
        // 5. Handle bsp_hpput's and lpf_put's
        for ( size_t i = 0 ; i < m_hpput_queue.size(); ++i )
        {
            const UnbufMsg msg = m_hpput_queue[i];
            
            lpf_memslot_t src_slot = LPF_INVALID_MEMSLOT;
            rc = lpf_register_local( m_ctx, msg.addr, msg.size, &src_slot );
            ASSERT( rc == LPF_SUCCESS );
            m_curregs += 1;

            m_local_slots.push_back( src_slot );

            rc = lpf_put( m_ctx, src_slot, 0,
                    msg.pid, msg.slot, msg.offset, msg.size, 
                    LPF_MSG_DEFAULT );
            ASSERT( rc == LPF_SUCCESS );
        }

        for ( size_t i = 0 ; i < m_put_queue.size();  )
        {
            lpf_pid_t pid;
            lpf_memslot_t dst_slot;
            size_t src_offset, dst_offset, size;

            i = access_put( src_offset, pid, dst_slot, dst_offset, size, i, true);
            
            rc = lpf_put( m_ctx, m_send_slot, src_offset,
                    pid, dst_slot, dst_offset, size, 
                    LPF_MSG_DEFAULT );
            ASSERT( rc == LPF_SUCCESS );
        }

        // 6. Handle bsp_send's and bsp_hpsend's
        m_index.clear(); m_index.resize( m_nprocs );
        for ( size_t i = 0, m = 0; i < m_send_queue.size(); ++i, ++m )
        {
            const Send msg = m_send_queue[i];
            
            rc = lpf_put( m_ctx, m_send_slot, msg.payload_offset,
                    msg.pid, m_recv_slot, 
                    m_outbound_send_payload_remote_offsets[m],
                    msg.size, 
                    LPF_MSG_DEFAULT );
            ASSERT( rc == LPF_SUCCESS );

            if ( m_cur_tag_size > 0 ) 
            {
                size_t tag_offset = m_index[msg.pid] + m_resize[msg.pid].tag_offset;
                rc = lpf_put( m_ctx, m_send_tags_slot, msg.tag_offset,
                        msg.pid, m_recv_tags_slot, 
                        tag_offset * m_cur_tag_size,
                        m_cur_tag_size,
                        LPF_MSG_DEFAULT );
                ASSERT( rc == LPF_SUCCESS );
            }
            m_index[ msg.pid ] += 1;
        }
        for ( size_t i = 0, m = m_send_queue.size(); i < m_hpsend_queue.size(); ++i, ++m )
        {
            const HpSend msg = m_hpsend_queue[i];

            lpf_memslot_t src_slot = LPF_INVALID_MEMSLOT;
            rc = lpf_register_local( m_ctx, msg.payload, msg.size, &src_slot );
            ASSERT( rc == LPF_SUCCESS );
            m_curregs += 1;

            m_local_slots.push_back( src_slot );
            
            rc = lpf_put( m_ctx, src_slot, 0,
                    msg.pid, m_recv_slot, 
                    m_outbound_send_payload_remote_offsets[m],
                    msg.size, 
                    LPF_MSG_DEFAULT );
            ASSERT( rc == LPF_SUCCESS );

            if ( m_cur_tag_size > 0 ) 
            {
                size_t tag_offset = m_index[msg.pid] + m_resize[msg.pid].tag_offset;
                rc = lpf_put( m_ctx, m_send_tags_slot, msg.tag_offset,
                        msg.pid, m_recv_tags_slot, 
                        tag_offset * m_cur_tag_size,
                        m_cur_tag_size,
                        LPF_MSG_DEFAULT );
                ASSERT( rc == LPF_SUCCESS );
            }
            m_index[ msg.pid ] += 1;
        }
        for (lpf_pid_t i = 0; i < m_nprocs; ++i) 
        {
            size_t send_offset = i == 0? 0 : m_index[i-1];
            size_t recv_offset = m_resize[ i ].tag_offset ;
            rc = lpf_put( m_ctx, 
                    m_outbound_send_payload_remote_offsets_slot, 
                    send_offset * sizeof(size_t), 
                    i, m_inbound_send_payload_local_offsets_slot, 
                    recv_offset * sizeof(size_t),
                    sizeof(size_t) * m_comvol_outbound[i].tags(),
                    LPF_MSG_DEFAULT );
            ASSERT( LPF_SUCCESS == rc );
        }

///////////////// BARRIER 4 //////////////////////////////////////        
        rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );
        if ( LPF_SUCCESS != rc )
            return BSPLIB_ERR_FATAL;


        // 8a. effectuate bsp_pop_regs
        ASSERT( m_mpqueue.size() == delregs );
        for ( size_t i = 0; i < delregs ; ++i )
        {
            lpf_memslot_t slot = m_mpqueue[i];
            rc = lpf_deregister( m_ctx, slot );
            ASSERT(rc == LPF_SUCCESS );
        }
        ASSERT( m_curregs >= m_mpqueue.size() + m_local_slots.size() );
        m_curregs -= m_mpqueue.size();
        m_mpqueue.clear();

        for (DirtyAddrs::iterator i = m_popreg_dirty.begin();
                i != m_popreg_dirty.end(); ++i )
        {
            if ( m_memreg[ *i ].is_empty() )
                m_memreg.erase( *i );
            else
                m_memreg[*i].clear_tail();
        }
        m_popreg_dirty.clear();

        //7. Do the memory registrations by bsp_push_reg
        for (size_t n = 0; n < newregs; ++n )
        {
            MemregQueue :: iterator i = m_mrqueue.begin();
            void * ident = (*i)[m_safemode?m_pid:0].addr;
            size_t size = (*i)[m_safemode?m_pid:0].size;
            if (m_safemode) {
                m_slot_sizes[ newregs * m_pid + n ] = size;
                m_slot_addrs[ n ] = i;
            }
            lpf_memslot_t slot = LPF_INVALID_MEMSLOT;
            rc = lpf_register_global( m_ctx, ident, size, &slot );
            ASSERT( rc == LPF_SUCCESS );
            (*i)[m_safemode?m_pid:0].slot = slot;

            // insert the new entry to the address lookup table
            MemregStack empty_stack;
            Memreg::value_type item( ident, empty_stack );

            MemregStack & regstack = m_memreg.insert( item ).first->second;
            regstack.push( i, m_mrqueue);
            m_curregs += 1;
        }
        ASSERT( m_mrqueue.empty() );

        if ( m_safemode ) 
        {
            m_slot_sizes_slot = reg_glob_vec( m_slot_sizes );

            ////// SAFETY BARRIER 4a////
            rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );
            if ( LPF_SUCCESS != rc )
                return BSPLIB_ERR_FATAL;

            for (size_t i = 0; i < m_nprocs; ++i)
            {
                if ( i != m_pid ) {
                    rc = lpf_put( m_ctx, m_slot_sizes_slot, 
                                sizeof(size_t) * newregs * m_pid,
                            i, m_slot_sizes_slot,
                                sizeof(size_t) * newregs * m_pid,
                            sizeof(size_t) * newregs, LPF_MSG_DEFAULT );
                    ASSERT( LPF_SUCCESS == rc );
                }
            }

            ////// SAFETY BARRIER 4b////
            rc = lpf_sync( m_ctx, LPF_SYNC_DEFAULT );
            if ( LPF_SUCCESS != rc )
                return BSPLIB_ERR_FATAL;

            for (size_t n = 0; n < newregs; ++n )
            {
                MemregQueue::iterator i = m_slot_addrs[n];
                for (lpf_pid_t p = 0; p < m_nprocs; ++p) {
                    (*i)[ p ].size = m_slot_sizes[ p * newregs + n ];
                }
            }

            rc = lpf_deregister( m_ctx, m_slot_sizes_slot );
            ASSERT( LPF_SUCCESS == rc );
            m_slot_sizes_slot = LPF_INVALID_MEMSLOT ;
        }


        // 10. Clean up the local registers 
        for ( size_t i = 0; i < m_local_slots.size(); ++i)
        {
            rc = lpf_deregister( m_ctx, m_local_slots[i] );
            ASSERT( rc == LPF_SUCCESS );
        }
        ASSERT( m_curregs >= m_local_slots.size() );
        m_curregs -= m_local_slots.size();
        m_local_slots.clear();
        // note: Now m_curregs has the same value on all processes


        // 11: Reset temporary storage
        ComVol emptyVol;
        std::copy( m_comvol_outbound.begin(), m_comvol_outbound.begin() + m_nprocs,
                m_comvol_prev_outbound.begin() );
        std::fill( m_comvol_outbound.begin(), m_comvol_outbound.end(), emptyVol );
        m_put_queue.clear();
        m_hpput_queue.clear();
        m_get_queue.clear();
        m_hpget_queue.clear();
        m_send_queue.clear();
        m_hpsend_queue.clear();
        m_send.clear();
        m_send_tags.clear();
        m_hpput_queue_is_heap = false;
        m_hpget_queue_is_heap = false;
        m_hpsend_queue_is_heap = false;

        // 12. cycle the BSMP tag sizes
        m_prev_tag_size = m_cur_tag_size;
        m_cur_tag_size = m_next_tag_size;

        // 13. reset global error status
        m_error = false;
        return BSPLIB_SUCCESS;
    }


private:
    template <class T>
    lpf_memslot_t reg_glob_vec( std::vector< T > & array)
    {
        lpf_memslot_t slot = LPF_INVALID_MEMSLOT;
        lpf_err_t rc = lpf_register_global( m_ctx, array.data(),
                array.capacity() * sizeof(T), &slot );
        ASSERT( rc == LPF_SUCCESS );
        return slot;
    }

    template <class T>
    lpf_memslot_t reg_glob_pod( T & pod )
    {
        lpf_memslot_t slot = LPF_INVALID_MEMSLOT;
        lpf_err_t rc = lpf_register_global( m_ctx, &pod,
                sizeof(pod), &slot );
        ASSERT( rc == LPF_SUCCESS );
        return slot;
    }

    void broadcast_error()
    {
        m_error_mine = 1;
        for ( lpf_pid_t i = 0; i < m_nprocs; ++i) {
           lpf_err_t rc = lpf_put( m_ctx, m_error_mine_slot, 0,
                   i, m_error_slot, 0, sizeof(m_error),
                   LPF_MSG_DEFAULT );
           ASSERT( rc == LPF_SUCCESS );
        }
    }

    int has_anyone_else_exited()
    {
        int yes = 0;
        for (lpf_pid_t i = 0; i < m_nprocs; ++i)
            if (i != m_pid)
                yes = yes || m_exit[i];
        return yes;
    }

    int has_everyone_exited()
    {
        int yes = 1;
        for (lpf_pid_t i = 0; i < m_nprocs; ++i)
            if (i != m_pid)
                yes = yes && (m_exit[i]==1);
        return yes;
    }

    // Get a reference to smallest HP message
    template <typename HpQueue>
    static typename HpQueue::iterator get_smallest_hp_msg( HpQueue & hp_queue, bool & queue_is_heap ) 
    {
        ASSERT( !hp_queue.empty() );
        if (!queue_is_heap) 
        {
            std::make_heap( hp_queue.begin(), hp_queue.end(), Smallest()  );
            queue_is_heap = true;
        }

        typename HpQueue::iterator last = hp_queue.end() ;
        --last;

        typename HpQueue::iterator second_to_last = last;
        if ( last != hp_queue.begin() ) 
        {
            std::pop_heap( hp_queue.begin(), last, Smallest() );
            --second_to_last;
        }
        
        typename HpQueue::iterator smallest;
        if ( second_to_last->size < last->size)
        {
            smallest = second_to_last;
        }
        else  
        {
            smallest = last;
        }

        return smallest;
    }

    template <typename HpQueue, typename It>
    static void remove_hp_msg( HpQueue & hp_queue, bool & queue_is_heap, It item)
    {
        hp_queue.erase( item );
        if ( queue_is_heap && !hp_queue.empty() ) 
            std::push_heap( hp_queue.begin(), hp_queue.end(), Smallest() );
    }

    // minimum number of registers required for bsplib
    static const int EXTRA_REGS = 15; 

    struct Memslot { 
        lpf_memslot_t slot; 
        void * addr; 
        size_t size ;
        Memslot( lpf_memslot_t slot, size_t size )
            : slot( slot ), addr(0), size( size ) {}
        Memslot( void * addr, size_t size )
            : slot(LPF_INVALID_MEMSLOT),
              addr( addr ), size( size ) {}

        Memslot() : slot(LPF_INVALID_MEMSLOT)
                    , addr(0), size(0) {}
     }; 

    typedef std::list< std::vector< Memslot > >
        MemregQueue;
    
    struct MemregStack
    {
        typedef std::list< std::vector< Memslot > > 
            List;
        
        typedef std::list< std::vector< Memslot > > :: iterator 
            It;
 
        typedef std::list< std::vector< Memslot > > :: const_iterator 
            ConstIt;
       
        List list;
        It end;

        MemregStack()
            : list(), end(list.end()) 
        {}

        MemregStack( const MemregStack & other)
            : list( other.list ), end( list.begin() )
        {
            ptrdiff_t n = 
                std::distance<ConstIt>(other.list.begin(), other.end);

            std::advance( end, n );
        }

        void swap( MemregStack & other)
        {
            using std::swap;
            this->list.swap( other.list );
            swap( this->end, other.end) ;
        }

        MemregStack & operator=( const MemregStack & other)
        {
            if (this != &other) {
                MemregStack tmp(other);
                this->swap(tmp);
            }
            return *this;
        }

        std::vector< Memslot > & back() { 
            std::list< std::vector< Memslot > > :: iterator tmp(end);
            return * (--tmp); 
        }
        const std::vector< Memslot > & active_back() const { 
            return list.back(); 
        }
        void pop_back() { --end; }
        bool is_empty() { return list.begin() == end; }
        bool has_one_entry()
        { return !is_empty() && end == ++(list.begin()); }

        void push( MemregQueue::iterator entry, MemregQueue & from_list)
        {
            list.splice( end, from_list, entry );
        }

        void clear_tail() { 
            list.erase( end, list.end() );
            end = list.end();
        }

    } ;

#if __cplusplus >= 201103L    
    typedef std::unordered_map< void *, MemregStack >
        Memreg;
#else
    typedef std::tr1::unordered_map< void *, MemregStack > 
        Memreg;
#endif

    typedef std::vector< lpf_memslot_t >
        MempopQueue;


    typedef std::vector< char > 
        Payload;

    size_t access_put( size_t & src_offset, 
           lpf_pid_t & pid, lpf_memslot_t & dst_slot, size_t & dst_offset,
           size_t & size, 
           size_t pos, bool read )
    {
        return lpf::MicroMsg( m_put_queue.data(), pos )
            . access(src_offset, read)
            . access(pid, read)
            . access(dst_slot, read)
            . access(dst_offset, read)
            . access(size, read)
            . pos();
    }
    static const size_t MaxPutSize = 10 * 5;
    void queue_put( size_t src_offset, 
            lpf_pid_t pid, lpf_memslot_t dst_slot, size_t dst_offset,
            size_t size )
    {
        size_t n = m_put_queue.size();
        if ( m_put_queue.capacity() < n  + MaxPutSize ) {
            m_put_queue.reserve( std::max( 2*m_put_queue.capacity(), 
                        n + MaxPutSize) );
        }
        m_put_queue.resize( n + MaxPutSize) ;
        n = access_put( src_offset, pid, dst_slot, dst_offset,
                size, n, false );
        m_put_queue.resize( n );
    }



    struct Send {
        size_t size, tag_offset, payload_offset;
        lpf_pid_t pid;
    };

    struct HpSend {
        size_t size, tag_offset;
        void * payload;
        lpf_pid_t pid;
    };

    struct UnbufMsg { 
        size_t offset;
        size_t size;
        void * addr;
        lpf_pid_t pid;
        lpf_memslot_t slot;
    };

    // a comparison operator to pop and push the smallest element from a heap of
    // HP messages
    struct Smallest {
        template <class T>
        bool operator()( const T & a, const T & b) const
        { return a.size > b.size; }
    };

    typedef std::vector< char > PutQueue;
    typedef std::vector< UnbufMsg > UnbufMsgQueue;
    typedef std::vector< Send > SendQueue;
    typedef std::vector< HpSend > HpSendQueue;

    typedef std::vector< lpf_memslot_t > LocalSlots;
    typedef std::vector< size_t > PayloadOffsets;

    struct ComVol {
        size_t puts, hpputs, gets, hpgets, sends, hpsends;
        size_t get_payload;
        size_t send_payload;
        size_t newregs, delregs;
        size_t next_tag_size;
        unsigned char growth;

        char micro_msg[10 * 10];
        size_t micro_msg_size;

        ComVol() : puts(0), hpputs(0), gets(0), hpgets(0)
                   , sends(0), hpsends(0)
                   , get_payload(0)
                   , send_payload(0)
                   , newregs(0), delregs(0)
                   , next_tag_size(0)
                   , growth(0)
                   , micro_msg()
                   , micro_msg_size(0)
        {}

        ComVol & operator+=( const ComVol & other ) 
        {   puts += other.puts;
            hpputs += other.hpputs;
            gets += other.gets;
            hpgets += other.hpgets;
            sends += other.sends;
            hpsends += other.hpsends;
            send_payload += other.send_payload;
            get_payload += other.get_payload;
            growth = growth || other.growth;
            return *this;
        }

        bool operator<(const ComVol & other)  const
        {
            return puts < other.puts
                && hpputs < other.hpputs
                && gets < other.gets
                && hpgets < other.hpgets
                && sends < other.sends
                && hpsends < other.hpsends;
        }

        size_t nmsgs() const 
        { return puts + hpputs + gets + hpgets + 2*(sends + hpsends); }

        size_t tags() const { return sends + hpsends; }

        // compressed or uncompressed data structure
        void access(bool read) {
            micro_msg_size = lpf::MicroMsg( micro_msg, 0)
                .access( puts, read)
                .access( hpputs, read)
                .access( gets, read)
                .access( hpgets, read)
                .access( sends, read)
                .access( hpsends, read)
                .access( get_payload, read)
                .access( send_payload, read)
                .access( newregs, read)
                .access( delregs, read)
                .access( next_tag_size, read)
                .access( growth, read)
                .pos();
        }

        void compress()   { access(false); }
        void uncompress() { access(true); }
    };
    typedef std::vector< ComVol > ComVolBuf; 

    struct Resize {
        size_t regs, msgs;
        size_t send_offset, tag_offset;

        char micro_msg[4 * 10 + 1];
        size_t micro_msg_size;
    
        void access(bool read) {
            micro_msg_size = lpf::MicroMsg( micro_msg, 0)
                .access( regs, read)
                .access( msgs, read)
                .access( send_offset, read)
                .access( tag_offset, read)
                .pos();
        }

        void compress()   { access(false); }
        void uncompress() { access(true); }
    };

    typedef std::vector< Resize > ResizeBuf;

    typedef std::vector< size_t > SlotSizes;
    typedef std::vector< MemregQueue :: iterator > SlotAddrs;

#if __cplusplus >= 201103L    
    typedef std::unordered_set< void * > DirtyAddrs;
#else    
    typedef std::tr1::unordered_set< void * > DirtyAddrs;
#endif


    lpf_t     m_ctx;
    lpf_pid_t m_pid;
    lpf_pid_t m_nprocs;
    lpf::Time m_start;
    bool      m_safemode;

    // Memory registration 
    MemregQueue m_mrqueue;
    MempopQueue m_mpqueue;
    lpf_memslot_t m_mpqueue_slot;
    Memreg    m_memreg;
    DirtyAddrs m_popreg_dirty;
    size_t    m_curregs;
    size_t    m_maxregs;
    size_t    m_maxhpregs;
    size_t    m_maxmsgs;

    // Buffer to communicate sizes of memory registrations during lpf_sync
    SlotAddrs m_slot_addrs;
    SlotSizes m_slot_sizes;
    lpf_memslot_t m_slot_sizes_slot;

    // Buffers for message payloads and tags
    Payload   m_send;  // this buffer stores sends & puts
    Payload   m_send_tags;
    lpf_memslot_t m_send_slot;
    lpf_memslot_t m_send_tags_slot;
    Payload   m_recv;  // this buffer stores sends & gets
    Payload   m_recv_tags;
    lpf_memslot_t m_recv_slot;
    lpf_memslot_t m_recv_tags_slot;

    // Queues to hold communication requests
    PutQueue  m_put_queue;
    UnbufMsgQueue  m_get_queue;
    UnbufMsgQueue  m_hpget_queue;
    UnbufMsgQueue  m_hpput_queue;
    SendQueue m_send_queue;
    HpSendQueue m_hpsend_queue;
    bool      m_hpget_queue_is_heap;
    bool      m_hpput_queue_is_heap;
    bool      m_hpsend_queue_is_heap;

    // Buffer to hold temporarily stored local registrations
    LocalSlots m_local_slots;

    // Collection of buffers to communicate the communication volume
    // and compute destination buffer offsets during a lpf_sync
    PayloadOffsets m_outbound_send_payload_remote_offsets;
    lpf_memslot_t m_outbound_send_payload_remote_offsets_slot;
    PayloadOffsets m_inbound_send_payload_local_offsets;
    lpf_memslot_t m_inbound_send_payload_local_offsets_slot;
    ComVolBuf m_comvol_prev_outbound;
    ComVolBuf m_comvol_outbound;
    ComVolBuf m_comvol_inbound;
    ResizeBuf m_resize;
    lpf_memslot_t m_comvol_outbound_slot;
    lpf_memslot_t m_comvol_inbound_slot;
    lpf_memslot_t m_resize_slot;
    PayloadOffsets m_index;

    // collection of variables to trace whether we need reregistration
    size_t    m_send_prev_capacity, m_send_tags_prev_capacity;
    size_t    m_recv_prev_capacity, m_recv_tags_prev_capacity;
    size_t    m_outbound_send_payload_remote_offsets_prev_capacity;
    size_t    m_inbound_send_payload_local_offsets_prev_capacity;
    bool      m_reregister;
    bool      m_reregister_read;
    lpf_memslot_t m_reregister_slot;
    lpf_memslot_t m_reregister_read_slot;

    // Collection of variables to hold the BSMP tag size
    size_t    m_prev_tag_size;
    size_t    m_cur_tag_size;
    size_t    m_next_tag_size;

    // Buffer to communicate global error state during lpf_sync
    int       m_error_mine;
    lpf_memslot_t m_error_mine_slot;
    int       m_error;
    lpf_memslot_t m_error_slot;

    // Buffer to communicate end of life of this object
    std::vector<int> m_exit;
    lpf_memslot_t m_exit_slot;
};

bsplib_err_t bsplib_create( lpf_t ctx, lpf_pid_t pid, lpf_pid_t nprocs, 
        int safemode, size_t max_hp_regs, bsplib_t * bsplib )
{
    try {
        *bsplib = new BSPlib( ctx, pid, nprocs, safemode?true:false, max_hp_regs );
    }
    catch ( std::bad_alloc & ) {
        return BSPLIB_ERR_OUT_OF_MEMORY;
    } catch ( std::runtime_error & ) {
        return BSPLIB_ERR_FATAL;
    }
    return BSPLIB_SUCCESS;
}

bsplib_err_t bsplib_destroy( bsplib_t bsplib )
{
    bsplib_err_t rc = bsplib->end();
    delete bsplib;
    return rc;
}


double bsplib_time( bsplib_t bsplib )
{ return bsplib->time() ; }

lpf_pid_t bsplib_nprocs( bsplib_t bsplib )
{ return bsplib->nprocs(); }

lpf_pid_t bsplib_pid( bsplib_t bsplib )
{ return bsplib->pid(); }

bsplib_err_t bsplib_sync( bsplib_t bsplib )
{ return bsplib->sync( ); }

bsplib_err_t bsplib_push_reg( bsplib_t bsplib, const void * ident, size_t size )
{ return bsplib->pushreg( ident, size ); }

bsplib_err_t bsplib_pop_reg( bsplib_t bsplib, const void * ident )
{ return bsplib->popreg( ident ); }

bsplib_err_t bsplib_put( bsplib_t bsplib, lpf_pid_t dst_pid, const void * src,
        void * dst, size_t offset, size_t nbytes )
{ return bsplib->put( dst_pid, src, dst, offset, nbytes ); }

bsplib_err_t bsplib_hpput( bsplib_t bsplib, lpf_pid_t dst_pid, const void * src,
        void * dst, size_t offset, size_t nbytes )
{ return bsplib->hpput( dst_pid, src, dst, offset, nbytes ); }

bsplib_err_t bsplib_get( bsplib_t bsplib, lpf_pid_t src_pid, const void * src,
        size_t offset, void * dst, size_t nbytes )
{ return bsplib->get( src_pid, src, offset, dst, nbytes ); }

bsplib_err_t bsplib_hpget( bsplib_t bsplib, lpf_pid_t src_pid, const void * src,
        size_t offset, void * dst, size_t nbytes )
{ return bsplib->hpget( src_pid, src, offset, dst, nbytes ); }

size_t bsplib_set_tagsize( bsplib_t bsplib, size_t tagsize )
{ return bsplib->set_tagsize( tagsize ); }

bsplib_err_t bsplib_send( bsplib_t bsplib, lpf_pid_t dst_pid, const void * tag,
        const void * payload, size_t nbytes )
{ return bsplib->send( dst_pid, tag, payload, nbytes ); }

bsplib_err_t bsplib_hpsend( bsplib_t bsplib, lpf_pid_t dst_pid, const void * tag,
        const void * payload, size_t nbytes )
{ return bsplib->hpsend( dst_pid, tag, payload, nbytes ); }

bsplib_err_t bsplib_qsize( bsplib_t bsplib, size_t * nmessages, size_t * accum_bytes )
{ return bsplib->qsize( nmessages, accum_bytes ); }

bsplib_err_t bsplib_get_tag( bsplib_t bsplib, size_t * status, void * tag )
{ return bsplib->get_tag( status, tag ); }

bsplib_err_t bsplib_move( bsplib_t bsplib, void * payload, size_t reception_bytes )
{ return bsplib->move( payload, reception_bytes ); }

size_t bsplib_hpmove( bsplib_t bsplib, const void ** tag_ptr, const void ** payload_ptr )
{ return bsplib->hpmove( tag_ptr, payload_ptr ); }


