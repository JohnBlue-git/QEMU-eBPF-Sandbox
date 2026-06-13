cmake_minimum_required(VERSION 3.20)
project(ebpf_boost_asio_design LANGUAGES C CXX)

include(FetchContent)
include(ExternalProject)

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

# Find thread library in general (POSIX, ...)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
# Find eBpf library
find_library(LIBBPF_LIBRARY bpf HINTS /usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64 REQUIRED)
find_library(LIBELF_LIBRARY elf HINTS /usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64 REQUIRED)
find_library(LIBZ_LIBRARY z HINTS /usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64 REQUIRED)
# Find Boost headers from system packages
set(BOOST_INCLUDE_DIR "")
find_package(Boost 1.74 QUIET)

if(Boost_FOUND)
    set(BOOST_INCLUDE_DIR ${Boost_INCLUDE_DIRS})
    message(STATUS "Using system Boost includes: ${BOOST_INCLUDE_DIR}")
else()
    message(STATUS "Boost headers not found; fetching Boost 1.86 headers via CMake FetchContent")
    FetchContent_Declare(
        boost_headers
        URL https://archives.boost.io/release/1.86.0/source/boost_1_86_0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(boost_headers)
    set(BOOST_INCLUDE_DIR ${boost_headers_SOURCE_DIR})
    message(STATUS "Using fetched Boost headers: ${BOOST_INCLUDE_DIR}")
endif()

set(EOPPD_EBPF_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../eBPF_oop_design")
set(EOPPD_SOURCE_DIR ${CMAKE_SOURCE_DIR})
set(EOPPD_BUILD_DIR ${CMAKE_BINARY_DIR})
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
    COMMENT "Generating CO-RE header"
)


# Macro to generate bpf object build rules for each example
foreach(example xdp_drop syscall_trace socket_filter cgroup_egress)
    set(BPF_SRC ${EOPPD_EBPF_SOURCE_DIR}/${example}/${example}.bpf.c)
    set(BPF_OBJ ${example}.bpf.o)
    add_custom_command(
        OUTPUT ${BPF_OBJ}
        WORKING_DIRECTORY ${EOPPD_BUILD_DIR}
        COMMAND ${CLANG_EXECUTABLE} -I${EOPPD_EBPF_SOURCE_DIR} -I${LIBBPF_INCLUDE_DIR} -I${EOPPD_BUILD_DIR} -O2 -g -target bpf -c ${BPF_SRC} -o ${BPF_OBJ}
        DEPENDS ${BPF_SRC} ${EOPPD_VMLINUX_HEADER}
        COMMENT "Compiling ${example} BPF object"
    )
    list(APPEND BPF_OBJECTS ${BPF_OBJ})
endforeach()
add_custom_target(all_bpf_objects DEPENDS ${BPF_OBJECTS})

include_directories(
    ${EOPPD_SOURCE_DIR}/coroutine
    ${EOPPD_SOURCE_DIR}/action
    ${EOPPD_SOURCE_DIR}/log_action

    ${EOPPD_EBPF_SOURCE_DIR}/ebpf
    ${EOPPD_EBPF_SOURCE_DIR}/xdp_drop
    ${EOPPD_EBPF_SOURCE_DIR}/cgroup_egress
    ${EOPPD_EBPF_SOURCE_DIR}/socket_filter
    ${EOPPD_EBPF_SOURCE_DIR}/syscall_trace

    ${LIBBPF_INCLUDE_DIR}
    ${BOOST_INCLUDE_DIR}
    ${EOPPD_BUILD_DIR}
)


# Macro to generate loader build/install rules for each example
foreach(example xdp_drop syscall_trace socket_filter cgroup_egress)
    add_executable(${example}_loader
        ${EOPPD_EBPF_SOURCE_DIR}/ebpf/BpfProgram.cpp
        ${EOPPD_EBPF_SOURCE_DIR}/${example}/${example}.cpp
        ${EOPPD_SOURCE_DIR}/${example}/main.cpp
        ${EOPPD_SOURCE_DIR}/action/ActionLoop.cpp
        ${EOPPD_SOURCE_DIR}/log_action/LogAction.cpp
    )
    target_compile_definitions(${example}_loader PRIVATE BOOST_ERROR_CODE_HEADER_ONLY)
    target_link_libraries(${example}_loader PRIVATE ${LIBBPF_LIBRARY} ${LIBELF_LIBRARY} ${LIBZ_LIBRARY} Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
    add_dependencies(${example}_loader all_bpf_objects)
    install(FILES ${EOPPD_BUILD_DIR}/${example}.bpf.o DESTINATION /opt/ebpf_boost_asio_design)
    install(TARGETS ${example}_loader DESTINATION /opt/ebpf_boost_asio_design)
    install(FILES ${EOPPD_EBPF_SOURCE_DIR}/${example}/${example}.bpf.c DESTINATION /opt/ebpf_boost_asio_design/${example})
endforeach()

message(STATUS "ebpf_boost_asio_design configured")

# Add a small runner that forces ActionLoop to initialize and start its thread immediately
add_executable(action_runner
    ${EOPPD_SOURCE_DIR}/action/main.cpp
    ${EOPPD_SOURCE_DIR}/action/ActionLoop.cpp
)
target_link_libraries(action_runner PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
install(TARGETS action_runner DESTINATION /opt/ebpf_boost_asio_design)
