#include <bpf/libbpf.h>
#include <cstdio>

#include "BpfProgram.hpp"

BpfProgram::BpfProgram(std::string object_path) noexcept
    : objectPath_(std::move(object_path)), object_(nullptr), loaded_(false)
{
}

BpfProgram::~BpfProgram() noexcept
{
    detach();
    if (object_) {
        bpf_object__close(object_);
        object_ = nullptr;
    }
}

BpfProgram::BpfProgram(BpfProgram&& other) noexcept
    : objectPath_(std::move(other.objectPath_)), object_(other.object_), loaded_(other.loaded_)
{
    other.object_ = nullptr;
    other.loaded_ = false;
}

BpfProgram& BpfProgram::operator=(BpfProgram&& other) noexcept
{
    if (this != &other) {
        detach();
        if (object_) {
            bpf_object__close(object_);
        }

        objectPath_ = std::move(other.objectPath_);
        object_ = other.object_;
        loaded_ = other.loaded_;

        other.object_ = nullptr;
        other.loaded_ = false;
    }
    return *this;
}

bool BpfProgram::openObject() noexcept
{
    if (object_)
        return true;

    object_ = bpf_object__open_file(objectPath_.c_str(), nullptr);
    if (!object_ || libbpf_get_error(object_)) {
        object_ = nullptr;
        return false;
    }

    return true;
}

bool BpfProgram::loadFilter()
{
    if (!openObject())
        return false;

    if (bpf_object__load(object_))
        return false;

    if (!attachProgram())
        return false;

    loaded_ = true;
    return true;
}

void BpfProgram::detach() noexcept
{
    if (loaded_) {
        detachProgram();
        loaded_ = false;
    }
}

bool BpfProgram::isLoaded() const noexcept
{
    return loaded_;
}

int BpfProgram::getMapFd(const std::string& map_name) const noexcept
{
    if (!object_)
        return -1;
    return bpf_object__find_map_fd_by_name(object_, map_name.c_str());
}

int BpfProgram::getProgramFd(const std::string& prog_name) const noexcept
{
    if (!object_)
        return -1;
    struct bpf_program *prog = bpf_object__find_program_by_name(object_, prog_name.c_str());
    return prog ? bpf_program__fd(prog) : -1;
}
