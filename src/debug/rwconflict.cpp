
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

#include "rwconflict.hpp"
#include "assert.hpp"

namespace lpf { namespace debug {

void ReadWriteConflict ::
    insertRead( void * address, size_t size ) 
{
    char * begin = static_cast<char *>(address);
    char * end = begin + size;

    if (size == 0) return;

    if (m_read_map.empty()) {
        m_read_map[begin] = size;
        return;
    }

    ReadMMap::iterator i = m_read_map.lower_bound( begin );
    ReadMMap::iterator j = i;
    if (j != m_read_map.begin()) {
        --j; // now j points to the first chunk starting before the new range
    }
    char * a = static_cast<char *>(j->first);
    char * b = a + j->second;

    // If the new range is completely contained in a chunk
    //
    //     j ----------------------.
    //     |  N-------------\      |
    //     |  |             |      |   N = new chunk, j = original chunk
    //     |  |             |      |
    //     |  |begin     end|      |
    //     |  \-------------/      |
    //     \-----------------------/
    //     a                       b
    //                            
    //  ignore the new range:
    if ( a < begin && end <= b ) {
        return;
    }
    
    // Now, try to build a range [j,k] of chunks that overlap
    //  If 'i' was already the first existing block, then 'j'
    //  points to it now too.
    //
    //     N --------------.
    //     |        j,i----|-----.
    //     |        |      |     |     N = new chunk
    //     |begin   |   end|     |
    //     \--------|------/     |
    //              \------------/
    //              a            b
    //
    // otherwise 'j' points to the first block that starts before
    // the new block
    //  For example:
    //
    //     j---------------.        i---
    //     |        N------|-----.  |
    //     |        |      |     |  |   N = new chunk
    //     |a       |     b|     |  |
    //     \--------|------/     |  \---
    //              \------------/
    //              begin      end
    // 
    //
    //     N ------------------------.
    //  j--|-----------\ i--\     /--|--
    //  |  |           | |  |     |  |     N = new chunk
    //  |  |           | |  | ... |  |
    //  |a |          b| |  |     |  |  
    //  \--|-----------/ \--/     \--|--
    //     \-------------------------/
    //     begin                  end                       
    //
    //
    //     j----.      i-----------
    //     |    |   N--|---------.
    //     |    |   |  |         |      N = new chunk
    //     |    |   |  |         |   
    //     \----/   |  \---------|--
    //     a    b   \------------/
    //              begin      end
    //
    // This last situation is trivial. Check for that condition, solve it
    // and make 'j' the first block that intersects with the new block

    if ( j != m_read_map.end() && j->first + j->second < begin )
        ++j;

    ASSERT( j == m_read_map.end() 
            || j->first > end 
            || j->first + j->second >= begin 
          );
     
    if ( j == m_read_map.end() || j->first > end ) {
        // then no block intersects with the new block
        //
        // so we can just add the block
        m_read_map[ begin ] = size_t( end - begin );

        // and quit
        return;
    }

    a = j->first;
    b = a + j->second;

    ASSERT( a <= end );
    ASSERT( b >= begin );

    // Now we try to find the last intersecting block, i.e. that starts
    // before the end
    // first we find the first block that starts after the end
    ReadMMap::iterator k = m_read_map.lower_bound( end );

    ASSERT( k == m_read_map.end() || k->first >= end ); 

    // since 'j' already intersects with this block, we know
    if ( k == m_read_map.end() || k->first > end )
    {
        ASSERT( k != m_read_map.begin() ) ;
        --k;
        ASSERT( k != m_read_map.end() ) ;
    }
    // now k is the first block starting before or at the end.
    // possibly j == k

    char * c = k->first;
    char * d = c + k->second;

    // k must intersect with the new block
    ASSERT( d >= begin );
    ASSERT( c <= end );

    //
    //     N ------------------------.
    //  j--|-----------. /--.     k--|--
    //  |  |           | |  |     |  |     N = new chunk
    //  |  |           | |  | ... |  |
    //  |a |          b| |  |     |c | d
    //  \--|-----------/ \--/     \--|--
    //     \-------------------------/
    //     begin                  end                       
    
    //
    //     N -----------------------.
    //     |  j--------.     k------|--
    //     |  |        |     |      |   N = new chunk
    //     |  |        | ... |      |
    //     |  |a      b|     |c     | d
    //     |  \--------/     \------|--
    //     \-----------------------/
    //                            

    // we are going to replace the range [j,k] with a
    // the merged version 
    begin = std::min( a, begin);
    end = std::max( d, end );
    ++k; m_read_map.erase( j, k );
    m_read_map[begin] = size_t( end - begin );
}

    // returns true iff there is a conflict
bool ReadWriteConflict 
    :: checkWrite( void * address, size_t size )  const
{
    char * begin = static_cast<char *>(address);
    char * end = begin + size;

    if (size == 0 ) return false;

    ReadMMap::const_iterator i = m_read_map.lower_bound( begin );

    // by definition of ::lower_bound
    ASSERT( i == m_read_map.end() || i->first >= begin ); 
     
    if ( i != m_read_map.begin() && 
            ( i == m_read_map.end() || i->first >= end) ) {
        // then 'i' is just behind the block to be written
        // rewind one
        --i;

        // now, 'i' is the first block starting before 'begin'
        ASSERT( i != m_read_map.end() );
        ASSERT( i->first < begin );

        // return true iff the end of 'i' is after begin
        return i->first + i->second > begin;
    }

    ASSERT( i == m_read_map.end() || i->first >= begin );
    ASSERT( i == m_read_map.begin() || i->first < end );
    return i != m_read_map.end() && i->first < end ;
}
    
void ReadWriteConflict
    :: printReadMap( std::ostream & out) const
{
    for (ReadMMap::const_iterator i = m_read_map.begin(); i != m_read_map.end(); ++i) {
        out << "[ " << static_cast<void*>( i->first ) 
            << ", " << static_cast<void*>( i->first + i->second ) 
            << ") ";
    }
}

} }
