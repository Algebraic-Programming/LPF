
#
#   Copyright 2021 Huawei Technologies Co., Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# assume MPI was compiled with GCC, as in most Linux distributions;
# with recent distributions, MPI may contain Link-Time Optimization
# flags, which are compiler-specific and thus not portable:
# if the compiler is not gcc, disable LTO and allow FindMPI to
# make MPI work with Clang
if ( CMAKE_CXX_COMPILER_ID STREQUAL "Clang" )
     set( MPI_COMPILER_FLAGS "-fno-lto" )
     message( STATUS "detecting MPI with Clang by disabling LTO" )
endif()
find_package(MPI)

# Find the 'mpirun' frontend
string( REGEX REPLACE "exec$" "run" mpirun "${MPIEXEC}" )
if (NOT EXISTS ${mpirun})
    set(mpirun ${MPIEXEC})
endif()
set( MPIRUN "${mpirun}" CACHE STRING "The mpirun script " )

set(MPI_RMA FALSE)
if (MPI_FOUND)
    set(MPI_RMA TRUE)
# Exclude C++ bindings on OpenMPI
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOMPI_SKIP_MPICXX=1 ")
# Exclude C++ bindings on IBM Platform MPI    
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_MPICC_H=1")


# Test whether we have MPI-3.1 with windows support MPI_WIN_UNIFIED
    try_run( MPI_WIN_UNIFIED_RC MPI_WIN_UNIFIED_COMPILES
           ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mpi_win_unified.c
           LINK_LIBRARIES ${MPI_C_LIBRARIES}
           CMAKE_FLAGS 
                -DCMAKE_C_FLAGS:STRING=${MPI_C_COMPILE_FLAGS}
                -DCMAKE_EXE_LINKER_FLAGS:STRING=${MPI_C_LINK_FLAGS}
                -DINCLUDE_DIRECTORIES:STRING=${MPI_C_INCLUDE_PATH}
           ARGS ${MPIRUN}
           )
    set(MPI_WIN_UNIFIED NO)
    if ( MPI_WIN_UNIFIED_RC EQUAL 0 AND MPI_WIN_UNIFIED_COMPILES)
      set(MPI_WIN_UNIFIED YES)
    endif()


    try_compile( MPI_IS_NOT_OPENMPI1 "${CMAKE_BINARY_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mpi_is_openmpi1.c"
           LINK_LIBRARIES ${MPI_C_LIBRARIES}
           CMAKE_FLAGS 
                -DCMAKE_C_FLAGS:STRING=${MPI_C_COMPILE_FLAGS}
                -DCMAKE_EXE_LINKER_FLAGS:STRING=${MPI_C_LINK_FLAGS}
                -DINCLUDE_DIRECTORIES:STRING=${MPI_C_INCLUDE_PATH}
           )


    try_compile( IS_NOT_OPENMPI2 "${CMAKE_BINARY_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mpi_is_openmpi2.c"
           LINK_LIBRARIES ${MPI_C_LIBRARIES}
           CMAKE_FLAGS 
                -DCMAKE_C_FLAGS:STRING=${MPI_C_COMPILE_FLAGS}
                -DCMAKE_EXE_LINKER_FLAGS:STRING=${MPI_C_LINK_FLAGS}
                -DINCLUDE_DIRECTORIES:STRING=${MPI_C_INCLUDE_PATH}
           )


    try_compile( IS_NOT_MVAPICH2 "${CMAKE_BINARY_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mpi_is_mvapich2.c"
           LINK_LIBRARIES ${MPI_C_LIBRARIES}
           CMAKE_FLAGS 
                -DCMAKE_C_FLAGS:STRING=${MPI_C_COMPILE_FLAGS}
                -DCMAKE_EXE_LINKER_FLAGS:STRING=${MPI_C_LINK_FLAGS}
                -DINCLUDE_DIRECTORIES:STRING=${MPI_C_INCLUDE_PATH}
           )


    try_run( MPI_IS_THREAD_COMPAT_RC MPI_IS_THREAD_COMPAT_COMPILES
           ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mpi_is_thread_compat.c
           LINK_LIBRARIES ${MPI_C_LIBRARIES}
           CMAKE_FLAGS 
                -DCMAKE_C_FLAGS:STRING=${MPI_C_COMPILE_FLAGS}
                -DCMAKE_EXE_LINKER_FLAGS:STRING=${MPI_C_LINK_FLAGS}
                -DINCLUDE_DIRECTORIES:STRING=${MPI_C_INCLUDE_PATH}
           ARGS ${MPIRUN}
           )
    set( MPI_IS_THREAD_COMPAT NO)
    if ( MPI_IS_THREAD_COMPAT_RC EQUAL 0 AND MPI_IS_THREAD_COMPAT_COMPILES )
      set(MPI_IS_THREAD_COMPAT YES)
    endif()


    try_run( MPI_OPEN_PORT_RC MPI_OPEN_PORT_COMPILES
           ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mpi_open_port.c
           LINK_LIBRARIES ${MPI_C_LIBRARIES}
           CMAKE_FLAGS 
                -DCMAKE_C_FLAGS:STRING=${MPI_C_COMPILE_FLAGS}
                -DCMAKE_EXE_LINKER_FLAGS:STRING=${MPI_C_LINK_FLAGS}
                -DINCLUDE_DIRECTORIES:STRING=${MPI_C_INCLUDE_PATH}
           ARGS ${MPIRUN}
           )
    set( MPI_OPEN_PORT NO)
    if ( MPI_OPEN_PORT_RC EQUAL 0 AND MPI_OPEN_PORT_COMPILES )
      set(MPI_OPEN_PORT YES)
    endif()


    try_run( MPI_IBARRIER_RC MPI_IBARRIER_COMPILES
           ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mpi_ibarrier.c
           LINK_LIBRARIES ${MPI_C_LIBRARIES}
           CMAKE_FLAGS 
                -DCMAKE_C_FLAGS:STRING=${MPI_C_COMPILE_FLAGS}
                -DCMAKE_EXE_LINKER_FLAGS:STRING=${MPI_C_LINK_FLAGS}
                -DINCLUDE_DIRECTORIES:STRING=${MPI_C_INCLUDE_PATH}
           ARGS ${MPIRUN}
           )
    set( MPI_IBARRIER NO)
    if ( MPI_IBARRIER_RC EQUAL 0 AND MPI_IBARRIER_COMPILES )
      set(MPI_IBARRIER YES)
    endif()

    string(STRIP "${MPI_C_LIBRARIES}" f1)
    string(REPLACE ";" " " f2 "${f1}")
    set( MPI_C_LIB_CMDLINE "${f2}" )

    string(STRIP "${MPI_CXX_LIBRARIES}" f1 )
    string(REPLACE ";" " " f2 "${f1}")
    set( MPI_CXX_LIB_CMDLINE "${f2}" )

    string(STRIP "${MPI_C_LINK_FLAGS}" f1)
    string(REPLACE " " ";" f2 "${f1}")

    set( MPI_C_LINK_OPTIONS ${f2} )

    string(STRIP "${MPI_CXX_LINK_FLAGS}" f1)
    string(REPLACE " " ";" f2 "${f1}")

    set( MPI_CXX_LINK_OPTIONS ${f2} )

    string(STRIP "${MPI_C_COMPILE_FLAGS}" f1)
    string(REPLACE " " ";" f2 "${f1}")

    set( MPI_C_COMPILE_OPTIONS ${f2} )

    string(STRIP "${MPI_CXX_COMPILE_FLAGS}" f1)
    string(REPLACE " " ";" f2 "${f1}")

    set( MPI_CXX_COMPILE_OPTIONS ${f2} )

endif()


