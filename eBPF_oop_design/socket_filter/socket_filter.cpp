#include <bpf/libbpf.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <net/if.h>

#include "socket_filter.hpp"
#include "../action/ActionLoop.hpp"
#include "../log_action/LogAction.hpp"

struct socket_filter_event {
    __u64 ts_ns;
    __u32 pkt_len;
    __u32 ifindex;
    __u8 ip_proto;
    __u8 action;
    __u8 pad[2];
};

SocketFilterProgram::SocketFilterProgram(std::string object_path, std::string interface, std::string log_path) noexcept
    : BpfProgram(std::move(object_path)),
      interface_(std::move(interface)),
      log_path_(std::move(log_path)),
      ifindex_(0),
      sock_fd_(-1)
{
    this->ifindex_ = if_nametoindex(this->interface_.c_str());
}

SocketFilterProgram::SocketFilterProgram(SocketFilterProgram&& other) noexcept
    : BpfProgram(std::move(other)),
      interface_(std::move(other.interface_)),
      log_path_(std::move(other.log_path_)),
      ifindex_(other.ifindex_),
      sock_fd_(other.sock_fd_)
{
    other.ifindex_ = 0;
    other.sock_fd_ = -1;
}

SocketFilterProgram& SocketFilterProgram::operator=(SocketFilterProgram&& other) noexcept
{
    if (this != &other) {
        BpfProgram::operator=(std::move(other));

        this->ifindex_ = other.ifindex_;
        this->sock_fd_ = other.sock_fd_;
        this->interface_ = std::move(other.interface_);
        this->log_path_ = std::move(other.log_path_);

        other.ifindex_ = 0;
        other.sock_fd_ = -1;
    }
    return *this;
}

SocketFilterProgram::~SocketFilterProgram() noexcept
{}

bool SocketFilterProgram::attachProgram() noexcept {
    int prog_fd = getProgramFd("filter_tcp_only");
    if (prog_fd < 0) {
        std::fprintf(stderr, "Error: filter_tcp_only program not found\n");
        return false;
    }

    sock_fd_ = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock_fd_ < 0) {
        std::fprintf(stderr, "Error: failed to create raw socket: %s\n", strerror(errno));
        sock_fd_ = -1;
        return false;
    }

    int ifindex = if_nametoindex(interface_.c_str());
    if (!ifindex) {
        std::fprintf(stderr, "Error: interface '%s' not found\n", interface_.c_str());
        return false;
    }

    struct sockaddr_ll sll = {};
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifindex;
    sll.sll_protocol = htons(ETH_P_ALL);
    if (bind(sock_fd_, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
        std::fprintf(stderr, "Error: failed to bind socket: %s\n", strerror(errno));
        return false;
    }

    if (setsockopt(sock_fd_, SOL_SOCKET, SO_ATTACH_BPF, &prog_fd, sizeof(prog_fd)) < 0) {
        std::fprintf(stderr, "Error: failed to attach socket filter: %s\n", strerror(errno));
        return false;
    }
    return true;
}

void SocketFilterProgram::detachProgram() noexcept
{
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }
}

auto SocketFilterProgram::getRingBufferHandler() -> int(*)(void*, void*, size_t)
{
    return &SocketFilterProgram::ringBufferHandler;
}

int SocketFilterProgram::ringBufferHandler(void* ctx, void* data, size_t data_sz)
{
    if (data_sz < sizeof(socket_filter_event))
        return 0;

    auto self = static_cast<SocketFilterProgram*>(ctx);
    auto event = static_cast<const socket_filter_event*>(data);
    char line[256];
    std::snprintf(line, sizeof(line),
                 "ts_ns=%llu ifindex=%u pkt_len=%u proto=%u action=%s",
                 (unsigned long long)event->ts_ns,
                 event->ifindex,
                 event->pkt_len,
                 event->ip_proto,
                 event->action ? "PASS" : "DROP");

    auto action = std::make_unique<LogAction>(std::string(line), self->log_path_);
    ActionLoop::getInstance().pushAction(std::move(action));

    return 0;
}
