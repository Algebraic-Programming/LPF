#include "ibverbsNoc.hpp"

namespace lpf 
{
namespace mpi
{

    struct IBVerbsNoc::Exception : std::runtime_error {
        Exception(const char * what) : std::runtime_error( what ) {}
    };

    IBVerbsNoc::IBVerbsNoc(Communication & comm) : IBVerbs(comm)
    {
    }

    IBVerbs::SlotID IBVerbsNoc :: regLocal( void * addr, size_t size )
    {
        printf("Enter IBVErbsNoc::regLocal\n");
        ASSERT( size <= m_maxRegSize );

        MemorySlot slot;
        if ( size > 0) {
            LOG(4, "Registering locally memory area at " << addr << " of size  " << size );
            struct ibv_mr * const ibv_mr_new_p = ibv_reg_mr(
                    m_pd.get(), addr, size,
                    IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE
                    );
            if( ibv_mr_new_p == NULL )
                slot.mr.reset();
            else
                slot.mr.reset( ibv_mr_new_p, ibv_dereg_mr );
            if (!slot.mr) {
                LOG(1, "Could not register memory area at "
                        << addr << " of size " << size << " with IB device");
                throw Exception("Could not register memory area");
            }
        }
        MemoryRegistration local;
        local.addr = addr;
        local.size = size;
        local.lkey = size?slot.mr->lkey:0;
        local.rkey = size?slot.mr->rkey:0;

        SlotID id =  m_memreg.addNocReg( slot );

        m_memreg.update( id ).glob.resize( m_nprocs );
        m_memreg.update( id ).glob[m_pid] = local;
        LOG(4, "Memory area " << addr << " of size " << size << " has been locally registered as NOC slot. Slot = " << id );
        return id;
    }

} // namespace mpi
} // namespace lpf
