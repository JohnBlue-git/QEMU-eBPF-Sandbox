#pragma once

#include <string>
#include <bpf/libbpf.h>

#include "../ebpf/BpfProgram.hpp"

class CgroupEgressProgram final : public BpfProgram {
public:
    CgroupEgressProgram(std::string object_path, std::string cgroup_path, std::string log_path) noexcept;
    CgroupEgressProgram(CgroupEgressProgram&&) noexcept;
    ~CgroupEgressProgram() noexcept override;

    CgroupEgressProgram& operator=(CgroupEgressProgram&&) noexcept;

private:
    bool attachProgram() noexcept override;
    void detachProgram() noexcept override;

    auto getRingBufferHandler() -> int(*)(void*, void*, size_t) override;
    static int ringBufferHandler(void *ctx, void *data, size_t data_sz);

    std::string cgroup_path_;
    std::string log_path_;
    struct bpf_link *link_;
    int cgroup_fd_;
};
