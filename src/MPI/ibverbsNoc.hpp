#pragma once

#include "ibverbs.hpp"

namespace lpf 
{

namespace mpi 
{
    class _LPFLIB_LOCAL IBVerbsNoc : public IBVerbs {
        public:
            IBVerbsNoc(Communication & comm) : IBVerbs(comm) {}
            //~IBVerbsNoc() {}

            IBVerbs::SlotID regNoc( void * addr, size_t size );

    };
} // namespace mpi
} // namespace lpf
