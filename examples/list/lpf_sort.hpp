
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

#ifndef LPF_SORT_HPP
#define LPF_SORT_HPP

#include "lpf.hpp"

#include <algorithm>
#include <numeric>

namespace lpf {


namespace detail {

    template <class T, class C > 
    struct nested_pair_compare
        : std::binary_function< std::pair< T, lpf_pid_t >,
             std::pair< T, lpf_pid_t >, bool > {

          nested_pair_compare( const C & compare )
              : m_compare( compare )
          {}

          bool operator()( const std::pair< T, lpf_pid_t > & a,
                  const std::pair< T, lpf_pid_t > & b ) const
          {
              return m_compare( a.first, b.first );
          }

          C m_compare;
    };

    template <class MultiQueueIdx, class V1, class V2, class C>
    void multi_merge( temporary_space & tmp,
            const MultiQueueIdx & multi_queue_begin,
            MultiQueueIdx & multi_queue_idx, 
            const V1 & values, V2 & sorted, const C & compare )
    {
        typedef typename V1::value_type T;

        const size_t N = values.size();
        const size_t M = multi_queue_idx.size();
        global< std::vector< std::pair< T, lpf_pid_t > > , temporary > 
            heap( tmp, M, std::make_pair( T(), 0), machine::zero_rel );

        lpf_pid_t k = 0;
        for (lpf_pid_t j = 0; j < M; ++j) {
            if (multi_queue_idx[j] > 0 ) {
                multi_queue_idx[j] -= 1;
                size_t i = multi_queue_begin[j] + multi_queue_idx[j];
                heap[k] = std::make_pair( values[i], j );
                k+=1;
            }
        }
        heap.resize( k );
        detail::nested_pair_compare< T, C > pair_compare( compare );
        std::make_heap( heap.begin(), heap.end(), pair_compare );

        for (lpf_pid_t j = N ; j > 0; --j) {
            // pop highest value from heap
            std::pop_heap( heap.begin(), heap.end(), pair_compare);
            // write it in the resulting array
            sorted[j-1] = heap.back().first;

            // push new value to the queue if there any left
            size_t s = heap.back().second;
            if ( multi_queue_idx[ s ] > 0 ) {
                multi_queue_idx[ s ] -= 1;
                size_t i = multi_queue_begin[s] + multi_queue_idx[s];
                heap.back().first = values[i];
                std::push_heap( heap.begin(), heap.end(), pair_compare);
            }
            else {
                heap.pop_back();
            }
        }
    }

}

template < class T, template <class X> class S, class Compare >
void sort( machine & ctx, temporary_space & tmp,
        global< std::vector< T >, S > & xs, const Compare & compare )
{
    const size_t local_count = xs.size();
    const lpf_pid_t P = ctx.nprocs();
    const lpf_pid_t s = ctx.pid();
    (void) s;

    global< std::vector< T > , temporary > local_samples( tmp, P-1, T(), ctx.p_rel );
    global< std::vector< T > , temporary > all_samples( tmp, (P-1)*P, T(), ctx.p_rel );
    global< std::vector< T > , temporary > sorted_samples( tmp, (P-1)*P, T(), ctx.p_rel );
    global< std::vector< lpf_pid_t >, temporary > multi_queue_begin( tmp, P, 0u, ctx.zero_rel );
    global< std::vector< lpf_pid_t >, temporary > multi_queue_idx( tmp, P, P-1, ctx.zero_rel );


    // 1) local sort
    std::sort( xs.begin(), xs.end(), compare );

    // 2) take samples at regular intervals
    for (lpf_pid_t i=0; i < P-1; ++i )
    {
        size_t k = local_count * (i+1) / P;
        const T deflt = T(); // a default value for when the local process
                             // does not have any elements to sort
        local_samples[i] = k < local_count ? xs[k] : deflt;
    }

    ctx.fence();
    allgather( local_samples, all_samples );
    ctx.fence();

    // 3) do a multi-merge of all_samples (replicated)
    //
    for ( lpf_pid_t p = 0; p < P; ++p ) {
        multi_queue_begin[p] = p * (P-1);
        multi_queue_idx[p] = P-1;
    }

    detail::multi_merge(tmp, multi_queue_begin, multi_queue_idx,
            all_samples, sorted_samples, compare );

    // 4) choose P-1 pivot values from P*(P-1) samples (replicated)
    // I can reuse the memory for local_samples for that (evil)
    global< std::vector< T >, temporary > pivots( tmp, P-1, T(), ctx.zero_rel);
    for (lpf_pid_t j=0; j < P-1; ++j )
        pivots[j] = sorted_samples[ (2*j + 1)*P/2 ];

    // 5) Partition all locally sorted data in P groups
    //
    // first count
    
    global< std::vector< size_t >, temporary > src_count( tmp, P, 0ul, ctx.p_rel );
    global< std::vector< size_t >, temporary > src_offsets( tmp, P, 0ul, ctx.p_rel );
    global< std::vector< size_t >, temporary > dst_count( tmp, P, 0ul, ctx.p_rel );
    global< std::vector< size_t >, temporary > dst_offsets( tmp, P, 0ul, ctx.p_rel );

    // count the number of elements to scatter to each process
    { lpf_pid_t pivot = 0;
      for ( size_t i = 0; i < local_count; ++i ) {
          while ( pivot < P-1 && compare(pivots[pivot], xs[i]) ) ++pivot;
          // now: pivots[pivot] >= xs[i] s.t. pivots[pivot-1] < xs[i]
          //      ( xs[i] <= pivots[pivot] s.t. xs[i] > pivots[pivot-1] )
          //      OR pivot == P-1

          src_count[ pivot ] += 1;
      }
    }

    // compute prefix sums of src_count, so we have offsets
    { size_t offset = 0;
      for (lpf_pid_t p = 0; p < P; ++p ) {
        src_offsets[p] = offset;
        offset += src_count[p];
      }
    }

    ctx.fence();
    alltoall( src_count, dst_count );
    alltoall( src_offsets, dst_offsets );
    ctx.fence();

    const size_t new_local_count = std::accumulate( 
            dst_count.begin(), dst_count.end(), 0ul );
    global< size_t , temporary > max_local_count( tmp, new_local_count );
    ctx.fence();
    allreduce( max_local_count, max_op<size_t>() );
    ctx.fence();

    global< std::vector< T >, temporary > ys( tmp, *max_local_count, T(), ctx.p_rel );
    xs.reserve( *max_local_count );
    
    { size_t offset = 0;
      for ( lpf_pid_t p = 0; p < P; ++p ) {
        size_t xs_begin = dst_offsets[p];
        size_t xs_end = xs_begin + dst_count[p]; 

        size_t ys_begin = offset;
        size_t ys_end = ys_begin + dst_count[p];
        offset += dst_count[p];
         
        copy( ctx.one_rel, p, xs, xs_begin, xs_end, ys, ys_begin, ys_end );
    } }

    ctx.fence();

    ys.resize( new_local_count );
    xs.resize( new_local_count );
   
    { size_t offset = 0; 
        for ( lpf_pid_t p = 0; p < P; ++p ) {
            multi_queue_begin[p] = offset;
            multi_queue_idx[p] = dst_count[p];
            offset += dst_count[p];
        }
    }

    detail::multi_merge(tmp, multi_queue_begin, multi_queue_idx,
            ys, xs, compare );
}

template < class T, template <class X> class S >
void sort( machine & ctx, temporary_space & tmp,
        global< std::vector< T >, S > & xs )
{
    sort( ctx, tmp, xs, std::less<T>() );
}


}

#endif
