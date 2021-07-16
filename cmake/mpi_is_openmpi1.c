
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

#include <mpi.h>
#if OMPI_MAJOR_VERSION == 1 
    // OpenMPI 1 cannot deal with threads
    #error OpenMPI 1 does not work well in the presence of threads.
#endif

int main(int argc, char ** argv)
{
    (void) argc;
    (void) argv;
    return 0;
}
