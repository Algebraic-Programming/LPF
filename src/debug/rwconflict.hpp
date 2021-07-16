
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

#ifndef PLATFORM_CORE_DEBUG_RWCONFLICT_H
#define PLATFORM_CORE_DEBUG_RWCONFLICT_H

#include <map>
#include <iostream>

namespace lpf { namespace debug {

class ReadWriteConflict 
{
    typedef std::map< char *, size_t > ReadMMap;

public:
    bool empty() const
    { return m_read_map.empty(); }

    void clear() 
    { m_read_map.clear(); }

    size_t chunkCount() const
    { return m_read_map.size(); }
    
    void insertRead( void * address, size_t size ) ;
    bool checkWrite( void * address, size_t size ) const ;
    void printReadMap( std::ostream & out) const ;

private:

    ReadMMap m_read_map;
}; 


} }


#endif
