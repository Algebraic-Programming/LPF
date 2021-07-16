
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

#include "config.hpp"

#include <cstdlib>
#include <sstream>
#include <string>
#include <cstring>
#include <limits>

#include <unistd.h> // for sysconf

#ifdef LPF_ON_MACOS
#include <sys/sysctl.h>
#else
#include <sys/sysinfo.h>
#endif

namespace lpf {

Config :: ReadError :: ReadError( const char * message )
    : std::runtime_error(  
            std::string( "Cannot read configuration: " ) + message 
         )
{}

Config & Config :: instance()
{
    static Config object;
    return object;
}

Config::SpinMode Config::getSpinMode() const
{
    const char * spinModeEnv = std::getenv("LPF_SPIN_MODE");
    std::string spinMode = spinModeEnv == NULL?"FAST":spinModeEnv;

    if ( spinMode == "FAST" )
        return SPIN_FAST;
    else if (spinMode == "PAUSE" )
        return SPIN_PAUSE;
    else if (spinMode == "YIELD" )
        return SPIN_YIELD;
    else if (spinMode == "COND" )
        return SPIN_COND;
    else throw ReadError("LPF_SPIN_MODE environment variable may only be FAST, PAUSE, YIELD, or COND");
}

lpf_pid_t Config :: getSysProcs() const
{
    return lpf_pid_t(sysconf( _SC_NPROCESSORS_ONLN ));
}

size_t Config :: getLocalRamSize() const
{
#ifdef LPF_ON_MACOS
    int mib[2] = { CTL_HW, HW_MEMSIZE };
    uint64_t memsize = 0;
    size_t len = sizeof(memsize);
    int rc = sysctl(mib, 2, &memsize, &len, NULL, 0);
    if (rc != 0)
        throw ReadError("MacOS system call to 'sysctl' returned -1");

    if (memsize > std::numeric_limits<size_t>::max())
        throw ReadError("Too much memory to handle");

    return memsize;
#else
    struct sysinfo info;
    int rc = sysinfo( &info );
    if (rc != 0 )
        throw ReadError("Linux system call to 'sysinfo' returned -1");

    if (info.totalram > std::numeric_limits<size_t>::max() / info.mem_unit)
        throw ReadError("Too much memory to handle");

    return size_t(info.totalram) * info.mem_unit;
#endif
}

lpf_pid_t Config :: getMaxProcs() const
{
    const char * maxProcsEnv = std::getenv( "LPF_MAX_PROCS" );

    if (maxProcsEnv && std::strlen(maxProcsEnv) > 0)
    {
        std::istringstream s(maxProcsEnv);

        lpf_pid_t p = 0;
        s >> p;

        if (!s)
            throw ReadError("LPF_MAX_PROCS environment variable was malformed");

        return p;
    }
    else
    {
        return 0;
    }
}

std::string Config :: getPinBitmap() const
{
    const char * pinningEnv = std::getenv( "LPF_PROC_PINNING" );

    if ( pinningEnv )
    {
        return pinningEnv;
    }
    else
    {
        return std::string();
    }
}

double Config :: getMaxSecondsForProbe() const
{
    double result = 1.0;

    const char * seconds = std::getenv("LPF_MAX_SECONDS_FOR_PROBE");

    if (seconds)
    {
        std::istringstream s(seconds);
        s >> result;

        if (!s)
            throw ReadError("LPF_MAX_SECONDS_FOR_PROBE environment variable was malformed. It must be a floating point number");
    }

    return result;
}


int Config :: getLogLevel() const
{
    const char * env = std::getenv("LPF_LOG_LEVEL");
    int level = 0;
    if (env)
    {
        std::istringstream s(env);
        s >> level;
        if (!s)
            throw ReadError("LPF_LOG_LEVEL must be unset or set to an integer");
    }

    return level;
}

int Config :: getBSPlibSafetyMode() const
{
    const char * env = std::getenv("LPF_BSPLIB_SAFETY_MODE");
    int safemode = 1;
    if (env)
    {
        std::istringstream s(env);
        s >> safemode;
        if (!s)
            throw ReadError("LPF_BSPLIB_SAFETY_MODE must be 1 or 0");
    }
    return safemode;
}

size_t Config :: getBSPlibMaxHpRegs() const
{
    const char * env = std::getenv("LPF_BSPLIB_MAX_HP_REGS");
    size_t maxHpRegs = std::numeric_limits<size_t>::max();
    if (env)
    {
        std::istringstream s(env);
        s >> maxHpRegs;
        if (!s)
            throw ReadError("LPF_BSPLIB_MAX_HP_REGS must be an unsigned integer or not defined at all");
    }
    return maxHpRegs;
}

std::string Config :: getEngine() const
{
    const char * env = std::getenv("LPF_ENGINE");
    if (env) {
        return std::string(env);
    }
    else
    {
        return "";
    }
}

Config::A2AMode Config::getA2AMode() const
{
    const char * a2aModeEnv = std::getenv("LPF_A2A_MODE");
    std::string a2aMode = a2aModeEnv == NULL?"SPARSE":a2aModeEnv;

    if ( a2aMode == "SPARSE" )
        return A2A_SPARSE;
    else if (a2aMode == "DENSE" )
        return A2A_DENSE;
    else if (a2aMode == "HOEFLER" )
        return A2A_HOEFLER;
    else throw ReadError("LPF_A2A_MODE environment variable may only be DENSE, SPARSE, or HOEFLER");
}

Config::TinyMsgSize Config :: getTinyMsgSize () const
{
    const char * env = std::getenv("LPF_TINY_MSG_SIZE");
    TinyMsgSize size = {16, 128};
    if (env)
    {
        std::istringstream s(env);
        s >> size.sizeForSharedMem;
        s >> size.sizeForMPI;
        if (!s)
            throw ReadError("LPF_TINY_MSG_SIZE must be a sequence of two unsigned integers or not defined at all");
    }
    return size;
}

namespace  { 
    int log2( size_t x ) {
        int y = -1;
        while (x) {
            y++;
            x >>= 1;
        }
        return y;
    }
}

int Config :: getMpiMsgSortGrainSizePower() const
{
    const char * env = std::getenv("LPF_WRITE_CONFLICT_BLOCK_SIZE");
    size_t blockSize = 1024;
    if (env)
    {
        std::istringstream s(env);
        s >> blockSize;
        if (!s || log2(blockSize) < 0 || (1ul << log2(blockSize)) != blockSize )
            throw ReadError("LPF_WRITE_CONFLICT_BLOCK_SIZE must be an unsigned integer equal to a power of two or not defined at all");
    }
    return log2( blockSize );
}

size_t Config :: getMaxMPIMsgSize() const
{
    const char * env = std::getenv("LPF_MPI_MAX_MSG_SIZE");
    int maxMsgSize = std::numeric_limits<int>::max();
    if (env)
    {
        std::istringstream s(env);
        s >> maxMsgSize;
        if (!s || maxMsgSize < 1)
            throw ReadError("LPF_MPI_MAX_MSG_SIZE must be a positive integer of at least one");
    }
    return size_t(maxMsgSize);
}


std::string  Config :: getIBDeviceName() const
{
    const char * name = std::getenv("LPF_INFINIBAND_DEVICE_NAME");
    if (name)
        return std::string(name);

    std::string empty;
    return empty;
}

int Config :: getIBPort() const 
{
    const char * env = std::getenv("LPF_INFINIBAND_PORT");
    int port = 1;
    if (env)
    {
        std::istringstream s(env);
        s >> port;
        if (!s || port < 1)
            throw ReadError("LPF_INFINIBAND_PORT must be a positive integer");
    }

    return port;
}


int Config :: getIBGidIndex() const 
{
    const char * env = std::getenv("LPF_INFINIBAND_GID_INDEX");
    int idx = -1;
    if (env)
    {
        std::istringstream s(env);
        s >> idx;
        if (!s)
            throw ReadError("LPF_INFINIBAND_GID_INDEX must be an integer");
    }

    return idx;
}

unsigned Config :: getIBMTU() const 
{
    const char * env = std::getenv("LPF_INFINIBAND_MTU_SIZE");
    unsigned mtu = 4096;
    if (env)
    {
        std::istringstream s(env);
        s >> mtu;
        if (!s)
            throw ReadError("LPF_INFINIBAND_MTU_SIZE must be 256, 512, 1024, 2048, or 4096");
    }

    return mtu;
}





}
