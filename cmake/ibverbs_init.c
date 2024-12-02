#include "infiniband/verbs.h"
#include <stdlib.h>

int main(int argc, char** argv) 
{

    int numDevices = -1;
    struct ibv_device * * const try_get_device_list = ibv_get_device_list( &numDevices );

    if (!try_get_device_list) {
	abort();
    }

    if (numDevices < 1) {
	abort();
    }

    return 0;
}
