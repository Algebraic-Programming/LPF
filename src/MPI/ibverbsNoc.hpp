#pragma once

#include "ibverbs.hpp"

namespace lpf 
{

namespace mpi 
{
    class _LPFLIB_LOCAL IBVerbsNoc : public IBVerbs {
        public:
            IBVerbsNoc(Communication & comm) : IBVerbs(comm) {}
            IBVerbs::SlotID regLocal( void * addr, size_t size );
            //~IBVerbsNoc() {}

    };
} // namespace mpi
} // namespace lpf
