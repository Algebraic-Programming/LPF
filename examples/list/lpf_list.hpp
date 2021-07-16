
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

#ifndef LPF_LIST_HPP
#define LPF_LIST_HPP

#include "lpf.hpp"
#include "lpf_sort.hpp"

#include <cassert>
#include <iostream>
#include <cmath>

namespace lpf {

template <class T>
class list 
{
    template <class U> friend class list;
    typedef std::pair< lpf_pid_t , size_t > link_t;
    typedef std::pair< link_t, link_t > range_t;

public:

    typedef size_t size_type;
    typedef T      value_type;
    typedef T &    reference_type;
    typedef T *    pointer;

    class iterator {
        friend class list<T>;
    public:
        typedef size_t distance_type;
        typedef T      value_type;
        typedef T &    reference_type;
        typedef T *    pointer;

        iterator() 
            : m_list(0)
            , m_idx(INVALID_LINK)
        {}

        bool is_valid() const 
        { return m_list &&  m_list->m_ctx.pid() == m_idx.first ; }

        T & operator*() 
        {
            assert( is_valid() );
            return m_list->m_data[ m_idx.second ]; 
        }

        const T & operator*() const
        { 
            assert( is_valid() );
            return m_list->m_data[ m_idx.second ]; 
        }

        T * operator->() 
        {
            assert( is_valid() );
            return &m_list->m_data[ m_idx.second ]; 
        }

        const T * operator->() const
        { 
            assert( is_valid() );
            return &m_list->m_data[ m_idx.second ]; 
        }

                
        iterator & operator++()   // collective
        {
            increment();
            return *this;
        }

        iterator operator++(int)   // collective
        {
            iterator copy(*this);
            increment();
            return copy;
        }

        iterator & operator--()   // collective
        {
            decrement();
            return *this;
        }

        iterator operator--(int)   // collective
        {
            iterator copy(*this);
            decrement();
            return copy;
        }

        bool operator==( const iterator & other ) const 
        { return m_idx == other.m_idx; }

        bool operator!=( const iterator & other ) const 
        {   return !( *this == other ); }

    private:
        iterator( list & list, link_t idx )
           : m_list( & list)
           , m_idx( idx) 
        {}

        lpf_pid_t pid() const { return m_list->m_ctx.pid(); }

        void fence() { return m_list->m_ctx.fence(); }

        void increment() // collective
        {
            lpf::global< link_t, temporary > m_link(m_list->m_tmp_space);

            if ( pid() == m_idx.first ) {
                *m_link = m_list->m_next[ m_idx.second ];
            }
            fence();
            broadcast( m_link, m_idx.first ); 
            fence();
            m_idx = *m_link;
        }

        void decrement() // collective
        {
            lpf::global< link_t, temporary > m_link(m_list->m_tmp_space);

            if ( pid() == m_idx.first ) {
                *m_link= m_list->m_prev[ m_idx.second ];
            }
            fence();
            broadcast( m_link, m_idx.first ); 
            fence();
            m_idx = *m_link;
        }

        list * m_list;
        link_t m_idx; // globally replicated
    };


    list( lpf::machine & lpf )
        : m_ctx( lpf )
        , m_data(lpf, 2, T(), machine::n_rel)
        , m_next(lpf, 2, INVALID_LINK, machine::n_rel)
        , m_prev(lpf, 2, INVALID_LINK, machine::n_rel)
        , m_tmp_space( lpf )
    {
        clear();
    }

    void clear()
    {
        m_data.resize(2);
        m_next.resize(2);
        m_prev.resize(2);
        m_next[0] = REAR_SENTINEL;
        m_next[1] = INVALID_LINK;
        m_prev[0] = INVALID_LINK;
        m_prev[1] = HEAD_SENTINEL;
    }

    /** Get a sequential iterator from the start of the list
     *
     * \note This function is collective
     * \returns Iterator to the start of the list
     */
    iterator begin()
    { iterator it( *this, HEAD_SENTINEL ); return ++it; }
    
    /** Get a sequential iterator from the start of the list
     *
     * \note This function is collective
     * \returns Iterator to the end of the list
     */
    iterator end()
    { return iterator( *this, REAR_SENTINEL ); }
   

    /** Insert an element on one of the processes
     *
     * \note This function is collective
     *
     * \param new_pid The ID of the process to store the element on
     * \param pos     The position where to insert the element
     * \param val     The value to insert. This is only required on the
     *                process \a new_pid
     * \returns       An iterator the new element in the list
     */
    iterator insert( lpf_pid_t new_pid, const iterator & pos, const T * val = 0) 
    {
        assert( pos.m_list == this );

        global_reserve( m_data.size() + 1);

        global< link_t, temporary > prev( m_tmp_space );
        global< link_t, temporary > newpos( m_tmp_space );

        lpf_pid_t it_pid = pos.m_idx.first;

        if ( i_am( it_pid ) ) 
            *prev = m_prev[ pos.m_idx.second ];
        
        if ( i_am( new_pid ) ) {
            *newpos = link_t( new_pid, m_data.size() );
            m_data.push_back( *val );
            m_next.push_back( pos.m_idx );
            m_prev.push_back( INVALID_LINK );
        }
        m_ctx.fence();
        broadcast( prev, it_pid ); 
        broadcast( newpos, new_pid );
        m_ctx.fence();

        lpf_pid_t prev_pid = prev->first;
        if ( HEAD_SENTINEL == *prev || i_am( prev_pid ) ) 
            m_next[ prev->second ] = *newpos;

        if ( i_am( new_pid ) ) 
            m_prev.back() = *prev ;

        if ( REAR_SENTINEL == pos.m_idx || i_am( it_pid ) ) 
            m_prev[ pos.m_idx.second ] = *newpos;

        return iterator( *this, *newpos );
    }

    /** Insert a range of elements on all processes.
     *
     * \param pos The position to insert the new range
     * \param new_begin The start of the range to be inserted
     * \param new_end The end of the range to be inserted
     * \returns An iterator the start of the new range in the list
     */
    template <class It>
    iterator insert( const iterator & pos, It new_begin, It new_end )
    {
        assert( pos.m_list == this );

        const lpf_pid_t my_pid = m_ctx.pid();
        const lpf_pid_t it_pid = pos.m_idx.first;

        global_reserve( m_data.size() + std::distance( new_begin, new_end ) );

        // insert the elements
        link_t n( my_pid, m_data.size() );
        link_t p = INVALID_LINK ;
        const link_t start_range = n ; 
        while (new_begin != new_end ) {
            m_data.push_back( *new_begin );
            m_prev.push_back( p );
            p = n;
            n.second += 1;
            m_next.push_back( n );
            ++new_begin ;
        }
        const link_t end_range = n;
        // and store the range for communication
        global< range_t , temporary > range( m_tmp_space );
        global< std::vector< range_t > , temporary > ranges( m_tmp_space, m_ctx.nprocs(), range_t(HEAD_SENTINEL, HEAD_SENTINEL), m_ctx.p_rel );
        *range = range_t( start_range, end_range );
        
        // lookup the node previous to pos
        global< link_t , temporary > prev( m_tmp_space );
        if ( i_am( it_pid ) ) 
            *prev = m_prev[ pos.m_idx.second ];

        // broadcast all data
        m_ctx.fence();
        broadcast( prev, it_pid ); 
        allgather( range, ranges );
        m_ctx.fence();

        // search the lowest pid with a non-empty insertion range
        lpf_pid_t lowest_non_empty = 0;
        while ( lowest_non_empty < m_ctx.nprocs() &&
                ranges[ lowest_non_empty ].first.second
                == ranges[ lowest_non_empty ].second.second )
            lowest_non_empty++;

        // search the highest pid with a non-empty insertion range
        lpf_pid_t highest_non_empty = m_ctx.nprocs();
        while ( highest_non_empty > 0 &&
                ranges[ highest_non_empty - 1 ].first.second
                == ranges[ highest_non_empty - 1].second.second )
            highest_non_empty-- ;
            
        // do nothing when all pids had empty ranges
        if ( lowest_non_empty == m_ctx.nprocs() )
            return pos;

        assert( highest_non_empty > 0 );
        highest_non_empty -= 1;

        const link_t start = ranges[ lowest_non_empty ].first;
        const link_t end = ranges[ highest_non_empty ].second;

        // update the link previous to the global range
        lpf_pid_t prev_pid = prev->first;
        if ( HEAD_SENTINEL == *prev || i_am( prev_pid ) ) {
            m_next[ prev->second ] = start;
        }

        // update the 'prev' link after the global range
        if ( REAR_SENTINEL == pos.m_idx || i_am( it_pid ) ) 
           m_prev[ pos.m_idx.second ] = end;
 
        // set the last 'next' pointer in the global range
        if (my_pid == highest_non_empty)
            m_next[ end_range.second - 1] = pos.m_idx;

        // set the first 'prev' pointer in the global range
        if ( my_pid == lowest_non_empty )
            m_prev[ start_range.second ] = *prev;

        // link all the blocks from lowest_non_empty to highest_non_empty
        if ( ranges[my_pid].first != ranges[my_pid].second ) {
            lpf_pid_t s;
            // search a pid with a non-empty range to the right of me
            for ( s = my_pid + 1; s <= highest_non_empty; ++s ) {
                if ( ranges[s].first != ranges[s].second ) {
                    m_next[ end_range.second - 1] = ranges[s].first;
                    break;
                }
            }

            // search a pid with a non-empty range to the left of me
            for ( s = my_pid; s > lowest_non_empty; --s ) {
                if ( ranges[s-1].first != ranges[s-1].second ) {
                    m_prev[ start_range.second ] = ranges[s-1].second;
                    m_prev[ start_range.second ].second -= 1;
                    break;
                }
            }
        }

        return iterator( *this, start );
    }

    /** The number of elements in this list that are local to this process 
     *
     * \returns The number of elements in this list that are local to this
     *          process 
     **/
    size_t local_count() const
    { return m_data.size() - 2; }

    /** The total number of elements in this list
     *
     * \note This function is collective
     *
     * \returns The total number of elements in this list
     * */
    size_t count() const 
    {
        global< size_t, temporary > count( m_tmp_space );
        *count = local_count();
        m_ctx.fence();
        allreduce( count, std::plus<size_t>() );
        m_ctx.fence();
        return *count;
    }

    /** The difference between the largest and the smallest number of
     * elements on any process.
     *
     * \note This function is collective
     *
     * \returns The difference between the largest and the smallest
     *          number of elements on any process.
     * */
    size_t imbalance() const 
    {
        const size_t loc = local_count();
        global< size_t, temporary > min( m_tmp_space );
        global< size_t, temporary > max( m_tmp_space );
        * min = loc;
        * max = loc;
        m_ctx.fence();
        allreduce( min, min_op<size_t>() );
        allreduce( max, max_op<size_t>() );
        m_ctx.fence();
        return *max - *min;
    }

    /** Redistributes the elements in the linked list so that each
     * process gets exactly \f$ (N + P - s - 1)/P \f$ elements,
     * where \f$N\f$ is the total number of elements, \f$P\f$ the
     * number of processes, and \f$s\f$ the process ID of the local
     * process.
     *
     * \note This function is collective
     *
     */
    void rebalance() 
    {
        const size_t N = count();
        const lpf_pid_t nprocs = m_ctx.nprocs();
        const lpf_pid_t pid = m_ctx.pid();

        global< size_t, temporary > local_use( m_tmp_space );
        global< std::vector< size_t >, temporary > glob_use( m_tmp_space, m_ctx.nprocs(), 0ul, m_ctx.p_rel );
        size_t local_count = this->local_count();

        size_t nloc = (N+nprocs-pid-1)/nprocs;
        assert( m_data.capacity() >= nloc );
        m_data.resize( std::max( m_data.size(), 2+nloc ) );
        m_next.resize( std::max( m_next.size(), 2+nloc ) );
        m_prev.resize( std::max( m_prev.size(), 2+nloc ) );

        * local_use = local_count; 
        m_ctx.fence();
        allgather( local_use, glob_use );
        m_ctx.fence();

        // compute a redistribution
        std::vector< std::pair< block, block > > redistr;
        compute_redistr( N, glob_use, redistr ) ;

        // this data structure is quite small, so we don't need
        // to worry that we'll have to search through many
        // records when rewriting links
        assert( redistr.size() <= nprocs ); 

        // do the redistribution
        for ( size_t b = 0; b < redistr.size(); ++b ) {

            block from = redistr[b].first;
            block to = redistr[b].second;

            if ( from.pid == pid ) {
                copy( m_ctx.one_rel, m_data, from.begin, from.end, 
                        to.pid, m_data, to.begin, to.end );
                copy( m_ctx.one_rel, m_next, from.begin, from.end, 
                        to.pid, m_next, to.begin, to.end );
                copy( m_ctx.one_rel, m_prev, from.begin, from.end, 
                        to.pid, m_prev, to.begin, to.end );
            }
        }

        m_ctx.fence();

        m_data.resize( 2 + nloc );
        m_next.resize( 2 + nloc );
        m_prev.resize( 2 + nloc );

        update_links( redistr, m_next );
        update_links( redistr, m_prev );
    }


    void rank( list<size_t> & ranking ) const 
    {
        const size_t N = count();
        const lpf_pid_t P = m_ctx.nprocs();
        const lpf_pid_t s = m_ctx.pid();
        const lpf_pid_t log_P = lpf_pid_t( ceil( log2(P) ) ); // log p rounded upwards

        const lpf_pid_t head_pid = HEAD_SENTINEL.first;
        const lpf_pid_t rear_pid = REAR_SENTINEL.first; (void) rear_pid;
        const size_t head_pos = HEAD_SENTINEL.second;
        const size_t rear_pos = REAR_SENTINEL.second;
        const machine::comm_pattern one_rel = machine::one_rel;
        const machine::comm_pattern n_rel = machine::n_rel;
        const machine::comm_pattern p_rel = machine::p_rel;

        if ( N == 0 ) {
            ranking.clear();
            return;
        }

        if ( N <= P * P * log_P ) {
            // Then there is not enough data. Just do everything on all processes
            // Do an allgather with variable length arrays
            global< size_t, temporary > nloc( m_tmp_space );
            global< std::vector< size_t >, temporary > offsets( m_tmp_space, P, 0ul, p_rel);
            ranking.m_data.reserve( N + 2*P); // don't forget space for sentinels
            ranking.m_prev.reserve( N + 2*P);
            ranking.m_next.reserve( N + 2*P);

            const size_t alien = -1;
            ranking.m_data.clear(); ranking.m_data.resize( N + 2*P, alien );
            ranking.m_prev.clear(); ranking.m_prev.resize( N + 2*P );
            ranking.m_next.clear(); ranking.m_next.resize( N + 2*P );

            *nloc = m_data.size();
            m_ctx.fence();
            scan( nloc, offsets, std::plus<size_t>(), 0ul );
            allgather( m_prev, ranking.m_prev );
            allgather( m_next, ranking.m_next );
            m_ctx.fence();

            link_t ptr = ranking.m_next[ HEAD_SENTINEL.second ];
            size_t index = 0;
            while ( ptr != REAR_SENTINEL) {
                size_t o = ptr.first == 0 ? 0ul : offsets[ ptr.first-1 ];
                ranking.m_data[ ptr.second + o ] = (i_am(ptr.first)? index : alien);
                index += 1;
                ptr = ranking.m_next[ ptr.second + o ];
            }

            // now, throw away all aliens
            ranking.m_next = m_next;
            ranking.m_prev = m_prev;
            size_t i = s == 0 ? 0ul : offsets[ s - 1];
            size_t j = offsets[s];
            std::copy( ranking.m_data.begin() + i, ranking.m_data.begin() + j,
                    ranking.m_data.begin() );

            ranking.m_data.resize( *nloc );
            return;
        }
        
        // Phase 0: Initialize
        const size_t local_nodes = m_data.size();
        global< size_t, temporary > max_local_nodes( m_tmp_space, local_nodes);
        m_ctx.fence();
        allreduce( max_local_nodes, max_op<size_t>() );
        m_ctx.fence();

        const size_t inactive = size_t(-1);
        const size_t invalid = size_t(-1);
        std::vector< size_t > active( local_nodes-2 );
        for (size_t i = 2; i < local_nodes; ++i) active[i-2] = i;

        global< std::vector< int >, temporary > my_choice( m_tmp_space, *max_local_nodes, 0, n_rel );
        global< std::vector< int >, temporary > prev_choice( m_tmp_space, *max_local_nodes, 0, n_rel );
        global< std::vector< int >, temporary > next_choice( m_tmp_space, *max_local_nodes, 0, n_rel );
           // it stores whether we have picked 'heads' or 'tails' for
           // a node with values '1' and '0'
        my_choice[head_pos] = 1; my_choice[rear_pos] = 0;
           // The head sentinel always chooses '1' so that it can't be merged
           // The rear sentinel always chooses '0'
        
        global< std::vector< size_t >, temporary > prev_index( m_tmp_space, *max_local_nodes, 0, n_rel );
        global< std::vector< link_t >, temporary > prev_prev( m_tmp_space, *max_local_nodes, HEAD_SENTINEL, n_rel );

        // index of each node into the local active list.
        global< std::vector< size_t >, temporary > my_active( m_tmp_space, *max_local_nodes, 0ul, n_rel );
        global< std::vector< size_t >, temporary > prev_active( m_tmp_space, *max_local_nodes, 0ul, n_rel );
        my_active[rear_pos] = invalid;
        prev_active[head_pos] = invalid;
           

        ranking.m_prev.reserve( local_nodes ); ranking.m_prev = m_prev;
        ranking.m_next.reserve( local_nodes ); ranking.m_next = m_next;
        ranking.m_data.reserve( local_nodes );
        ranking.m_data.resize( local_nodes );

        ranking.m_data[head_pos] = 0; 
        ranking.m_data[rear_pos] = 0;
        for (size_t i = 2; i < local_nodes; ++i)
            ranking.m_data[i] = 1;
        // Phase 1: Reduce from N elements to N/P elements by
        //          repetitive pairing


        global< size_t, temporary > total_remaining( m_tmp_space );
        while(true)
        {
            // toss a coin for each node in the active list
            for (size_t i  = 0 ; i < active.size(); ++i) {
                size_t j = active[i];
                if (j == inactive) 
                {
                    using std::swap;
                    swap( active[i], active.back() );
                    active.pop_back();
                    i -= 1;
                }
                else
                {
                    my_choice[j] = (m_ctx.random() <= 0.5? 0 : 1) ;
                    my_active[j] = i;
                }
            }
            *total_remaining = active.size();

            m_ctx.fence();
            allreduce( total_remaining, std::plus<size_t>() );
            m_ctx.fence();

            if ( *total_remaining <= N/P ) break; 

            // get the coin toss results of the 'prev' nodes
            for (size_t i = 0 ; i < active.size(); ++i) {
                size_t j = active[i];

                lpf_pid_t prev_pid = ranking.m_prev[j].first;
                size_t prev_pos = ranking.m_prev[j].second;

                lpf_pid_t next_pid = ranking.m_next[j].first;
                size_t next_pos = ranking.m_next[j].second;

                get( one_rel, prev_pid, my_choice, prev_pos, prev_choice, j );
                get( one_rel, prev_pid, ranking.m_data, prev_pos, prev_index, j);
                get( one_rel, prev_pid, ranking.m_prev, prev_pos, prev_prev, j );
                get( one_rel, next_pid, my_choice, next_pos, next_choice, j );
            }
            m_ctx.fence();

            for (size_t i = 0 ; i < active.size(); ++i) {
                size_t j = active[i];
                if (my_choice[j]==1 && prev_choice[j]==0 ) {
                    assert( ranking.m_prev[j] != HEAD_SENTINEL );

                    // by-pass 'prev' link
                    ranking.m_prev[j] = prev_prev[j];
                    
                    // add the index
                    ranking.m_data[j] += prev_index[j];

                }
                // update the next-link of the prev prev
                if (my_choice[j] == 0 && next_choice[j] == 1 ) {
                    assert( ranking.m_next[j] != REAR_SENTINEL );

                    lpf_pid_t prev_pid = ranking.m_prev[j].first;
                    size_t prev_pos = ranking.m_prev[j].second;

                    put( one_rel, ranking.m_next, j, prev_pid, ranking.m_next, prev_pos );

                    // mark this node now as inactive, because it has been
                    // merged with the next node
                    active[i] = inactive;
                }
            }
            m_ctx.fence();
        } 
        
        // Phase 2. Now there are only about N/P active nodes left
        // all-gather all active nodes, and do a local prefix on
        // each process
        const size_t n_remaining = active.size();

        global< std::vector< size_t >, temporary> active_offsets(m_tmp_space, P, 0ul, p_rel);
        { global< size_t, temporary > count( m_tmp_space, n_remaining );
          m_ctx.fence();
          scan( count, active_offsets, std::plus<size_t>(), 0ul );
          m_ctx.fence();
        }

        const size_t part_two = N/P;

        assert( *total_remaining == active_offsets.back() );
        assert( *total_remaining <= N/P );
        // so also, max remaining active nodes per process <= N/P 
        assert( n_remaining <= N/P );
        
        // compress the remaining active nodes in a smaller range
        // which can be easier allgathered. We also need to translate the
        // links, which is what we start with here 
        global< std::vector< size_t >, temporary > root_ranks( m_tmp_space, N/P, 0ul, n_rel );
        global< std::vector< size_t >, temporary > active_next( m_tmp_space, 2*N/P, 0ul, n_rel );
        global< std::vector< size_t >, temporary > active_prev( m_tmp_space, 2*N/P, 0ul, n_rel );
        global< size_t, temporary > active_head(m_tmp_space);
 
        // transform my_active from local active indices to global active
        // indices
        for ( size_t i = 0; i < active.size(); ++i) {
            size_t o = s==0 ? 0ul : active_offsets[s-1];
            my_active[active[i]] += o;
        }

        // get the first active node
        if ( !i_am(head_pid) )
            get( p_rel, head_pid, ranking.m_next, head_pos, ranking.m_next, head_pos );

        m_ctx.fence();

        get( p_rel, ranking.m_next[head_pos].first, my_active, ranking.m_next[head_pos].second,
                active_head );

        // transform the linked list from global indices to global active
        // indices
        for (size_t i = 0 ; i < active.size(); ++i ) {
            size_t j = active[ i ];

            lpf_pid_t next_pid = ranking.m_next[ j ].first;
            size_t next_pos    = ranking.m_next[ j ].second;

            get( one_rel, next_pid, my_active, next_pos, active_next, i );

            lpf_pid_t prev_pid = ranking.m_prev[ j ].first;
            size_t prev_pos    = ranking.m_prev[ j ].second;
            get( one_rel, prev_pid, my_active, prev_pos, active_prev, i );
        }
        m_ctx.fence();
        // now: active_next = next pointers with global active indices
        // now: active_prev = prev pointers with global active indices
        // now: active_head = the head of the list 

        // allgather all the active links and rankings so far into the scratch space
        for ( lpf_pid_t p = 0; p < P; ++p ) {
            size_t o = s==0 ? 0ul : active_offsets[s-1];

            copy( one_rel, active_next, 0ul, n_remaining, 
                    p, active_next, part_two + o, part_two + o + n_remaining );
            copy( one_rel, active_prev, 0ul, n_remaining, 
                    p, active_prev, part_two + o, part_two + o + n_remaining );

            for (size_t i = 0; i < n_remaining; ++i) {
                size_t j = active[i];
                put( one_rel, ranking.m_data, j, p, root_ranks, o + i );
            }
        }
        
        m_ctx.fence();
        // now the m_next and m_prev links in the scratch space form a doubly
        // linked list, which we can now easily compute the list ranking of
        for ( size_t i = *active_head, index=0; i != invalid; 
                i = active_next[ part_two + i ]) {
            index += root_ranks[ i ] ;
            root_ranks[ i ] = index;
        }

        // now, we can move the computed prefix sum back to original positions
        // and set prev links to sentinel, because they are all contracted
        for (size_t i = 0; i < n_remaining; ++i) {
            size_t j = active[i];
            size_t o = s==0 ? 0ul : active_offsets[s-1];
            ranking.m_data[ j ] = root_ranks[ o + i ];
            ranking.m_prev[ j ] = HEAD_SENTINEL;
        }

        // Phase 3. Repetitively (O( log(P) ) times) lookup indices
        //
        // we need some lookup tables first
        std::vector< size_t > todo( local_nodes - 2); todo.clear();
        for ( size_t j = 2; j < local_nodes; ++j ) {
            bool active = ( ranking.m_prev[j] == HEAD_SENTINEL);
            my_active[ j ] = active ;
            if (!active) todo.push_back( j );
        }

        // now we can do the O(log P) iterations
        while(true) {
            global< size_t, temporary > todo_size( m_tmp_space, todo.size() );
            m_ctx.fence();
            allreduce( todo_size, std::plus<size_t>() );
            m_ctx.fence();
            
            if (*todo_size == 0 ) break;

            m_ctx.fence();

            for (size_t i = 0; i < todo.size(); ++i) {
                size_t j = todo[i];
                assert( j < ranking.m_prev.size() );

                lpf_pid_t prev_pid = ranking.m_prev[j].first;
                size_t    prev_pos = ranking.m_prev[j].second;

                get( one_rel, prev_pid, my_active, prev_pos, prev_active, j );
                get( one_rel, prev_pid, ranking.m_data, prev_pos, prev_index, j );
            } 

            m_ctx.fence();

            for (size_t i = 0; i < todo.size(); ++i) {
                size_t j = todo[i];

                if ( prev_active[j] ) {
                    ranking.m_data[j] += prev_index[j];
                    my_active[j] = 1;

                    using std::swap;
                    swap( todo[i], todo.back() );
                    todo.pop_back();
                    i -= 1;
                }
            }
        }
        m_ctx.fence();

        // now recover the original prev and next links
        ranking.m_prev = m_prev;
        ranking.m_next = m_next;
        ranking.m_data.resize( local_nodes );
    }


    void generate_random( size_t total_number, T start, T end )
    {
        clear();

        const lpf_pid_t P = m_ctx.nprocs();
        const lpf_pid_t s = m_ctx.pid();

        const size_t nloc = (total_number + P - s -1 ) / P;
        const size_t max_nloc = (total_number + P - 1) / P;

        const std::pair< double, link_t > invalid_location
            = std::make_pair(0.0, INVALID_LINK );
        global< std::vector< std::pair< double, link_t  > > , temporary > 
            rnd_node_locations( m_tmp_space, 2*max_nloc, invalid_location, m_ctx.zero_rel);

        m_data.reserve( nloc + 2); m_data.resize( nloc + 2 );

        rnd_node_locations.resize( nloc );
        for (size_t i = 0; i < nloc; ++i) {
            link_t location = link_t(s, 2+i) ;
            double rnd = m_ctx.random();
            rnd_node_locations[i] = std::make_pair( rnd , location);
            m_data[2+i] = start + T( (end - start ) * rnd ) ;
        }

        sort( m_ctx, m_tmp_space, rnd_node_locations );

        // last node of local list
        global< link_t, temporary > last( m_tmp_space, HEAD_SENTINEL, m_ctx.p_rel );

        // last node of all local lists
        global< std::vector< link_t > , temporary > prev( m_tmp_space, P+1, HEAD_SENTINEL, m_ctx.p_rel );

        // first node of local list
        global< link_t, temporary > first( m_tmp_space, REAR_SENTINEL, m_ctx.p_rel );

        // first node of all local lists
        global< std::vector< link_t > , temporary > next( m_tmp_space, P+1, REAR_SENTINEL, m_ctx.p_rel );

        // just the node locations of the rnd_node_locations, because
        // only these should be communicated
        global< std::vector< link_t > , temporary > nodes( m_tmp_space, 2*max_nloc, INVALID_LINK, m_ctx.n_rel );
        nodes.resize( rnd_node_locations.size() );

        for ( size_t i = 0; i < rnd_node_locations.size(); ++i)
            nodes[i] = rnd_node_locations[i].second;

        if (!nodes.empty()) {
            *first = nodes.front();
            *last = nodes.back();
        }
        else {
            *first = INVALID_LINK;
            *last = INVALID_LINK;
        }

        m_ctx.fence();
        allgather( first, next );
        allgather( last, prev );
        m_ctx.fence();

        // lookup the connection between the last of this block
        // and first of the next local block
        lpf_pid_t i_next = s+1;
        while (i_next < P && next[i_next] == INVALID_LINK )
            i_next += 1;

        // lookup the connection between last of the previous block
        // and the first of this block
        lpf_pid_t i_prev = s;
        while (i_prev > 0 && prev[i_prev-1] == INVALID_LINK )
            i_prev -= 1;

        if ( i_prev == 0 ) i_prev = P ; else i_prev -= 1;

        m_next.reserve( nloc + 2); m_next.resize( nloc + 2 );
        m_prev.reserve( nloc + 2); m_prev.resize( nloc + 2 );

        for (size_t i = 0; i < nodes.size(); ++i)
        {
            if (i == 0)
                put( m_ctx.one_rel, prev, i_prev, 
                        nodes[i].first, m_prev, nodes[i].second );
            else
                put( m_ctx.one_rel, nodes, i-1, 
                        nodes[i].first, m_prev, nodes[i].second );

            if (i == nodes.size() - 1 ) 
                put( m_ctx.one_rel, next, i_next, 
                        nodes[i].first, m_next, nodes[i].second );
            else
                put( m_ctx.one_rel, nodes, i+1, 
                        nodes[i].first, m_next, nodes[i].second );
        }

        // lookup the first node of the whole linked list
        i_next = 0;
        while (i_next < P && next[i_next] == INVALID_LINK )
            i_next += 1;

        // and set the start of the list
        if ( i_next < P ) 
            m_next[ HEAD_SENTINEL.second ] = next[i_next];

        // lookup the last node of the whole linked list
        i_prev = P;
        while (i_prev > 0 && prev[i_prev-1] == INVALID_LINK )
            i_prev -= 1;

        // and set the start of the list
        if ( i_prev > 0 )
            m_prev[ REAR_SENTINEL.second ] = prev[i_prev-1];

        m_ctx.fence();
    }



#ifdef LPF_LIST_DEBUG
    void dump( std::ostream & out ) const
    {
        for (lpf_pid_t p = 0; p < m_ctx.nprocs(); ++p) {
            if (i_am(p)) {

                out << "PID " << p << ": "
                    << m_data.size() << " elements\n  Next: ";

                for (size_t i = 0; i < m_next.size(); ++i) {
                    out << "< " << m_next[i].first 
                        << ", " << m_next[i].second << " > ";
                }
                out << "\n  Prev: ";
                for (size_t i = 0; i < m_prev.size(); ++i) {
                    out << "< " << m_prev[i].first 
                        << ", " << m_prev[i].second << " > ";
                }
                out << "\n";
            }
            m_ctx.fence();
        }
    }
#endif

private:
    struct block { lpf_pid_t pid; size_t begin, end; } ;

    template <class Container>
    void compute_redistr( const size_t N, const Container & glob_use, 
            std::vector< std::pair< block, block > > & redistr )
    {
        lpf_pid_t nprocs = m_ctx.nprocs();

        std::vector< block > over_use;
        std::vector< block > space;
        for (lpf_pid_t s = 0; s < nprocs; ++s) {
            const size_t nloc_s = (N + nprocs - s - 1 ) / nprocs;
            
            if (glob_use[s] > nloc_s){
                block b = { s, nloc_s, glob_use[s] };
                over_use.push_back( b );
            }
            if (glob_use[s] < nloc_s){
                block b = { s, glob_use[s], nloc_s };
                space.push_back( b );
            }
        }

        redistr.clear();
        for (size_t b = 0; b < over_use.size(); ++b) {
            size_t use = over_use[b].end-over_use[b].begin;
            size_t src_offset = over_use[b].begin;
            size_t dst_offset = space.back().begin;
            size_t copied = 0;

            while (copied < use ) {
                size_t c = std::min(space.back().end-dst_offset, use-copied);
                copied += c; 

                lpf_pid_t p = over_use[b].pid;
                lpf_pid_t q = space.back().pid;
                block new_from = { p, 2 + src_offset, 2 + src_offset + c };
                block new_to = { q, 2 + dst_offset, 2 + dst_offset + c };
                assert( p != q );
                redistr.push_back( std::make_pair( new_from, new_to ) );
                dst_offset += c; 
                src_offset += c;

                if (dst_offset >= space.back().end) {
                    assert( dst_offset == space.back().end );
                    space.pop_back();
                    dst_offset = space.empty()? size_t(-1):space.back().begin;
                }
                else {
                    assert( copied == use );
                    space.back().begin = dst_offset;
                }
            }
        }
        assert( redistr.size() <= nprocs ); // TODO: prove this
    }

    void update_links( 
            const std::vector< std::pair < block, block > > & redistr,
            global< std::vector< link_t >, persistent > & links ) 
    {
        lpf_pid_t nprocs = m_ctx.nprocs();

        const size_t invalid = -1;
        std::vector< size_t > from_idx(nprocs, invalid);

        // build an index into redistr
        for ( size_t b = 0; b < redistr.size(); ++b ) { 
            block from = redistr[b].first;
            if ( from_idx[from.pid] == invalid )
                from_idx[ from.pid ] = b;
        }

        for (size_t i = 0; i < links.size(); ++i ) 
        {
            lpf_pid_t from_pid = links[i].first; 
            if ( INVALID_LINK != links[i] && from_idx[ from_pid ] != invalid ){
                for (size_t b = from_idx[ from_pid ]; b < redistr.size(); ++b)
                {
                    block from = redistr[b].first;
                    block to = redistr[b].second;
                    if (from.pid != from_pid) break;

                    size_t j = links[i].second;
                    if ( j >= from.begin && j < from.end ) {
                        links[i].first = to.pid;
                        links[i].second = to.begin + (j - from.begin);
                    }
                }
            }
        }
    }

    void local_reserve( size_t size )
    {
        if (size > m_data.capacity() )
        {
            size_t new_size = std::max( 2*m_data.capacity(), size );
            m_data.reserve( new_size );
            m_next.reserve( new_size );
            m_prev.reserve( new_size );
        }
    }

    void global_reserve( size_t size )
    {
        global< size_t, temporary > count( m_tmp_space, size );

        m_ctx.fence();
        allreduce( count, max_op<size_t>() );
        m_ctx.fence();
        
        local_reserve( *count );
    }


    static const link_t INVALID_LINK;
    static const link_t HEAD_SENTINEL;
    static const link_t REAR_SENTINEL;

    bool i_am( lpf_pid_t p ) const { return m_ctx.pid() == p; }

    lpf::machine &           m_ctx;
    global< std::vector<T>, persistent >        m_data;
    global< std::vector< link_t >, persistent > m_next;
    global< std::vector< link_t >, persistent > m_prev;

    mutable temporary_space  m_tmp_space;
};

template <class T>
const typename list<T>::link_t list<T> :: INVALID_LINK(-1, -1);

template <class T>
const typename list<T>::link_t list<T> :: HEAD_SENTINEL(0, 0);

template <class T>
const typename list<T>::link_t list<T> :: REAR_SENTINEL(0, 1);

}

#endif
