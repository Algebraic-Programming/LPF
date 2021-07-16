
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

#ifndef LPFLIB_BSPLIB_WRAPPER_H
#define LPFLIB_BSPLIB_WRAPPER_H

/**
 * \defgroup BSPLIB LPF BSPlib API Reference Documentation
 *
 * LPF can presents a group of networked computers as a Bulk Synchronous
 * Parallel (BSP) computer programmable by means of the BSPlib C API. This means
 * that programs using this library will adhere to the BSP cost model, in so far
 * the user can guarantee that no shared resource is used and that the only I/O
 * happens through the network interface as provided by the LPF BSPlib. The BSP
 * cost model states that the running time of a program will be no longer than:
 *
 * \f[ W + H g + S l \f]
 *
 * where W is the total amount of computation per node, H is the total amount
 * of communication per node, and S is the number of supersteps in which the
 * program is divided. The parameters g and l depend on the computers and how
 * they are networked. To determine them, careful benchmarking has to be
 * performed. More helpful explanation of the BSPlib interface and the BSP cost
 * model can be found in literature, e.g. :
 *
 *    -# "BSPlib: The BSP Programming Library" by Jonathan Hill et al.
 *    -# "Parallel Scientific Computing" by Rob H. Bisseling. 2004, Oxford
 *        University Press
 *
 * This document is a reference guide of the BSPlib API, which span the
 * following modules:
 *    - \link LPFLIB_SPMD The SPMD Framework\endlink and some remarks about
 *      \link LPFLIB_SPMD_IO I/O.\endlink
 *    - \link LPFLIB_DRMA The Direct Remote Memory Access API \endlink
 *    - \link LPFLIB_BSMP The Bulk Synchronous Message Passing API \endlink
 *
 * \author Wijnand Suijlen
 *
 * \par Copyright
 * Copyright (c) 2017 by Huawei Technologies Co., Ltd. \n
 * All rights reserved.
 *
 * @{
 */

#include <stddef.h>

// Symbol visibility macros
// copied from https://gcc.gnu.org/wiki/Visibility
//
// Generic helper definitions for shared library support
#if defined _WIN32 || defined __CYGWIN__
  #define BSPLIB_HELPER_DLL_IMPORT __declspec(dllimport)
  #define BSPLIB_HELPER_DLL_EXPORT __declspec(dllexport)
  #define BSPLIB_HELPER_DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define BSPLIB_HELPER_DLL_IMPORT __attribute__ ((visibility ("default")))
    #define BSPLIB_HELPER_DLL_EXPORT __attribute__ ((visibility ("default")))
    #define BSPLIB_HELPER_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
   #ifndef DOXYGEN
    #define BSPLIB_HELPER_DLL_IMPORT
    #define BSPLIB_HELPER_DLL_EXPORT
    #define BSPLIB_HELPER_DLL_LOCAL
   #endif
  #endif
#endif

// Now we use the generic helper definitions above to define BSPLIB_API and
// BSPLIB_LOCAL. BSPLIB_API is used for the public API symbols. It either DLL
// imports or DLL exports (or does nothing for static build). BSPLIB_LOCAL is
// used for non-api symbols.

#ifdef BSPLIB_DLL // defined if BSPLIB is compiled as a DLL
  #ifdef BSPLIB_DLL_EXPORTS // defined if we are building the BSPLIB DLL (instead of using it)
    #define BSPLIB_API BSPLIB_HELPER_DLL_EXPORT
  #else
    #define BSPLIB_API BSPLIB_HELPER_DLL_IMPORT
  #endif // BSPLIB_DLL_EXPORTS
  #define BSPLIB_LOCAL BSPLIB_HELPER_DLL_LOCAL
#else // BSPLIB_DLL is not defined: this means BSPLIB is a static lib.
 #ifndef DOXYGEN
  #define BSPLIB_API
  #define BSPLIB_LOCAL
 #endif
#endif // BSPLIB_DLL


#  ifdef __cplusplus
extern "C"
{
#  endif


#  if LPF_USE_MCBSP_API
   /**
    * \defgroup LPFLIB_TYPES Common Data Types
    * @{
    */

    /**
     * A type large enough to hold a process ID
     *
     *  \remark This typedef is non-standard
     */
    typedef unsigned int bsp_pid_t;

    /**
     * A type large enough to hold the number of processes.
     *
     *  \remark This typedef is non-standard
     */
    typedef unsigned int bsp_nprocs_t;

    /**
     * A type large enough to hold the size of any contiguous local memory
     * region.
     *
     *  \remark This typedef is non-standard
     */
    typedef size_t bsp_size_t;

    /**
     * A constant defining that the operation failed in an expected way.
     *
     * \remark This constant definition is non-standard
     */
    static const bsp_size_t bsp_size_unavailable = (bsp_size_t) -1;

    /**
     * @}
     */

    /**
     * \defgroup LPFLIB_MCBSP_API MulticoreBSP Compatibility
     * To enable source compatibility with MulticoreBSP a few functions are
     * provided.
     * @{
     */


    /**
     * This macro does nothing.
     *
     *  \remark This type is MulticoreBSP specific
     */
#    define mcbsp_disable_checkpointing()       /* empty */

    /**
     * This macro does nothing.
     *
     *  \remark This type is MulticoreBSP specific
     */
#    define mcbsp_enable_checkpointing()        /* empty */

    /**
     * @}
     */

#ifndef DOXYGEN
#define bsp_init   bsplibstd_init
#define bsp_abort  bsplibstd_abort
#define bsp_time   bsplibstd_time
#define bsp_sync   bsplibstd_sync
#define bsp_nprocs mcbsp_nprocs
#define bsp_pid   mcbsp_pid
#define bsp_push_reg mcbsp_push_reg
#define bsp_pop_reg bsplibstd_pop_reg
#define bsp_put   mcbsp_put
#define bsp_hpput mcbsp_hpput
#define bsp_get   mcbsp_get
#define bsp_hpget mcbsp_hpget
#define bsp_set_tagsize  mcbsp_set_tagsize
#define bsp_send  mcbsp_send
#define bsp_hpsend mcbsp_hpsend
#define bsp_qsize mcbsp_qsize
#define bsp_get_tag mcbsp_get_tag
#define bsp_move  mcbsp_move
#define bsp_hpmove mcbsp_hpmove
#endif

#else

#ifndef DOXYGEN
#define bsp_init   bsplibstd_init
#define bsp_abort  bsplibstd_abort
#define bsp_time   bsplibstd_time
#define bsp_sync   bsplibstd_sync
#define bsp_nprocs bsplibstd_nprocs
#define bsp_pid   bsplibstd_pid
#define bsp_push_reg bsplibstd_push_reg
#define bsp_pop_reg bsplibstd_pop_reg
#define bsp_put   bsplibstd_put
#define bsp_hpput bsplibstd_hpput
#define bsp_get   bsplibstd_get
#define bsp_hpget bsplibstd_hpget
#define bsp_set_tagsize  bsplibstd_set_tagsize
#define bsp_send  bsplibstd_send
#define bsp_hpsend bsplibstd_hpsend
#define bsp_qsize bsplibstd_qsize
#define bsp_get_tag bsplibstd_get_tag
#define bsp_move  bsplibstd_move
#define bsp_hpmove bsplibstd_hpmove
#endif


#endif


    /**
     * \defgroup LPFLIB_SPMD SPMD Framework
     * BSPlib adopts the Single Program Multiple Data (SPMD) programming model.
     *
     * There are basically two flavours of packaging your parallel program.
     * In case your entire program is SPMD, there is a simple way:
     *
     * \code
     *  #include <bsp.h>
     *
     *  int main(int argc, char ** argv)
     *  {
     *      bsp_begin( bsp_nprocs() );
     *
     *      // doing the parallel code here
     *
     *      bsp_end();
     *      return 0;
     *  }
     * \endcode
     *
     * If some initialisation or postprocessing by exactly one process is
     * required, a slightly more elaborate start-up is required:
     *
     * \code
     *  #include <bsp.h>
     *
     *  void spmd(void)
     *  {
     *      bsp_begin( bsp_nprocs() );
     *
     *      // doing the parallel code here
     *
     *      bsp_end();
     *  }
     *
     *  int main(int argc, char ** argv)
     *  {
     *      bsp_init( &spmd, argc, argv);
     *
     *      // doing some sequential initialisation here....
     *
     *      spmd();
     *
     *      // doing some sequential postprocessing here...
     *
     *      return 0;
     *  }
     * \endcode
     *
     * In the latter fashion, process 0 in the SPMD section is the continuation
     * of the original process initiating the program.
     *
     * @{
     */

    /**
     * Explicitly initializes the BSPlib runtime. This function must be
     * called if some of the program is not supposed to run in parallel, i.e.
     * the first statement in your program is not bsp_begin() or the last
     * statement before return or exit is not bsp_end().  That might happen,
     * for example, if some initialization or postprocessing work has to be
     * performed sequentially at the start or end of the program, respectivly.
     * In this case the parallel part of the program must be in a separate
     * function, whose pointer has to be provided through the \a spmd_part
     * parameter.
     *
     * \param[in] spmd_part
     *      The pointer to the function with the parallel part of the program.
     *      The first statement of that function must be bsp_begin(). The last
     *      the statement must be bsp_end().
     *
     * \param[in] argc
     *      Number of command line parameters \c argc as provided by \c main
     *
     * \param[in] argv
     *      Array of pointers \c argv to the command line parameters as
     *      provided by \c main
     *
     * \exception bsp_abort
     *      When parameter \a spmd_part is \c NULL
     *
     * \exception bsp_abort
     *      When bsp_init() called more than once.
     *
     * \see bsp_begin
     * \see bsp_end
     *
     */
#ifdef DOXYGEN
    void bsp_init( void ( *spmd_part ) ( void ), int argc, char *argv[] );
#else
    extern BSPLIB_API
    void bsplibstd_init( void ( *spmd_part ) ( void ), int argc, char *argv[] );
#endif

    /**
     * Start the parallel section of the program with at most \a maxprocs BSP
     * processes. If some sequential initialization of preprocessing is
     * required, a call to bsp_init() first is mandatory.
     *
     * \param[in] maxprocs
     *      Number of requested processes. It is not guaranteed that this
     *      amount will be available. The actual number is returned by
     *      bsp_nprocs() after the call to bsp_begin().
     *
     * \exception bsp_abort
     *      When bsp_begin() is called for a second time without an intermediate
     *      call to bsp_init first.
     *
     * \exception bsp_abort
     *      When \a maxprocs is less than 1.
     *
     * \exception bsp_abort
     *      When it runs out of memory.
     *
     * \see bsp_init
     * \see bsp_end
     * \see bsp_nprocs
     */
#ifdef DOXYGEN
    void bsp_begin( int maxprocs );
#else

    #define bsp_begin( maxprocs ) \
         do { if ( bsplibstd_spawn( (unsigned int) maxprocs )) goto __bsplibstd_end_label ; } while(0)

    extern BSPLIB_API
    int bsplibstd_spawn( unsigned int maxprocs );
#endif

    /**
     * End the parallel section of the program. If some sequential
     * postprocessing is required a call to bsp_init() is mandatory.
     *
     * \exception bsp_abort
     *      When bsp_begin() hasn't been called yet
     *
     * \exception bsp_abort
     *      When bsp_end() is called for a second time
     *
     * \exception bsp_abort
     *      When another process called bsp_sync() instead
     *
     * \exception bsp_abort
     *      When communication subsystem fails.
     *
     * \see bsp_init
     * \see bsp_begin
     * \see bsp_nprocs
     */
#ifdef DOXYGEN
    void bsp_end( void );
#else

   #define bsp_end()  bsplibstd_end();  __bsplibstd_end_label:

    extern BSPLIB_API
    void bsplibstd_end( void );
#endif


    /**
     * Abnormally terminate execution of all BSP processes with the given
     * message.  The \a format and subsequent parameters are exactly the same
     * as \c printf.
     *
     * \param[in] format
     *      Format string which is exactly as \c printf
     *
     * \exception bsp_abort
     *      Always
     */
#ifdef DOXYGEN
    void bsp_abort( const char *format, ... );
#else
    extern BSPLIB_API
    void bsplibstd_abort( const char * format, ...);
#endif

    /**
     * Return the number of available parallel processes. When called before
     * bsp_begin(), it returns an upper bound of how many be acquired by a
     * bsp_begin(). When called after bsp_begin(), it returns how many
     * processes are actually used.
     *
     * \return The number of available parallel processes.
     *
     * \exception bsp_abort
     *      When it is called outside an SPMD section and it failed to
     *      query machine parameters
     */
#ifdef DOXYGEN
    int bsp_nprocs( void );
#else
    extern BSPLIB_API
    int bsplibstd_nprocs( void );

    extern BSPLIB_API
    unsigned int mcbsp_nprocs( void );
#endif

    /**
     * Return the ID of the process. The processes are uniquely numbered from 0
     * to P-1 where P is the number of processes in the parallel section.
     *
     * \return The ID of the process.
     *
     * \exception bsp_abort
     *      When not between bsp_begin() and bsp_end()
     *
     * \invariant bsp_pid() < bsp_nprocs()
     * \invariant bsp_pid() >= 0
     */
#ifdef DOXYGEN
    int bsp_pid( void );
#else
    extern BSPLIB_API
    int bsplibstd_pid( void );

    extern BSPLIB_API
    unsigned int mcbsp_pid( void );
#endif

    /**
     * Return the elapsed wall clock time in seconds since bsp_begin().
     *
     * \return The elapsed wall clock time in seconds since bsp_begin()
     *
     * \exception bsp_abort
     *      When not between bsp_begin() and bsp_end()
     *
     * \remark Clocks will run roughly at the same pace on each process, but it
     * is not a global clock. It can and will happen that for brief moments
     * speeds differ. Over longer periods of time clock drift can be
     * substantial.
     *
     */
#ifdef DOXYGEN
    double bsp_time( void );
#else
    extern BSPLIB_API
    double bsplibstd_time( void );
#endif

    /**
     * Separates two supersteps with a barrier synchronization. There is no
     * notion of subset synchronization, so all processes must participate.
     * Doing otherwise will result in undefined behaviour and a runtime error
     * when one of the processes exits. Communication that was initiated in the
     * previous superstep is guaranteed to have finished after a call to this
     * function.
     *
     * \exception bsp_abort
     *      When not between bsp_begin() and bsp_end()
     *
     * \exception bsp_abort
     *      When another process called bsp_end()
     *
     * \exception bsp_abort
     *      When not all processes deregistered the same registration slot with
     *      bsp_pop_reg()
     *
     * \exception bsp_abort
     *      When not all processes registered the same number of memory regions
     *      with bsp_push_reg()
     *
     * \exception bsp_abort
     *      When not all processes set the same message tag size with
     *      bsp_set_tagsize()
     *
     * \exception bsp_abort
     *      When it runs out of memory.
     *
     * \exception bsp_abort
     *      When the communication subsystem fails.
     */
#ifdef DOXYGEN
    void bsp_sync( void );
#else
    extern BSPLIB_API
    void bsplibstd_sync( void );
#endif

    /**
     * @}
     */

    /**
     * \defgroup LPFLIB_SPMD_IO I/O in the SPMD Framework
     * BSPlib's SPMD framework sets specific rules on I/O. It is guaranteed to
     * always work on process 0. Other processes can only write to standard
     * output and standard error. This output is multiplexed in a
     * non-deterministic fashion.
     *
     * \code
     *  #include <bsp.h>
     *
     *  int main(int argc, char ** argv)
     *  {
     *      int pid, number;
     *      bsp_begin( bsp_nprocs() );
     *
     *      pid = bsp_pid();
     *      if ( 0 == pid )
     *      {
     *          printf("Hi, type a number:\n");
     *          scanf("%d", & number);
     *      }
     *      bsp_sync();
     *      if ( 0 == pid )
     *      {
     *          printf("Hi, I am root and I have your number %d\n", number);
     *      }
     *      else
     *      {
     *          printf( "Hi, I from process %d\n", pid );
     *      }
     *
     *      bsp_end();
     *      return 0;
     *  }
     * \endcode
     */

    /**
     * \defgroup LPFLIB_DRMA Direct Remote Memory Access
     * As a means to communicate between processes BSPlib defines a set of
     * functions to write in or read from the memory of another process.
     *
     * The basic flow of statements is as follows:
     *
     *   -# Register memory with bsp_push_reg() and bsp_sync()
     *   -# Do the communication with bsp_put(), bsp_hpput(), bsp_get(), or
     *      bsp_hpget()
     *   -# Wait for the communication to finish with a bsp_sync()
     *   -# Free resources by deregistering the memory with bsp_pop_reg()
     *
     * For example: a program that computes the maximum return value of a
     * function that is called by all processes.
     * \code
     *  #include <bsp.h>
     *  #include <stdlib.h>
     *  #include <stdio.h>
     *
     *  // A function that return a number depending on which process this is
     *  int f( void )
     *  {
     *      return 5 * bsp_pid();
     *  }
     *
     *
     *  int main( int argc, char ** argv)
     *  {
     *      int a, maxA;
     *      int *b;
     *      bsp_pid_t s, root, p;
     *      bsp_nprocs_t P;
     *
     *      // start the SPMD program
     *      bsp_begin( bsp_nprocs(  ) );
     *
     *      // store some short-hands
     *      root = 0;
     *      P = bsp_nprocs(  );
     *      s = bsp_pid(  );
     *
     *      // prepare an array to hold data
     *      b = ( int * ) malloc( sizeof( int ) * P );
     *
     *      // register this array globally, so that anyone can reach it
     *      bsp_push_reg( b, sizeof( int ) * P );
     *      bsp_sync(  );
     *
     *      // compute a value that depends somehow on this process
     *      a = f(  );
     *
     *      // Gather from all processes the value of 'a' on process 0
     *      bsp_put( root, &a, b, sizeof( int ) * s, sizeof( int ) );
     *      bsp_sync(  );
     *
     *      // Let process 0 determine the maximum
     *      if ( root == s )
     *      {
     *          maxA = -1;
     *          for ( p = 0; p < P; ++p )
     *              if ( maxA < b[p] ) maxA = b[p];
     *          printf( "The maximum is %d\n", maxA );
     *      }
     *
     *      // free resources
     *      bsp_pop_reg( b );
     *      free( b );
     *
     *      // terminate the program
     *      bsp_end(  );
     *      return 0;
     *  }
     *
     *
     * \endcode
     *
     * @{
     */

    /**
     * Register a memory region to be used with bsp_put(), bsp_hpput(),
     * bsp_get(), and bsp_hpget(). When a memory region is to be registered,
     * all processes must call this function in the same superstep with the
     * pointer \a ident to the memory region and its \a size. After the next
     * bsp_sync() this will create a new registration slot for this memory
     * region, which each process can identify by the pointer \a ident it
     * supplied.
     *
     * \remark Memory regions may be registered multiple times with the same
     * pointer \a ident. When referenced the most recent registration slot
     * takes precedence.
     *
     * \remark If a process wishes to register a memory region with zero \a
     * size, which may be the case if it doesn't have to answer to remote puts
     * and gets, it must still provide a (locally) unique pointer \a ident.
     *
     * \remark If a process is not involved in the communication at all, it may
     * register \c NULL with a zero memory \a size.
     *
     * \param[in] ident
     *      Pointer to the memory region and identifier to the memory slot.
     *
     * \param[in] size
     *      Size of the memory region in bytes.
     *
     * \exception bsp_abort
     *      When not between bsp_begin() and bsp_end()
     *
     * \exception bsp_abort
     *      When the \a size is negative
     *
     * \exception bsp_abort
     *      When \c NULL is registered with a non-zero \a size
     *
     * \see bsp_pop_reg
     */
#ifdef DOXYGEN
    void bsp_push_reg( const void *ident, int size );
#else
    extern BSPLIB_API
    void bsplibstd_push_reg( const void *ident, int size );

    extern BSPLIB_API
    void mcbsp_push_reg( const void *ident, size_t size );
#endif

    /**
     * Deregister a memory region. When a memory region is to be deregistered,
     * all processes must call this function in the same superstep with the
     * pointer \a ident identifying the targeted registration slot, which is
     * the same pointer that was used to register the memory region before. If
     * two or more registration slots match the pointer, the most recent
     * registration slot will be removed.
     *
     * \param[in] ident
     *      Pointer identifying the memory region.
     *
     * \exception bsp_abort
     *      When not between bsp_begin() and bsp_end()
     *
     * \exception bsp_abort
     *      When no memory registration slot can be identified by \a ident
     *
     * \see bsp_push_reg
     */
#ifdef DOXYGEN
    void bsp_pop_reg( const void *ident );
#else
    extern BSPLIB_API
    void bsplibstd_pop_reg( const void * ident );
#endif

    /**
     * Copy local data into remote memory region where it will appear after the
     * next bsp_sync(). The source data is copied to a buffer before the
     * function returns, and the data is actually written in the destination
     * memory at the next bsp_sync().
     *
     * \param[in] pid
     *      The process ID of the remote process
     *
     * \param[in] src
     *      A pointer to the local source data
     *
     * \param[out] dst
     *      A pointer identifying the registration slot of the remote memory
     *      region.
     *
     * \param[in] offset
     *      A byte offset into the destination remote memory
     *
     * \param[in] nbytes
     *      Number of bytes to copied.
     *
     * \exception bsp_abort
     *      When not between bsp_begin() and bsp_end().
     *
     * \exception bsp_abort
     *      When \a dst is \c NULL.
     *
     * \exception bsp_abort
     *      When either \a offset or \a size is negative.
     *
     * \exception bsp_abort
     *      When the registration slot of the remote memory region cannot be
     *      identified by \a dst.
     *
     * \exception bsp_abort
     *      When the write would be beyond the extent of the registered memory
     *      region.
     *
     * \see bsp_hpput
     * \see bsp_push_reg
     */
#ifdef DOXYGEN
    void bsp_put( int pid, const void *src, void *dst,
        int offset, int nbytes );
#else
    extern BSPLIB_API
    void bsplibstd_put( int pid, const void *src, void *dst,
        int offset, int nbytes );

    extern BSPLIB_API
    void mcbsp_put( unsigned int pid, const void *src, void *dst,
        size_t offset, size_t nbytes );
#endif

    /**
     * Copy local data into remote memory region where it is guaranteed to be
     * written after the next bsp_sync. The data may be copied at any time
     * between the function call and the next bsp_sync(). It is therefore
     * important not to touch the source and destination memory areas, as the
     * result of that will be undefined.
     *
     * \param[in] pid
     *      The process ID of the remote process
     *
     * \param[in] src
     *      A pointer to the local source data
     *
     * \param[out] dst
     *      A pointer identifying the registration slot of the remote memory
     *      region.
     *
     * \param[in] offset
     *      A byte offset into the destination remote memory
     *
     * \param[in] nbytes
     *      Number of bytes to copied.
     *
     * \exception bsp_abort
     *      When not between bsp_begin() and bsp_end().
     *
     * \exception bsp_abort
     *      When \a dst is \c NULL.
     *
     * \exception bsp_abort
     *      When either \a offset or \a size is negative.
     *
     * \exception bsp_abort
     *      When the registration slot of the remote memory region cannot be
     *      identified by \a dst.
     *
     * \exception bsp_abort
     *      When the operation would write beyond the extent of the registered
     *      memory region.
     *
     * \see bsp_put
     * \see bsp_push_reg
     */
#ifdef DOXYGEN
    void bsp_hpput( int pid, const void *src, void *dst,
        int offset, int nbytes );
#else
    extern BSPLIB_API
    void bsplibstd_hpput( int pid, const void *src, void *dst,
        int offset, int nbytes );

    extern BSPLIB_API
    void mcbsp_hpput( unsigned int pid, const void *src, void *dst,
        size_t offset, size_t nbytes );
#endif

    /**
     * Copy data from remote memory region into a local memory area where it
     * will appear after the next bsp_sync(). The remote data is read and
     * written to the local memory area at the next bsp_sync().
     *
     * \param[in] pid
     *      The process ID of the remote process
     *
     * \param[in] src
     *      A pointer identifying the registration slot of the remote memory
     *      region.
     *
     * \param[in] offset
     *      A byte offset into the source remote memory
     *
     * \param[out] dst
     *      A pointer to the local memory area.
     *
     * \param[in] nbytes
     *      Number of bytes to copied.
     *
     * \exception bsp_abort
     *      When not between bsp_begin() and bsp_end().
     *
     * \exception bsp_abort
     *      When \a src is \c NULL.
     *
     * \exception bsp_abort
     *      When either \a offset or \a size is negative.
     *
     * \exception bsp_abort
     *      When the registration slot of the remote memory region cannot be
     *      identified by \a src .
     *
     * \exception bsp_abort
     *      When the write would beyond the extent of the registered memory
     *      region.
     *
     * \see bsp_hpget
     * \see bsp_push_reg
     */
#ifdef DOXYGEN
    void bsp_get( int pid, const void *src, int offset,
        void *dst, int nbytes );
#else
    extern BSPLIB_API
    void bsplibstd_get( int pid, const void *src, int offset,
        void *dst, int nbytes );

    extern BSPLIB_API
    void mcbsp_get( unsigned int pid, const void *src, size_t offset,
        void *dst, size_t nbytes );
#endif

    /**
     * Copy data from remote memory region into a local memory area where it is
     * guaranteed to be written after the next bsp_sync(). The data is not
     * buffered, which means that at any time in that particular super-step the
     * remote data can be read, and that the retrieved data can be written to
     * the local memory area at any time between the call to bsp_hpget() and
     * the end of the call to the next bsp_sync().
     *
     * \param[in] pid
     *      The process ID of the remote process
     *
     * \param[in] src
     *      A pointer identifying the registration slot of the remote memory
     *      region.
     *
     * \param[in] offset
     *      A byte offset into the source remote memory
     *
     * \param[out] dst
     *      A pointer to the local memory area.
     *
     * \param[in] nbytes
     *      Number of bytes to copied.
     *
     * \exception bsp_abort
     *      When not between bsp_begin() and bsp_end().
     *
     * \exception bsp_abort
     *      When \a src is NULL.
     *
     * \exception bsp_abort
     *      When either \a offset or \a size is negative.
     *
     * \exception bsp_abort
     *      When the registration slot of the remote memory region cannot be
     *      identified by \a src .
     *
     * \exception bsp_abort
     *      When the write would beyond the extent of the registered memory
     *      region.
     *
     * \see bsp_get
     * \see bsp_push_reg
     */
#ifdef DOXYGEN
    void bsp_hpget( int pid, const void *src, int offset,
        void *dst, int nbytes );
#else
    extern BSPLIB_API
    void bsplibstd_hpget( int pid, const void *src, int offset,
        void *dst, int nbytes );

    extern BSPLIB_API
    void mcbsp_hpget( unsigned int pid, const void *src, size_t offset,
        void *dst, size_t nbytes );
#endif

    /**
     * @}
     */

    /**
     * \defgroup LPFLIB_BSMP Bulk Synchronous Message Passing
     * For irregular communication patterns, where processes don't know
     * beforehand how much data they will receive from other processes, BSPlib
     * provides also a way to communicate with messages.
     *
     * As an example, below a small program where each process compiles a
     * message, sends it to process 0, who then prints it to standard output.
     * Note that process 0 doesn't need to know the length of the messages to
     * process them.
     * \code
     *  #include <bsp.h>
     *  #include <stdlib.h>
     *  #include <stdio.h>
     *  #include <string.h>
     *
     *  int main( int argc, char ** argv)
     *  {
     *      bsp_pid_t s, root, p;
     *      bsp_nprocs_t P;
     *      char buffer[80];
     *      bsp_size_t payloadSize;
     *      const void * payload;
     *      const void * tag;
     *
     *      // start the SPMD program
     *      bsp_begin( bsp_nprocs() );
     *
     *      // store some short-hands
     *      root = 0;
     *      P = bsp_nprocs();
     *      s = bsp_pid();
     *
     *      // prepare a message
     *      memset(buffer, 0, sizeof(buffer));
     *      payloadSize =
     *         snprintf(buffer, sizeof(buffer), "Hi, this is process %d\n", s);
     *      bsp_send(0, NULL, buffer, payloadSize);
     *      bsp_sync();
     *
     *      // Let the root process the messages
     *      if (root == s)
     *      {
     *          payloadSize = bsp_hpmove( &payload, &tag );
     *          while ( bsp_size_unavailable != payloadSize )
     *          {
     *              int size = payloadSize;
     *              printf( "Received message: %.*s\n", size, payload );
     *              payloadSize = bsp_hpmove( &payload, &tag );
     *          }
     *      }
     *
     *      // terminate the program
     *      bsp_end();
     *      return 0;
     *  }
     *  \endcode
     *
     *
     * @{
     */

    /**
     * Set the tag size to be used by bsp_send() after the next bsp_sync(). All
     * processes must call this function with the same value. The value of the
     * parameter is overwritten with the current valid tag size. If this
     * function is called multiple times in the same super-step, the value of
     * the last call will be definitive.
     *
     * \param[in,out] tag_nbytes
     *      As input the size of tags as number of bytes after the next
     *      bsp_sync(). As output the tag size of the current superstep.
     *
     * \exception bsp_abort
     *      When not between bsp_begin() and bsp_end().
     *
     * \exception bsp_abort
     *      When \a tag_nbytes is negative
     *
     */
#ifdef DOXYGEN
    void bsp_set_tagsize( int * tag_nbytes );
#else
    extern BSPLIB_API
    void bsplibstd_set_tagsize( int * tag_nbytes );

    extern BSPLIB_API
    void mcbsp_set_tagsize( size_t * tag_nbytes );
#endif


    /**
     * Send a message to a remote process which is guaranteed to have arrived
     * after the next bsp_sync(). The \a payload and \a tag are copied before
     * this function returns.
     *
     * \param[in] pid
     *      The ID of the destination process.
     *
     * \param[in] tag
     *      A pointer to the tag to be send with the message. The tag size can
     *      be changed with a call to bsp_set_tagsize() in the preceding
     *      super-step, otherwise the tag size will remain the same. By default
     *      the tag size is 0.
     *
     * \param[in] payload
     *      A pointer to the payload to be send with the message.
     *
     * \param[in] payload_nbytes
     *      The size of the payload in bytes.
     *
     * \exception bsp_abort
     *      When not between bsp_begin() and bsp_end().
     *
     * \exception bsp_abort
     *      When \a payload_nbytes is negative
     *
     */
#ifdef DOXYGEN
    void bsp_send( int pid, const void *tag, const void *payload,
        int payload_nbytes );
#else
    extern BSPLIB_API
    void bsplibstd_send( int pid, const void *tag, const void *payload,
        int payload_nbytes );

    extern BSPLIB_API
    void mcbsp_send( unsigned int pid, const void *tag, const void *payload,
        size_t payload_nbytes );
#endif


    /**
     * Send a message to a remote process which is guaranteed to have arrived
     * after the next bsp_sync. The \a payload and \a tag are copied somewhere
     * between the call of this function and the next bsp_sync().
     *
     * \param[in] pid
     *      The ID of the destination process.
     *
     * \param[in] tag
     *      A pointer to the tag to be send with the message. The tag size can
     *      be changed with a call to bsp_set_tagsize() in the preceding
     *      super-step, otherwise the tag size will remain the same. By default
     *      the tag size is 0.
     *
     * \param[in] payload
     *      A pointer to the payload to be send with the message.
     *
     * \param[in] payload_nbytes
     *      The size of the payload in bytes.
     *
     * \exception bsp_abort
     *      When not between bsp_begin() and bsp_end().
     *
     * \exception bsp_abort
     *      When \a payload_nbytes is negative
     *
     */
#ifdef DOXYGEN
     void bsp_hpsend( int pid, const void *tag, const void *payload,
        int payload_nbytes );
#else
     extern BSPLIB_API
     void bsplibstd_hpsend( int pid, const void *tag, const void *payload,
        int payload_nbytes );

     extern BSPLIB_API
     void mcbsp_hpsend( unsigned int pid, const void *tag, const void *payload,
        size_t payload_nbytes );
#endif

    /**
     * Query the size of the message queue which have arrived at this process at
     * the previous bsp_sync().
     *
     * \param[out] nmessages
     *      The number of messages in the queue
     *
     * \param[out] accum_nbytes
     *      The total number of bytes in the payloads of messages in the queue.
     *
     * \exception bsp_abort
     *      When not between bsp_begin() and bsp_end().
     *
     */
#ifdef DOXYGEN
    void bsp_qsize( int * nmessages, int * accum_nbytes );
#else
    extern BSPLIB_API
    void bsplibstd_qsize( int * nmessages, int * accum_nbytes );

    extern BSPLIB_API
    void mcbsp_qsize( unsigned int * nmessages, size_t * accum_nbytes );
#endif


    /**
     * Get the tag size and the size of the payload of the first message in the
     * message queue. The message queue consists of the messages that have
     * arrived at this process at the previous bsp_sync() and haven't been
     * dequeued yet. If the queue is empty, \a status is set to \c -1
     *
     * \param[out] status
     *      If the queue is non-empty the size of the payload of the first
     *      message in the queue. If the queue is empty, \a status is set to -1.
     *
     * \param[out] tag
     *      A pointer to the memory area where this function can copy the
     *      message tag to. The size of this tag is the same as when the
     *      message was sent in the previous superstep.
     *
     * \exception bsp_abort
     *      When not between bsp_begin() and bsp_end().
     *
     */
#ifdef DOXYGEN
    void bsp_get_tag( int * status, void *tag );
#else
    extern BSPLIB_API
    void bsplibstd_get_tag( int * status, void *tag );

    extern BSPLIB_API
    void mcbsp_get_tag( size_t * status, void *tag );
#endif

    /**
     * Dequeue the first message in the message queue and copy the payload to
     * the specified place. The message queue consists of the messages that
     * have arrived at this process at the previous bsp_sync() and haven't been
     * dequeued yet.
     *
     * \param[out] payload
     *      A pointer to the memory where the payload should be copied to.
     *
     * \param[in] reception_bytes
     *      The maximum amount of bytes to copy from the payload in to the
     *      memory designated by \a payload. This means that if the payload was
     *      actually larger, the payload will be truncated.
     *
     * \exception bsp_abort
     *      When not between bsp_begin() and bsp_end().
     *
     * \exception bsp_abort
     *      When \a reception_bytes is negative
     *
     * \exception bsp_abort
     *      When the message queue is empty
     *
     */
#ifdef DOXYGEN
    void bsp_move( void *payload, int reception_bytes );
#else
    extern BSPLIB_API
    void bsplibstd_move( void *payload, int reception_bytes );

    extern BSPLIB_API
    void mcbsp_move( void *payload, size_t reception_bytes );
#endif


    /**
     * Dequeue the first message in the message queue and provide a pointer the
     * payload and tag. The message queue consists of the messages that have
     * arrived at this process at the previous bsp_sync() and haven't been
     * dequeued yet.
     *
     * \param[out] tag_ptr
     *      The pointer where to copy the tag from. Note that the tag size is
     *      the same when it was sent in the previous superstep.
     *
     * \param[out] payload_ptr
     *      The pointer where to copy the payload from.
     *
     * \return The payload size, or \c -1 when the queue is empty.
     *
     * \exception bsp_abort
     *      When not between bsp_begin() and bsp_end().
     *
     */
#ifdef DOXYGEN
    int bsp_hpmove( const void **tag_ptr, const void **payload_ptr );
#else
    extern BSPLIB_API
    int bsplibstd_hpmove( const void **tag_ptr, const void **payload_ptr );

    extern BSPLIB_API
    size_t mcbsp_hpmove( void **tag_ptr, void **payload_ptr );
#endif

    /**
     * @}
     */


#  ifdef __cplusplus
}
#  endif

/**
 * @}
 */

#endif
