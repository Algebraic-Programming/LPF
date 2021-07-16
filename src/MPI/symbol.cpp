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

#ifndef LPF_ON_MACOS
#ifndef _GNU_SOURCE
   #error A glibc extension of the dynamic linker is required. Please use GNU libc
#endif
#endif
#include <dlfcn.h>

#include "symbol.hpp"
#include "log.hpp"

namespace lpf {

Symbol :: LookupException :: LookupException( const std::string & reason ) : 
     std::runtime_error(reason)
 {}

Symbol :: Symbol()
    : m_impl()
{}

Symbol :: Symbol( const void * symbol )
    : m_impl( new Impl )
{
    m_impl->m_fileHandle = NULL;
    m_impl->m_symbol = const_cast<void *>(symbol);

    (void) dlerror(); // clear libdl error state
    // Lookup the symbol name of the symbol
    // NOTE: This works with GCC only with Position Independent Code
    // (compile source with SPMD section with -fPIC)
    Dl_info info;
    if ( 0 != dladdr( m_impl->m_symbol, &info ) 
            && NULL != info.dli_sname )
    {
        m_impl->m_fileName = info.dli_fname ;
        m_impl->m_symbolName = info.dli_sname;
    }
    else
    {
        throw LookupException("Could not resolve symbol name by address");
    }
}

Symbol :: Symbol( const std::string & file, const std::string & name )
    : m_impl( new Impl)
{
    m_impl->m_fileHandle = NULL;
    m_impl->m_symbol = NULL;
    m_impl->m_fileName = file;
    m_impl->m_symbolName = name;

    (void) dlerror();// clear libdl error state

    // Open a handle to the object file
    m_impl->m_fileHandle = dlopen( file.c_str(), RTLD_NOW );

    // it is quite likely that the requested object is in the current
    // program. It is illegal to open that, which will yield an error
    (void) dlerror(); // clear any error state

    // continue to load the symbol. If handle==NULL the symbol is searched
    // in the current executable.
    m_impl->m_symbol = dlsym( m_impl->m_fileHandle, name.c_str() );

    const char * error = dlerror();
    if ( error )
    {
        throw LookupException("Could not resolve symbol address by name, "
                "because" + std::string(error) );
    }
}

Symbol :: Impl :: ~Impl()
{
    if (m_fileHandle) {
        if (dlclose( m_fileHandle )) {
            LOG(1, "Error closing shared object file '" << m_fileName << "'");
        }
    }
    m_fileHandle = NULL;
    m_symbol = NULL;
}

void * Symbol :: getSymbol() const
{
    return m_impl ? m_impl->m_symbol : NULL;
}

std::string Symbol :: getSymbolName() const
{
    return m_impl ? m_impl->m_symbolName : "";
}


std::string Symbol :: getFileName() const
{
    return m_impl ? m_impl->m_fileName : "";
}

}
