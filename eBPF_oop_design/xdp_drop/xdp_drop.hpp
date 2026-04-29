#ifndef EBPF_OOP_DESIGN_XDP_DROP_HPP
#define EBPF_OOP_DESIGN_XDP_DROP_HPP

#include <string>

#include "BpfProgram.hpp"

class XdpDropProgram final : public BpfProgram {
public:
    XdpDropProgram(std::string object_path, std::string interface, std::string log_path) noexcept;
    XdpDropProgram(XdpDropProgram&&) noexcept;
    XdpDropProgram& operator=(XdpDropProgram&&) noexcept;
    ~XdpDropProgram() noexcept override;

    bool loadFilter() override;
    void detachProgram() noexcept override;
    int pollEvents(int timeout_ms) const noexcept;

private:
    bool attachProgram() override;
    static int ringBufferHandler(void *ctx, void *data, size_t data_sz);

    std::string interface_;
    std::string log_path_;
    int ifindex_;
    struct ring_buffer *ring_buffer_;
};

#endif // EBPF_OOP_DESIGN_XDP_DROP_HPP
