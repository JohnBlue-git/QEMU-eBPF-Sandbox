#ifndef EBPF_OOP_DESIGN_SOCKET_FILTER_HPP
#define EBPF_OOP_DESIGN_SOCKET_FILTER_HPP

#include <string>

#include "../ebpf/BpfProgram.hpp"

class SocketFilterProgram final : public BpfProgram {
public:
    SocketFilterProgram(std::string object_path, std::string interface, std::string log_path) noexcept;
    SocketFilterProgram(SocketFilterProgram&&) noexcept;
    ~SocketFilterProgram() noexcept override;

    SocketFilterProgram& operator=(SocketFilterProgram&&) noexcept;

private:
    bool attachProgram() noexcept override;
    void detachProgram() noexcept override;

    auto getRingBufferHandler() -> int(*)(void*, void*, size_t) override;
    static int ringBufferHandler(void *ctx, void *data, size_t data_sz);

    int ifindex_;
    int sock_fd_;
    std::string interface_;
    std::string log_path_;
};
#endif // EBPF_OOP_DESIGN_SOCKET_FILTER_HPP
