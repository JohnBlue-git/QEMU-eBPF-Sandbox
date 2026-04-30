#include <bpf/libbpf.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <fcntl.h>
#include <unistd.h>

#include "cgroup_egress.hpp"
#include "../action/ActionLoop.hpp"
#include "../log_action/LogAction.hpp"

struct cgroup_egress_event {
    __u64 ts_ns;
    __u32 pkt_len;
    __u32 ifindex;
    __u8 ip_proto;
    __u8 action;
    __u8 pad[2];
};

CgroupEgressProgram::CgroupEgressProgram(std::string object_path, std::string cgroup_path, std::string log_path) noexcept
    : BpfProgram(std::move(object_path)),
      cgroup_path_(std::move(cgroup_path)),
      log_path_(std::move(log_path)),
      link_(nullptr),
      cgroup_fd_(-1)
{}

CgroupEgressProgram::CgroupEgressProgram(CgroupEgressProgram&& other) noexcept
    : BpfProgram(std::move(other)),
      cgroup_path_(std::move(other.cgroup_path_)),
      log_path_(std::move(other.log_path_)),
      link_(other.link_),
      cgroup_fd_(other.cgroup_fd_)
{
    other.link_ = nullptr;
    other.cgroup_fd_ = -1;
}

CgroupEgressProgram& CgroupEgressProgram::operator=(CgroupEgressProgram&& other) noexcept
{
    if (this != &other) {
        BpfProgram::operator=(std::move(other));

        log_path_ = std::move(other.log_path_);
        link_ = other.link_;
        cgroup_fd_ = other.cgroup_fd_;

        other.link_ = nullptr;
        other.cgroup_fd_ = -1;
    }
    return *this;
}

CgroupEgressProgram::~CgroupEgressProgram() noexcept
{}

bool CgroupEgressProgram::attachProgram() noexcept
{
    struct bpf_program *prog = bpf_object__find_program_by_name(object_, "deny_icmp_egress");
    if (!prog) {
        std::fprintf(stderr, "Error: deny_icmp_egress program not found in object\n");
        return false;
    }

    cgroup_fd_ = open(this->cgroup_path_.c_str(), O_RDONLY | O_DIRECTORY);
    if (cgroup_fd_ < 0) {
        std::fprintf(stderr, "Error: failed to open cgroup path\n");
        return false;
    }

    link_ = bpf_program__attach_cgroup(prog, cgroup_fd_);
    if (libbpf_get_error(link_)) {
        std::fprintf(stderr, "Error: failed to attach cgroup egress program\n");
        link_ = nullptr;
        return false;
    }
    return true;
}

void CgroupEgressProgram::detachProgram() noexcept
{
    if (link_) {
        bpf_link__destroy(link_);
        link_ = nullptr;
    }

    if (cgroup_fd_ >= 0) {
        close(cgroup_fd_);
        cgroup_fd_ = -1;
    }
}

auto CgroupEgressProgram::getRingBufferHandler() -> int(*)(void*, void*, size_t)
{
    return &CgroupEgressProgram::ringBufferHandler;
}

int CgroupEgressProgram::ringBufferHandler(void *ctx, void *data, size_t data_sz)
{
    if (data_sz < sizeof(cgroup_egress_event))
        return 0;

    auto self = static_cast<CgroupEgressProgram*>(ctx);
    auto event = static_cast<const cgroup_egress_event*>(data);
    char line[256];
    std::snprintf(line, sizeof(line),
                 "ts_ns=%llu ifindex=%u pkt_len=%u proto=%u action=%s",
                 (unsigned long long)event->ts_ns,
                 event->ifindex,
                 event->pkt_len,
                 event->ip_proto,
                 event->action ? "ALLOW" : "DENY");

    auto action = std::make_unique<LogAction>(std::string(line), self->log_path_);
    ActionLoop::getInstance().pushAction(std::move(action));

    return 0;
}
