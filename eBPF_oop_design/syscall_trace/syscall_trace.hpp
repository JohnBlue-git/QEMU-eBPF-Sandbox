#ifndef EBPF_OOP_DESIGN_SYSCALL_TRACE_HPP
#define EBPF_OOP_DESIGN_SYSCALL_TRACE_HPP

#include <string>
#include <bpf/libbpf.h>

#include "../ebpf/BpfProgram.hpp"

class SyscallTraceProgram final : public BpfProgram {
public:
    SyscallTraceProgram(std::string object_path, std::string log_path) noexcept;
    SyscallTraceProgram(SyscallTraceProgram&&) noexcept;
    ~SyscallTraceProgram() noexcept override;

    SyscallTraceProgram& operator=(SyscallTraceProgram&&) noexcept;

private:
    bool attachProgram() noexcept override;
    void detachProgram() noexcept override;

    auto getRingBufferHandler() -> int(*)(void*, void*, size_t) override;
    static int ringBufferHandler(void *ctx, void *data, size_t data_sz);

    std::string log_path_;
    struct bpf_program *prog_openat;
    struct bpf_program *prog_read;
    struct bpf_program *prog_write;
    struct bpf_link *link_openat_;
    struct bpf_link *link_read_;
    struct bpf_link *link_write_;
};
#endif // EBPF_OOP_DESIGN_SYSCALL_TRACE_HPP
