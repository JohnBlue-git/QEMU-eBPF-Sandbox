#ifndef EBPF_OOP_DESIGN_BPF_PROGRAM_HPP
#define EBPF_OOP_DESIGN_BPF_PROGRAM_HPP

#include <string>
#include <bpf/libbpf.h>

class BpfProgram {
public:
    explicit BpfProgram(std::string object_path) noexcept;
    virtual ~BpfProgram() noexcept;

    BpfProgram(const BpfProgram&) = delete;
    BpfProgram& operator=(const BpfProgram&) = delete;

    BpfProgram(BpfProgram&& other) noexcept;
    BpfProgram& operator=(BpfProgram&& other) noexcept;

    virtual bool loadFilter();
    void detach() noexcept;
    bool isLoaded() const noexcept;

protected:
    virtual bool attachProgram() = 0;
    virtual void detachProgram() noexcept = 0;

    bool openObject() noexcept;
    int getMapFd(const std::string& map_name) const noexcept;
    int getProgramFd(const std::string& prog_name) const noexcept;

    std::string objectPath_;
    struct bpf_object *object_;
    bool loaded_;
};

#endif // EBPF_OOP_DESIGN_BPF_PROGRAM_HPP
