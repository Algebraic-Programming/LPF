
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

option(LPFLIB_MAKE_DOC "Build the documentation" ON)
if (LPFLIB_MAKE_DOC)
    find_package(Doxygen REQUIRED) 
    find_program(PDFLATEX "pdflatex" DOC "Command to run PDFlatex")

    message(STATUS "Documentation will be generated")
    set(pdffile "${CMAKE_CURRENT_BINARY_DIR}/api/latex/refman.pdf")
    if (NOT PDFLATEX)
        message("pdflatex not found. Can't generate PDF!")
    endif()

    # Generate Doxygen config file
    configure_file( lpf_core.cfg.in  lpf_core.cfg  @ONLY)

    # Run Doxygen
    add_custom_target( doc ALL
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/api/html/index.html
                    ${pdffile}         
            COMMENT "Generating API documentation"
            )
    add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/api/html/index.html
                               ${CMAKE_CURRENT_BINARY_DIR}/api/latex/refman.tex
            DEPENDS ${PROJECT_SOURCE_DIR}/include/lpf/core.h
                     ${PROJECT_SOURCE_DIR}/include/lpf/bsmp.h 
                     ${PROJECT_SOURCE_DIR}/include/lpf/mpi.h 
                     ${PROJECT_SOURCE_DIR}/include/lpf/pthread.h 
                     ${PROJECT_SOURCE_DIR}/include/lpf/collectives.h 
                     ${PROJECT_SOURCE_DIR}/include/lpf/bsplib.h 
                     ${PROJECT_SOURCE_DIR}/include/bsp/bsp.h
                     ${PROJECT_SOURCE_DIR}/include/lpf/hybrid.h
                     ${PROJECT_SOURCE_DIR}/include/lpf/mpirpc-client.h
                     ${PROJECT_SOURCE_DIR}/include/lpf/rpc-client.h
                     ${PROJECT_SOURCE_DIR}/doc/lpf_core.cfg.in
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/lpf_core.cfg
            COMMENT "Running Doxygen"
            )
    # Install HTML docs
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/api/html
            DESTINATION ${INSTALL_DOC}
           )

    # Install MAN pages
    # Note: if just the whole man/man3 directory is given instead of an
    # explicit list, then some lesser important man pages will be copied as
    # well and thus polluting the MAN page namespace, as it were.
    set(man_pages
                BSPCOSTS.3
                BSPLIB.3
                BSPLIB_ERR_EMPTY_MESSAGE_QUEUE.3
                BSPLIB_ERR_FATAL.3
                BSPLIB_ERR_MEMORY_ACCESS_OUT_OF_RANGE.3
                BSPLIB_ERR_MEMORY_NOT_REGISTERED.3
                BSPLIB_ERR_NULL_POINTER.3
                BSPLIB_ERR_OUT_OF_MEMORY.3
                BSPLIB_ERR_PID_OUT_OF_RANGE.3
                BSPLIB_ERR_POPREG_MISMATCH.3
                BSPLIB_ERR_PUSHREG_MISMATCH.3
                BSPLIB_ERR_TAGSIZE_MISMATCH.3
                BSPLIB_SUCCESS.3
                LPFLIB_BSMP.3
                LPFLIB_DRMA.3
                LPFLIB_SPMD.3
                LPFLIB_SPMD_IO.3
                LPF_BSMP.3
                LPF_BSPLIB.3
                LPF_COLLECTIVES.3
                LPF_CORE.3
                LPF_ERRS.3
                LPF_ERR_BSMP_FULL.3
                LPF_ERR_BSMP_INVAL.3
                LPF_ERR_FATAL.3
                LPF_ERR_OUT_OF_MEMORY.3
                LPF_ERR_RPC_FAILED.3
                LPF_EXTENSIONS.3
                LPF_HL.3
                LPF_HYBRID.3
                LPF_INIT_NONE.3
                LPF_INVALID_BSMP.3
                LPF_INVALID_COLL.3
                LPF_INVALID_MEMSLOT.3
                LPF_INVALID_MACHINE.3
                LPF_INVALID_SIZE.3
                LPF_MAX_P.3
                LPF_MPI.3
                LPF_MPI_AUTO_INITIALIZE.3
                LPF_MSG_DEFAULT.3
                LPF_NONE.3
                LPF_NO_ARGS.3
                LPF_PTHREAD.3
                LPF_ROOT.3
                LPF_RPC.3
                LPF_RPC_QUERY_NONE.3
                LPF_RPC_SERVER_NONE.3
                LPF_SUCCESS.3
                LPF_SYNC_DEFAULT.3
                _LPF_BMSP_VERSION.3
                _LPF_COLLECTIVES_VERSION.3
                _LPF_EXCLUSIVE_MEMORY.3
                _LPF_INCLUSIVE_MEMORY.3
                _LPF_VERSION.3
                bsmp.h.3
                bsp.h.3
                bsp_abort.3
                bsp_begin.3
                bsp_end.3
                bsp_get.3
                bsp_get_tag.3
                bsp_hpget.3
                bsp_hpmove.3
                bsp_hpput.3
                bsp_hpsend.3
                bsp_init.3
                bsp_move.3
                bsp_nprocs.3
                bsp_pid.3
                bsp_pop_reg.3
                bsp_push_reg.3
                bsp_put.3
                bsp_qsize.3
                bsp_send.3
                bsp_set_tagsize.3
                bsp_sync.3
                bsp_time.3
                bsplib.h.3
                bsplib_create.3
                bsplib_destroy.3
                bsplib_err_t.3
                bsplib_get.3
                bsplib_get_tag.3
                bsplib_hpget.3
                bsplib_hpmove.3
                bsplib_hpput.3
                bsplib_hpsend.3
                bsplib_move.3
                bsplib_nprocs.3
                bsplib_pid.3
                bsplib_pop_reg.3
                bsplib_push_reg.3
                bsplib_put.3
                bsplib_qsize.3
                bsplib_send.3
                bsplib_set_tagsize.3
                bsplib_sync.3
                bsplib_t.3
                bsplib_time.3
                collectives.h.3
                core.h.3
                hybrid.h.3
                lpf_allcombine.3
                lpf_allgather.3
                lpf_allreduce.3
                lpf_alltoall.3
                lpf_args_t.3
                lpf_broadcast.3
                lpf_bsmp_buffer.3
                lpf_bsmp_create.3
                lpf_bsmp_destroy.3
                lpf_bsmp_mode.3
                lpf_bsmp_sync.3
                lpf_bsmp_syncall.3
                lpf_bsmp_t.3
                lpf_coll_t.3
                lpf_collectives_destroy.3
                lpf_collectives_get_context.3
                lpf_collectives_init.3
                lpf_collectives_init_strided.3
                lpf_combine.3
                lpf_combiner_t.3
                lpf_deregister.3
                lpf_err_t.3
                lpf_exec.3
                lpf_func_t.3
                lpf_gather.3
                lpf_get.3
                lpf_hook.3
                lpf_hybrid_finalize.3
                lpf_hybrid_initialize.3
                lpf_init_t.3
                lpf_machine_t.3
                lpf_memslot_t.3
                lpf_move.3
                lpf_mpi_finalize.3
                lpf_mpi_initialize_over_tcp.3
                lpf_mpi_initialize_with_mpicomm.3
                lpf_mpirpc_close_server.3
                lpf_mpirpc_open.3
                lpf_msg_attr_t.3
                lpf_pid_t.3
                lpf_probe.3
                lpf_pthread_finalize.3
                lpf_pthread_initialize.3
                lpf_put.3
                lpf_reduce.3
                lpf_reducer_t.3
                lpf_register_global.3
                lpf_register_local.3
                lpf_rehook.3
                lpf_resize_memory_register.3
                lpf_resize_message_queue.3
                lpf_rpc_query_create.3
                lpf_rpc_query_destroy.3
                lpf_rpc_query_t.3
                lpf_rpc_send_query.3
                lpf_rpc_server_t.3
                lpf_scatter.3
                lpf_send.3
                lpf_spmd_t.3
                lpf_sync.3
                lpf_sync_attr_t.3
                lpf_t.3
                lpos_slot.3
                mpi.h.3
                mpirpc-client.h.3
                pthread.h.3
                rpc-client.h.3)

    foreach(man_page ${man_pages})
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/api/man/man3/${man_page}
                DESTINATION ${INSTALL_MAN}/man/man3/
               )
    endforeach(man_page)

    # Make the PDF
    if (PDFLATEX)
        add_custom_command(OUTPUT ${pdffile}
                COMMAND "make"
                DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/api/latex/refman.tex
                WORKING_DIRECTORY api/latex
        )    
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/api/latex/refman.pdf
                DESTINATION ${INSTALL_DOC}
                RENAME lpf_core.pdf
               )
   endif()
endif()

