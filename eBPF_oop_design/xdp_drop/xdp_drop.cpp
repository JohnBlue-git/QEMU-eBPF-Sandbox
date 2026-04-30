#include <arpa/inet.h>
#include <linux/if_link.h>
#include <net/if.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

#include "xdp_drop.hpp"
#include "../action/ActionLoop.hpp"
#include "../log_action/LogAction.hpp"

struct xdp_drop_event {
    __u64 ts_ns;
    __u32 pkt_len;
    __u32 ifindex;
    __u8 action;
    __u8 pad[3];
};

XdpDropProgram::XdpDropProgram(std::string object_path,
                               std::string interface,
                               std::string log_path) noexcept
    : BpfProgram(std::move(object_path)),
      interface_(std::move(interface)),
      log_path_(std::move(log_path)),
      ifindex_(0),
      attached_(false)
{
    this->ifindex_ = if_nametoindex(this->interface_.c_str());
}

XdpDropProgram::XdpDropProgram(XdpDropProgram&& other) noexcept
    : BpfProgram(std::move(other)),
      interface_(std::move(other.interface_)),
      log_path_(std::move(other.log_path_)),
      ifindex_(other.ifindex_),
      attached_(other.attached_)
{
    other.ifindex_ = 0;
    other.attached_ = false;
}

XdpDropProgram& XdpDropProgram::operator=(XdpDropProgram&& other) noexcept
{
    if (this != &other) {
        BpfProgram::operator=(std::move(other));

        this->ifindex_ = other.ifindex_;
        this->attached_ = other.attached_;
        this->interface_ = std::move(other.interface_);
        this->log_path_ = std::move(other.log_path_);

        other.ifindex_ = 0;
        other.attached_ = false;
    }
    return *this;
}

XdpDropProgram::~XdpDropProgram() noexcept
{}

bool XdpDropProgram::attachProgram() noexcept
{
    if (!this->ifindex_) {
        std::fprintf(stderr, "Error: interface '%s' not found\n", this->interface_.c_str());
        return false;
    }

    int prog_fd = getProgramFd("xdp_pass");
    if (prog_fd < 0) {
        std::fprintf(stderr, "Error: xdp program not found\n");
        return false;
    }

    if (bpf_xdp_attach(this->ifindex_, prog_fd, XDP_FLAGS_UPDATE_IF_NOEXIST, nullptr) < 0) {
        std::fprintf(stderr, "Error: failed to attach XDP program\n");
        return false;
    }
    attached_ = true;
    return true;
}

void XdpDropProgram::detachProgram() noexcept
{
    if (this->attached_) {
        bpf_xdp_detach(this->ifindex_, XDP_FLAGS_UPDATE_IF_NOEXIST, nullptr);
        this->attached_ = false;
    }
}

auto XdpDropProgram::getRingBufferHandler() -> int(*)(void*, void*, size_t)
{
    return &XdpDropProgram::ringBufferHandler;
}

int XdpDropProgram::ringBufferHandler(void *ctx, void *data, size_t data_sz)
{
    if (data_sz < sizeof(xdp_drop_event))
        return 0;

    auto self = static_cast<XdpDropProgram*>(ctx);
    auto event = static_cast<const xdp_drop_event *>(data);
    char line[256];
    std::snprintf(line, sizeof(line),
                  "ts_ns=%llu ifindex=%u pkt_len=%u action=%s",
                  (unsigned long long)event->ts_ns,
                  event->ifindex,
                  event->pkt_len,
                  event->action ? "PASS" : "DROP");

    auto action = std::make_unique<LogAction>(
        std::string(line),
        self->log_path_
    );
    ActionLoop::getInstance().pushAction(std::move(action));

    return 0;
}
