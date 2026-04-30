#ifndef EBPF_OOP_DESIGN_XDP_DROP_HPP
#define EBPF_OOP_DESIGN_XDP_DROP_HPP

#include <string>

#include "BpfProgram.hpp"

class XdpDropProgram final : public BpfProgram {
public:
    XdpDropProgram(std::string object_path, std::string interface, std::string log_path) noexcept;
    XdpDropProgram(XdpDropProgram&&) noexcept;
    ~XdpDropProgram() noexcept override;

    XdpDropProgram& operator=(XdpDropProgram&&) noexcept;

private:
    bool attachProgram() noexcept override;
    void detachProgram() noexcept override;

    auto getRingBufferHandler() -> int(*)(void*, void*, size_t) override;
    static int ringBufferHandler(void *ctx, void *data, size_t data_sz);

    int ifindex_;
    bool attached_;
    std::string interface_;
    std::string log_path_;
};

#endif // EBPF_OOP_DESIGN_XDP_DROP_HPP
