
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

#include "stack.hpp"

#include <cstdio>
#include <execinfo.h>
#include <unistd.h>
#include <cxxabi.h>
#include <cstring>
#include <cstdlib>
#include <climits>

#include "suppressions.h"


void lpfAssertionFailure( const char  * func, const char * file,
        const char * cond, int line )
{
     char abortMsg[200];
     if ( 0 < snprintf( abortMsg, sizeof(abortMsg),
            "Assertion failure in function %s at %s:%d\n"
            "      -> %s is false!",
            func, file, line, cond ) ) {
         lpfInternalPrintStack( unsigned(-1), abortMsg, 1 );
     }
     else
     {   char defaultMsg[] = "Assertion failure";
         lpfInternalPrintStack( unsigned(-1), defaultMsg, 1 );
     }
     std::abort();
}

void lpfInternalPrintStack( unsigned pid, char * name, int fd ) {
    void *btbuf[200];
    char buffer[10000];
    size_t pos = 0;

    char pidbuf[13]="[---]";
    if (pid != UINT_MAX) {
        (void) std::snprintf(pidbuf, sizeof(pidbuf), "[%03u]", pid );
    }

    int btlen = backtrace(btbuf, sizeof(btbuf)/sizeof(btbuf[0]));
    char ** bt =  backtrace_symbols( btbuf, btlen );
    if (name == NULL) {
        pos += size_t(std::snprintf( buffer+pos, sizeof(buffer)-pos,
            "%s ********** NO NAME **********************\n", pidbuf));
    }
    else if (std::strlen(name) <= 20 ) {
        pos += size_t(std::snprintf( buffer+pos, sizeof(buffer)-pos,
            "%s ********** %20s *********** \n", pidbuf, name));
    }
    else {
        pos += size_t(std::snprintf( buffer+pos, sizeof(buffer)-pos,
                "%s ***************************************** \n", pidbuf));
        char * nl = NULL;
        while( (nl = std::strchr( name+1, '\n')) ) {
           *nl = '\0';
           pos += size_t(std::snprintf( buffer+pos, sizeof(buffer)-pos,
                "%s * %s \n", pidbuf, name));
           name = nl + 1;
        }
       pos += size_t(std::snprintf( buffer+pos, sizeof(buffer)-pos,
            "%s * %s \n", pidbuf, name));
        pos += size_t(std::snprintf( buffer+pos, sizeof(buffer)-pos,
                "%s ***************************************** \n", pidbuf));
    }
    char * demangled = NULL;
    size_t demangledSize = 0;
    for (int i = 0; i < btlen; ++i) {
        int status = 2;
        char *mangStart = std::strchr(bt[i], '(');
        char *mangEnd = mangStart ? std::strchr(mangStart+1, '+') : NULL;
        if (mangStart && mangEnd) {
            *mangStart='\0';
            *mangEnd = '\0';
            demangled = abi::__cxa_demangle( mangStart+1, demangled, &demangledSize, &status);
            if (status != 0 ) {
                *mangStart='(';
                *mangEnd='+';
            }
        }
        if (mangStart == NULL || mangEnd == NULL || status == -2)
            pos += size_t(std::snprintf( buffer+pos, sizeof(buffer)-pos,
                    "%s %d) %s\n", pidbuf, i, bt[i]));
        else if (status == 0 && mangStart && mangEnd )
            pos += size_t(std::snprintf( buffer+pos, sizeof(buffer)-pos,
                    "%s %d) %s(%s+%s\n", pidbuf, i, bt[i], demangled, mangEnd+1));
        else pos += size_t(std::snprintf( buffer+pos, sizeof(buffer)-pos,
                    "%s %d) internal error while resolving symbol\n", pidbuf, i));
    }
    pos += size_t(std::snprintf( buffer+pos, sizeof(buffer)-pos,
            "%s ----------------------------------------- \n", pidbuf));


    // attempt to write the stack trace.
    // If we fail however, tough luck-- we'll call std::abort after this anyway
    LPFLIB_IGNORE_UNUSED_RESULT
    (void) write( fd, buffer, pos);
    LPFLIB_RESTORE_WARNINGS
    std::free(demangled);
    std::free(bt);
}

