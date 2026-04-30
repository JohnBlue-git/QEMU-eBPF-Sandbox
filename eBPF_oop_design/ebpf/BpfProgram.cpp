#include <bpf/libbpf.h>
#include <cstdio>

#include "BpfProgram.hpp"

BpfProgram::BpfProgram(std::string object_path) noexcept
    : objectPath_(std::move(object_path))
    , object_(nullptr)
    , loaded_(false)
    , ring_buffer_(nullptr)
{}

BpfProgram::~BpfProgram() noexcept
{
    this->BpfProgram::detachFilter();
    this->BpfProgram::closeObject();
    this->object_ = nullptr;
}

BpfProgram::BpfProgram(BpfProgram&& other) noexcept
    : objectPath_(std::move(other.objectPath_))
    , object_(other.object_), loaded_(other.loaded_)
    , ring_buffer_(other.ring_buffer_)
{
    other.loaded_ = false;
    other.object_ = nullptr;
    other.ring_buffer_ = nullptr;
}

BpfProgram& BpfProgram::operator=(BpfProgram&& other) noexcept
{
    if (this != &other) {
        this->BpfProgram::detachFilter();
        this->BpfProgram::closeObject();

        this->loaded_ = other.loaded_;
        this->object_ = other.object_;
        this->objectPath_ = std::move(other.objectPath_);
        this->ring_buffer_ = other.ring_buffer_;

        other.loaded_ = false;
        other.object_ = nullptr;
        other.ring_buffer_ = nullptr;
    }
    return *this;
}

bool BpfProgram::isLoaded() const noexcept
{
    return this->loaded_;
}

bool BpfProgram::loadFilter() noexcept
{
    if (!this->BpfProgram::openObject())
        return false;

    if (bpf_object__load(this->object_)) {
        this->BpfProgram::closeObject();
        return false;
    }

    if (!this->attachProgram()) {
        this->BpfProgram::detachFilter();
        this->BpfProgram::closeObject();
        return false;
    }

    if (!this->BpfProgram::registerEventHandler()) {
        this->BpfProgram::detachFilter();
        this->BpfProgram::closeObject();
        return false;
    }

    this->loaded_ = true;
    return true;
}

void BpfProgram::detachFilter() noexcept
{
    this->BpfProgram::releaseRingBuffer();
    this->detachProgram();
    this->loaded_ = false;
}

bool BpfProgram::openObject() noexcept
{
    if (this->object_)
        return true;

    this->object_ = bpf_object__open_file(this->objectPath_.c_str(), nullptr);
    if (!this->object_ || libbpf_get_error(this->object_)) {
        this->object_ = nullptr;
        return false;
    }
    return true;
}

void BpfProgram::closeObject() noexcept
{
    if (this->object_) {
        bpf_object__close(this->object_);
        this->object_ = nullptr;
    }
}

int BpfProgram::pollEvents(int timeout_ms) noexcept
{
    return this->BpfProgram::pollRingBuffer(timeout_ms);
}

int BpfProgram::getProgramFd(const std::string& prog_name) const noexcept
{
    if (!this->object_)
        return -1;

    struct bpf_program *prog = bpf_object__find_program_by_name(this->object_, prog_name.c_str());
    return prog ? bpf_program__fd(prog) : -1;
}

int BpfProgram::getMapFd(const std::string& map_name) const noexcept
{
    if (!this->object_)
        return -1;

    return bpf_object__find_map_fd_by_name(this->object_, map_name.c_str());
}

bool BpfProgram::registerEventHandler() noexcept
{
    int map_fd = getMapFd("events");
    if (map_fd < 0) {
        std::fprintf(stderr, "Error: failed to find events map\n");
        return false;
    }

    ring_buffer_ = ring_buffer__new(map_fd, this->getRingBufferHandler(), this, nullptr);
    if (!ring_buffer_) {
        std::fprintf(stderr, "Error: failed to create ring buffer\n");
        return false;
    }
    return true;
}

int BpfProgram::pollRingBuffer(int timeout_ms) noexcept
{
    if (!this->ring_buffer_)
        return -1;

    return ring_buffer__poll(this->ring_buffer_, timeout_ms);
}

void BpfProgram::releaseRingBuffer() noexcept
{
    if (this->ring_buffer_) {
		ring_buffer__free(this->ring_buffer_);
		this->ring_buffer_ = nullptr;
	}
}
