#pragma once

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

    bool isLoaded() const noexcept;
    bool loadFilter() noexcept;
    void detachFilter() noexcept;
    int pollEvents(int timeout_ms) noexcept;

protected:
    bool openObject() noexcept;
    void closeObject() noexcept;

    int getProgramFd(const std::string& prog_name) const noexcept;
    virtual bool attachProgram() noexcept = 0;
    virtual void detachProgram() noexcept = 0;

    int getMapFd(const std::string& map_name) const noexcept;
    bool registerEventHandler() noexcept;
    virtual int (*getRingBufferHandler())(void*, void*, size_t) = 0;

    int pollRingBuffer(int timeout_ms) noexcept;
    void releaseRingBuffer() noexcept;

    bool loaded_;
    std::string objectPath_;
    struct bpf_object *object_;
    struct ring_buffer *ring_buffer_;
};
