
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

cmake_minimum_required(VERSION 3.1)
project(LPF C CXX ASM)

# Version info
set(VERSION_MAJOR "1")
set(VERSION_MINOR "0")
set(VERSION_PATCH "0")
set(VERSION_PACKAGE "1")

# Integrate Jenkins build information
#if(DEFINED ENV{SVN_REVISION})
#    set(VERSION_PATCH "${VERSION_PATCH}.$ENV{SVN_REVISION}")
#endif()
#
#if(DEFINED ENV{BUILD_NUMBER})
#    set(VERSION_PATCH "${VERSION_PATCH}.$ENV{BUILD_NUMBER}")
#endif()

# Version for DLLs / SOs
set(SOVERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
if (Linux)
  set(SOSUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}.${SOVERSION}")
elseif(APPLE)
  set(SOSUFFIX ".${SOVERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}")
else()
  set(SOSUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif()
string(TIMESTAMP BUILD_DATE "%Y-%m-%dT%H:%M:%SZ" UTC)

# Packaging
set(CPACK_PACKAGE_NAME "LPF")
set(CPACK_PACKAGE_CONTACT "wijnand.suijlen@huawei.com" )
set(CPACK_PACKAGE_VENDOR "Huawei Technologies")
set(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${VERSION_PATCH}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
	"A high performance BSP communications library" )

set(CPACK_SOURCE_GENERATOR "TGZ" )
set(CPACK_SOURCE_IGNORE_FILES "/\\\\.git/" "/\\\\.svn/" "\\\\.swp$" "/site/" "/build/" "/pclint/" "/junit/" "/ideas/" )
set(CPACK_SOURCE_PACKAGE_FILE_NAME
	"LPF-${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}-${VERSION_PACKAGE}")

set(CPACK_GENERATOR "RPM")
set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
set(CPACK_PACKAGE_FILE_NAME "LPF-${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}-${VERSION_PACKAGE}-${CPACK_RPM_PACKAGE_ARCHITECTURE}")
set(CPACK_RPM_PACKAGE_RELEASE      "${VERSION_PACKAGE}" )
set(CPACK_RPM_PACKAGE_LICENSE      "Proprietary" )
set(CPACK_RPM_PACKAGE_GROUP        "Development/Libraries" )
set(CPACK_RPM_PACKAGE_VENDOR       "Huawei Technologies")
set(CPACK_RPM_PACKAGE_REQUIRES     "intel-mpi-rt, gcc >= 4.8, gcc-c++ >= 4.8" )
set(CPACK_RPM_PACKAGE_PROVIDES     "lpf" )
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST
        "/usr" "/usr/bin" "/usr/lib" "/usr/libexec" "/usr/share"
        "/usr/share/doc" "/usr/include"
   )
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_BINARY_DIR}/rpm-post-install" )
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_BINARY_DIR}/rpm-post-uninstall" )
file(WRITE "${CMAKE_BINARY_DIR}/rpm-post-install" "/usr/sbin/ldconfig\n")
file(WRITE "${CMAKE_BINARY_DIR}/rpm-post-uninstall" "/usr/sbin/ldconfig\n")

include(CPack)

# Naming
set(LPFLIB_CONFIG_NAME "generic" CACHE STRING
        "Name of to identify the build of this particular configuration")
if (NOT LPFLIB_CONFIG_NAME MATCHES "[_A-Za-z][_A-Za-z0-9]*")
   message( SEND_ERROR "The variable LPFLIB_CONFIG_NAME must be a valid C identifier; that is, it must match the regular expression [_A-Za-z][_A-Za-z0-9]*")
endif()


# Installation directories
include(GNUInstallDirs)
get_filename_component(prefix "${CMAKE_INSTALL_PREFIX}" ABSOLUTE)
set( INSTALL_BIN "${prefix}/bin" CACHE PATH "Installation path for executables")
set( INSTALL_LIB "${prefix}/lib" CACHE PATH
        "Installation path for static and dynamic libraries")
set( INSTALL_HELPERS "${prefix}/libexec/lpf" CACHE PATH
        "Installation path for helper scripts and executables")
set( INSTALL_DOC "${prefix}/share/doc/lpf" CACHE PATH
        "Installation path for documentation" )
set( INSTALL_MAN "${prefix}/share" CACHE PATH
        "Installation path for Unix man pages")
set( INSTALL_CMAKE_MODULES "${prefix}/lib/lpf/cmake" CACHE PATH
        "Installation path for exported CMake module files")
set( INSTALL_HEADERS "${prefix}/include" CACHE PATH
        "Installation path for header files" )
message( STATUS "Installation directory prefix is ${prefix}")

# C++ standard
find_file(TR1_ARRAY "tr1/array")
if (TR1_ARRAY)
    message(STATUS "Governing C++ standard is C++98/TR1")
    set(CMAKE_CXX_STANDARD 98)
    set(CMAKE_CXX_STANDARD_REQUIRED YES)
else()
    message(STATUS "Governing C++ standard is C++11")
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED YES)
endif()

# Dependencies
set(ENGINES)
find_library( LIB_POSIX_THREADS
    NAMES "pthread"
    DOC   "Posix Threads"
    )

if ( LIB_POSIX_THREADS )
    list(APPEND ENGINES "pthread")
endif()

find_library( LIB_MATH
        NAMES m
        DOC "Ansi C99 Math library"
        )

find_library( LIB_DL
        NAMES dl
        DOC "Posix dynamic linker with GNU extensions"
        )

find_library( LIB_RT
        NAMES rt       # On Linux systems, this will be in librt.so 
              System.B # On MacOS systems, these functions reside in 
                       # libSystem.B.dylib
        DOC "Posix Real Time"
)

find_library( LIB_IBVERBS
        NAMES ibverbs
        DOC "Infiniband verbs" )

include(cmake/hwloc.cmake)


# MPI
include(cmake/mpi.cmake)
if ( MPI_FOUND )
   if ( NOT MPI_WIN_UNIFIED )
       message( WARNING "Your MPI implementation does not support windows with MPI_WIN_UNIFIED. MPI RMA implementation will not be built")
       set(MPI_RMA FALSE)
   endif()
   if ( NOT MPI_IS_NOT_OPENMPI1 )
      message( WARNING "OpenMPI 1 doesn't work well in the presence of threads.")
   endif()
   if ( NOT IS_NOT_OPENMPI2 )
      message( WARNING "OpenMPI 2 has a broken MPI_Win_fence. MPI RMA implementation will not be built.")
      set(MPI_RMA FALSE)
   endif()
   if ( NOT IS_NOT_MVAPICH2 )
      message( WARNING "MVAPICH 2 has a broken MPI_Put, because it cannot handle large messages. MPI RMA implementation will not be built.")
      set(MPI_RMA FALSE)
   endif()
   if ( NOT MPI_IS_THREAD_COMPAT OR NOT MPI_IS_NOT_OPENMPI1 )
      message( WARNING "MPI implementation does not tolerate any threading. Hybrid implementation will not be built")
   endif()
   if ( NOT MPI_OPEN_PORT )
      message( WARNING "MPI implementation does not support dynamically connecting separate MPI processes. Hence, lpf_mpi_initialize_over_tcp will always fail.")
   endif()
   if ( NOT MPI_IBARRIER )
      message( WARNING "MPI implementation does not have MPI_Ibarrier, which is required to use the dense all-to-all algorithm on large (> 2 GB) meta-data exchanges")
   endif()
endif()

#enable MPI-RMA engine
if ( LIB_MATH AND LIB_DL AND MPI_FOUND )
    list(APPEND ENGINES "mpimsg")

    if (MPI_RMA)
        list(APPEND ENGINES "mpirma")
    endif()

    if (LIB_IBVERBS)
        list(APPEND ENGINES "ibverbs")
    endif()

endif()

#enable the hybrid engine
if ( LIB_POSIX_THREADS AND LIB_MATH AND LIB_DL AND MPI_FOUND
        AND MPI_IS_THREAD_COMPAT AND MPI_IS_NOT_OPENMPI1
	AND LIB_IBVERBS )
    list(APPEND ENGINES "hybrid")
    set(HYBRID_ENGINE_ENABLED on)
endif()

message( STATUS "The following engines will be built: ${ENGINES}")


# Enable all warning diagnostics by default
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()
if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-null-pointer-arithmetic -Wno-mismatched-tags")
endif()


# When system is not Linux, enable conditionally compiled blocks
if (APPLE)
    add_definitions(-DLPF_ON_MACOS=1)
endif()

# Restrict logging up to log level 2 for release builds: errors & warnings only
if ( CMAKE_BUILD_TYPE STREQUAL "Release" )
    add_definitions(-DLPF_CORE_LOG_LEVEL=2)
else() # and enable unlimited logging for all other configurations
    add_definitions(-DLPF_CORE_LOG_LEVEL=999)
endif()

# Make position independent code the default, because the libraries may end-up
# in a shared object / DLL
set( CMAKE_POSITION_INDEPENDENT_CODE ON )

# make sure only the API symbols are exported
add_definitions(-D_LPFLIB_DLL=1)
add_definitions(-DBSPLIB_DLL=1)


# Tests
option(LPF_ENABLE_TESTS
       "Enable unit and API tests. This uses Google Testing and Mocking Framework"
       OFF)
if (LPF_ENABLE_TESTS)
message(STATUS "Unit and API tests will be built")

# set testing timeout to 60 seconds
set(CMAKE_TESTING_TIMEOUT 60)

# import Google Testing Framework
include(cmake/googletest.cmake)

# Have directory to gather all the tests results
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/junit)
set(test_output "${CMAKE_BINARY_DIR}/junit")

# Have a macro to add a unit test
function(add_gtest testName)
    add_executable(${testName} ${ARGN})
    target_link_libraries(${testName} gtest_main)
    add_test(${testName} ${testName} --gtest_output=xml:${test_output}/ )
endfunction(add_gtest)

# Have a macro to add a unit test that should run with MPI
if (MPI_FOUND)
    function(add_gtest_mpi testName nprocs)
        add_executable(${testName} ${ARGN})
        target_link_libraries(${testName} ${MPI_C_LIBRARIES} gtest_main)
        foreach( p ${nprocs})
            set(mpmd)
            foreach( i RANGE 1 ${p})
                if (i GREATER 1)
                  set(mpmd ${mpmd} ":")
                endif()
                set(mpmd ${mpmd} ${MPIEXEC_NUMPROC_FLAG} 1 ${MPIEXEC_PREFLAGS}
                        ./${testName} --gtest_output=xml:${test_output}/${testName}_${i}of${p}.xml)
            endforeach(i)
            add_test(NAME ${testName}_${p}
                     COMMAND ${MPIRUN} ${mpmd}
                    )
        endforeach(p)
    endfunction(add_gtest_mpi)
endif(MPI_FOUND)

# Enable testing in CMake
enable_testing()
else(LPF_ENABLE_TESTS)
    message(STATUS "Unit and API tests will *not* be built")
    function(add_gtest testName)
       # Do nothing because tests are disabled
    endfunction(add_gtest)

    function(add_gtest_mpi testName nprocs)
       # DO nothing because tests are disabled
    endfunction(add_gtest_mpi)
endif(LPF_ENABLE_TESTS)

# Handling of compiler flags
function(target_add_compilation_flags target visibility)
    if (ARGC LESS 3)
      return()
    endif()

    string(REPLACE ";" " " ws_sep_flags ${ARGN})
    if ( visibility STREQUAL PRIVATE OR visibility STREQUAL PUBLIC)
      get_target_property(f ${target} COMPILE_FLAGS)
      if (f)
        set_target_properties(${target} PROPERTIES COMPILE_FLAGS "${f} ${ws_sep_flags}")
      else()
        set_target_properties(${target} PROPERTIES COMPILE_FLAGS "${ws_sep_flags}")
      endif()
    endif()
    if (visibility STREQUAL INTERFACE OR visibility STREQUAL PUBLIC)
      get_target_property(f ${target} INTERFACE_COMPILE_FLAGS)
      if (f)
        set_target_properties(${target} PROPERTIES INTERFACE_COMPILE_FLAGS "${f} ${ws_sep_flags}")
      else()
        set_target_properties(${target} PROPERTIES INTERFACE_COMPILE_FLAGS "${ws_sep_flags}")
      endif()
    endif()
endfunction(target_add_compilation_flags)

# add compilation flags to the target's compilation list and the list
# that is required to compile stuff that uses it (INTERFACE_COMPILE_FLAGS)
# Instead of flags, targets can also be passed along, whose flags will then
# be copied
function(target_compile_flags target)
  set(mode INIT)
  set(vis UNKNOWN)
  set(xs)
  foreach( a ${ARGN})
    if ( mode STREQUAL INIT )
      set(vis ${a})
      set(mode CONT)
    else()
      if (a STREQUAL PUBLIC OR a STREQUAL PRIVATE OR a STREQUAL INTERFACE)
         if (NOT (vis STREQUAL UNKNOWN) )
           target_add_compilation_flags( ${target} ${vis} ${xs})
         endif()
         set(vis ${a})
      else()
        set(f ${a})
        if (TARGET ${a})
          get_target_property(x ${target} INTERFACE_COMPILE_FLAGS)
          if (x)
            set(f ${x})
          else()
            set(f)
          endif()
        endif()
        list(APPEND xs ${f})
      endif()
    endif()
  endforeach(a)

  if (NOT (vis STREQUAL UNKNOWN) )
    target_add_compilation_flags( ${target} ${vis} ${xs})
  endif()
endfunction(target_compile_flags)

# Source
set(lpf_cflags)
set(lpf_lib_link_flags)
set(lpf_exe_link_flags)
include_directories(include)
include_directories(src/common)
add_subdirectory(src)

# Collating all compile & link flags
set(LPF_CORE_COMPILE_FLAGS "${lpf_cflags}" CACHE STRING "Compilation flags for all user code" )
set(LPF_CORE_LIB_LINK_FLAGS "${lpf_lib_link_flags}" CACHE STRING "Flags to link user libraries" )
set(LPF_CORE_EXE_LINK_FLAGS "${lpf_exe_link_flags}" CACHE STRING "Flags to link user executables" )

# Compiling LPF programmes in the build dir
function( target_link_exe_with_core target )
    set(engine "imp")
    if (ARGV1)
      set(engine "${ARGV1}")
    endif()
    set(corelib "lpf_core_univ_${engine}_${LPFLIB_CONFIG_NAME}")

    target_link_libraries(${target} ${corelib})
    target_compile_flags(${target} PRIVATE ${LPF_CORE_COMPILE_FLAGS})
    set_target_properties(${target} PROPERTIES
            LINK_FLAGS "${LPF_CORE_LIB_LINK_FLAGS} ${LPF_CORE_EXE_LINK_FLAGS}"
            LINKER_LANGUAGE CXX
    )
endfunction()

# Apps
add_subdirectory(src/utils)

# Tests
if (LPF_ENABLE_TESTS)
    add_subdirectory(tests)
endif (LPF_ENABLE_TESTS)

# Documentation
add_subdirectory(doc)
add_subdirectory(examples)


# Generation of frontends to be installed
configure_file( lpfcc.in  lpfcc  @ONLY)
configure_file( lpfcxx.in lpfcxx @ONLY)

# the ones that will be finally installed
set( hwloc_distrib ${HWLOC_DISTRIB})
set( hwloc_calc ${HWLOC_CALC})
set( lpfproxy ${INSTALL_HELPERS}/lpfproxy )
set( lpf_proxy_dummy ${INSTALL_HELPERS}/lpf_proxy_dummy)
set( lpf_probe ${INSTALL_HELPERS}/lpfprobe)
set( lpfrun ${INSTALL_BIN}/lpfrun )
set( lpfcc ${INSTALL_BIN}/lpfcc )
set( lpfcxx ${INSTALL_BIN}/lpfcxx )
set( lpfcore ${INSTALL_LIB}/liblpf_core_univ_ENGINE_${LPFLIB_CONFIG_NAME}${SOSUFFIX} )
configure_file( lpfrun.in lpfrun @ONLY)
configure_file( lpfproxy.in lpfproxy @ONLY)
configure_file( lpfprobe.in lpfprobe @ONLY)
configure_file( lpf-config.cmake.in lpf-config.cmake @ONLY)

# the ones that are used to do things within the build directory
set( hwloc_distrib ${HWLOC_DISTRIB})
set( hwloc_calc ${HWLOC_CALC})
set( lpfproxy ${CMAKE_CURRENT_BINARY_DIR}/lpfproxy_build )
set( lpf_proxy_dummy ${CMAKE_CURRENT_BINARY_DIR}/src/MPI/lpf_proxy_dummy)
set( lpf_probe ${CMAKE_CURRENT_BINARY_DIR}/src/utils/lpfprobe)
set( lpfrun ${CMAKE_CURRENT_BINARY_DIR}/lpfrun_build)
set( lpfcore ${CMAKE_CURRENT_BINARY_DIR}/src/*/liblpf_core_univ_ENGINE_${LPFLIB_CONFIG_NAME}${SOSUFFIX} )
configure_file( lpfrun.in lpfrun_build @ONLY)
configure_file( lpfproxy.in lpfproxy_build @ONLY)
configure_file( lpfprobe.in lpfprobe_build @ONLY)

# install the frontends and some helpers
install(PROGRAMS
            ${CMAKE_CURRENT_BINARY_DIR}/lpfcc
            ${CMAKE_CURRENT_BINARY_DIR}/lpfcxx
            ${CMAKE_CURRENT_BINARY_DIR}/lpfrun
            ${CMAKE_CURRENT_BINARY_DIR}/lpfprobe
        DESTINATION ${INSTALL_BIN}
       )
install(PROGRAMS
            ${CMAKE_CURRENT_BINARY_DIR}/lpfproxy
        DESTINATION ${INSTALL_HELPERS}
       )

install(FILES
            ${CMAKE_CURRENT_BINARY_DIR}/lpf-config.cmake
        DESTINATION ${INSTALL_CMAKE_MODULES}
       )

install(EXPORT lpf DESTINATION ${INSTALL_CMAKE_MODULES})

# Install Header files
install(DIRECTORY "include/lpf"    DESTINATION ${INSTALL_HEADERS})
install(DIRECTORY "include/bsp" DESTINATION ${INSTALL_HEADERS})
install(DIRECTORY "include/debug"  DESTINATION ${INSTALL_HEADERS}/lpf )

# Post install actions
add_subdirectory(post-install)

