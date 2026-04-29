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
set(EOPPD_BPF_SOURCE ${EOPPD_SOURCE_DIR}/xdp_drop/xdp_drop.bpf.c)
set(EOPPD_BPF_OBJECT ${EOPPD_BUILD_DIR}/xdp_drop.bpf.o)
set(EOPPD_VMLINUX_HEADER ${EOPPD_SOURCE_DIR}/../eBPF_basic_design/vmlinux.h)
set(EOPPD_VMLINUX_BTF /sys/kernel/btf/vmlinux)

add_custom_command(
    OUTPUT ${EOPPD_VMLINUX_HEADER}
    COMMAND ${CMAKE_COMMAND} -E echo "Generating CO-RE header: ${EOPPD_VMLINUX_HEADER}"
    COMMAND ${CMAKE_COMMAND} -E sh -c "${BPFOOL_EXECUTABLE} btf dump file '${EOPPD_VMLINUX_BTF}' format c > '${EOPPD_VMLINUX_HEADER}'"
    DEPENDS ${EOPPD_VMLINUX_BTF}
    COMMENT "Generating CO-RE header"
)

add_custom_command(
    OUTPUT ${EOPPD_BPF_OBJECT}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EOPPD_BUILD_DIR}
    COMMAND ${CLANG_EXECUTABLE} -I${EOPPD_SOURCE_DIR} -I${LIBBPF_INCLUDE_DIR} -I${EOPPD_SOURCE_DIR}/../eBPF_basic_design -O2 -g -target bpf -c ${EOPPD_BPF_SOURCE} -o ${EOPPD_BPF_OBJECT}
    DEPENDS ${EOPPD_BPF_SOURCE} ${EOPPD_VMLINUX_HEADER}
    COMMENT "Compiling XDP BPF object"
)

add_custom_target(xdp_drop_bpf_object DEPENDS ${EOPPD_BPF_OBJECT})

include_directories(
    ${EOPPD_SOURCE_DIR}/coroutine
    ${EOPPD_SOURCE_DIR}/bpf
    ${EOPPD_SOURCE_DIR}/xdp_drop
    ${EOPPD_SOURCE_DIR}/action
    ${EOPPD_SOURCE_DIR}/log_action
    ${LIBBPF_INCLUDE_DIR}
    ${EOPPD_SOURCE_DIR}/../eBPF_basic_design
)

add_executable(xdp_drop_loader
    xdp_drop/main.cpp
    xdp_drop/xdp_drop.cpp
    bpf/BpfProgram.cpp
    action/ActionLoop.cpp
    log_action/LogAction.cpp
)

target_link_libraries(xdp_drop_loader PRIVATE ${LIBBPF_LIBRARY} ${LIBELF_LIBRARY} ${LIBZ_LIBRARY} Threads::Threads)
add_dependencies(xdp_drop_loader xdp_drop_bpf_object)

install(FILES ${EOPPD_BPF_OBJECT} DESTINATION /opt/ebpf_oop_design)
install(TARGETS xdp_drop_loader DESTINATION /opt/ebpf_oop_design)
install(FILES ${EOPPD_SOURCE_DIR}/xdp_drop/xdp_drop.bpf.c DESTINATION /opt/ebpf_oop_design/xdp_drop)

message(STATUS "ebpf_oop_design configured")
