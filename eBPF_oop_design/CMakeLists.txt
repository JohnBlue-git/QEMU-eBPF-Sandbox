cmake_minimum_required(VERSION 3.20)
project(ebpf_oop_design LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_program(CLANG_EXECUTABLE clang REQUIRED)
find_program(BPFOOL_EXECUTABLE bpftool REQUIRED)

find_path(LIBBPF_INCLUDE_DIR bpf/libbpf.h
    HINTS /usr/include /usr/local/include /usr/include/bpf /usr/local/include/bpf)
if(NOT LIBBPF_INCLUDE_DIR)
    message(FATAL_ERROR "libbpf headers not found. Install libbpf-dev or set CMAKE_INCLUDE_PATH.")
endif()

find_library(LIBBPF_LIBRARY bpf HINTS /usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64 REQUIRED)
find_library(LIBELF_LIBRARY elf HINTS /usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64 REQUIRED)
find_library(LIBZ_LIBRARY z HINTS /usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64 REQUIRED)
find_package(Threads REQUIRED)

set(EOPPD_SOURCE_DIR ${CMAKE_SOURCE_DIR})
set(EOPPD_BUILD_DIR ${CMAKE_BINARY_DIR})
set(EOPPD_BPF_SOURCES
    ${EOPPD_SOURCE_DIR}/xdp_drop/xdp_drop.bpf.c
    ${EOPPD_SOURCE_DIR}/syscall_trace/syscall_trace.bpf.c
    ${EOPPD_SOURCE_DIR}/socket_filter/socket_filter.bpf.c
    ${EOPPD_SOURCE_DIR}/cgroup_egress/cgroup_egress.bpf.c
)
set(EOPPD_BPF_OBJECTS
    xdp_drop.bpf.o
    syscall_trace.bpf.o
    socket_filter.bpf.o
    cgroup_egress.bpf.o
)
set(EOPPD_VMLINUX_HEADER vmlinux.h)
set(EOPPD_VMLINUX_BTF /sys/kernel/btf/vmlinux)

add_custom_command(
    OUTPUT ${EOPPD_VMLINUX_HEADER}
    COMMAND sh -c "test -r '${EOPPD_VMLINUX_BTF}'"
    COMMAND sh -c "command -v ${BPFOOL_EXECUTABLE}"
    COMMAND sh -c "echo 'Generating CO-RE header: ${EOPPD_VMLINUX_HEADER}'"
    COMMAND sh -c "rm -f ${EOPPD_VMLINUX_HEADER}"
    COMMAND sh -c "${BPFOOL_EXECUTABLE} btf dump file '${EOPPD_VMLINUX_BTF}' format c >${EOPPD_VMLINUX_HEADER}"
    WORKING_DIRECTORY ${EOPPD_BUILD_DIR}
    DEPENDS ${EOPPD_VMLINUX_BTF}
    COMMENT "Generating CO-RE header"
)


# Macro to generate bpf object build rules for each example
foreach(example xdp_drop syscall_trace socket_filter cgroup_egress)
    set(BPF_SRC ${EOPPD_SOURCE_DIR}/${example}/${example}.bpf.c)
    set(BPF_OBJ ${example}.bpf.o)
    add_custom_command(
        OUTPUT ${BPF_OBJ}
        WORKING_DIRECTORY ${EOPPD_BUILD_DIR}
        COMMAND ${CLANG_EXECUTABLE} -I${EOPPD_SOURCE_DIR} -I${LIBBPF_INCLUDE_DIR} -I${EOPPD_BUILD_DIR} -O2 -g -target bpf -c ${BPF_SRC} -o ${BPF_OBJ}
        DEPENDS ${BPF_SRC} ${EOPPD_VMLINUX_HEADER}
        COMMENT "Compiling ${example} BPF object"
    )
    list(APPEND BPF_OBJECTS ${BPF_OBJ})
endforeach()
add_custom_target(all_bpf_objects DEPENDS ${BPF_OBJECTS})

include_directories(
    ${EOPPD_SOURCE_DIR}/coroutine
    ${EOPPD_SOURCE_DIR}/ebpf
    ${EOPPD_SOURCE_DIR}/xdp_drop
    ${EOPPD_SOURCE_DIR}/action
    ${EOPPD_SOURCE_DIR}/log_action
    ${LIBBPF_INCLUDE_DIR}
    ${EOPPD_BUILD_DIR}
)


# Macro to generate loader build/install rules for each example
foreach(example xdp_drop syscall_trace socket_filter cgroup_egress)
    add_executable(${example}_loader
        ${example}/main.cpp
        ${example}/${example}.cpp
        ebpf/BpfProgram.cpp
        action/ActionLoop.cpp
        log_action/LogAction.cpp
    )
    target_link_libraries(${example}_loader PRIVATE ${LIBBPF_LIBRARY} ${LIBELF_LIBRARY} ${LIBZ_LIBRARY} Threads::Threads)
    add_dependencies(${example}_loader all_bpf_objects)
    install(FILES ${EOPPD_BUILD_DIR}/${example}.bpf.o DESTINATION /opt/ebpf_oop_design)
    install(TARGETS ${example}_loader DESTINATION /opt/ebpf_oop_design)
    install(FILES ${EOPPD_SOURCE_DIR}/${example}/${example}.bpf.c DESTINATION /opt/ebpf_oop_design/${example})
endforeach()

message(STATUS "ebpf_oop_design configured")
