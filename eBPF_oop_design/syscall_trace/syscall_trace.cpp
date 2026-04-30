#include <bpf/libbpf.h>
#include <cstdio>
#include <cstring>
#include <memory>

#include "syscall_trace.hpp"
#include "../action/ActionLoop.hpp"
#include "../log_action/LogAction.hpp"

struct syscall_trace_event {
	__u64 ts_ns;
	__u32 syscall_id;
	__u32 pid;
	__u32 tid;
	__u64 count;
};

SyscallTraceProgram::SyscallTraceProgram(std::string object_path, std::string log_path) noexcept
	: BpfProgram(std::move(object_path)),
	  log_path_(std::move(log_path)),
	  prog_openat(nullptr),
	  prog_read(nullptr),
	  prog_write(nullptr),
	  link_openat_(nullptr),
	  link_read_(nullptr),
	  link_write_(nullptr)
{}

SyscallTraceProgram::SyscallTraceProgram(SyscallTraceProgram&& other) noexcept
	: BpfProgram(std::move(other)),
	  log_path_(std::move(other.log_path_)),
	  prog_openat(other.prog_openat),
	  prog_read(other.prog_read),
	  prog_write(other.prog_write),
	  link_openat_(other.link_openat_),
	  link_read_(other.link_read_),
	  link_write_(other.link_write_)
{
    other.prog_openat = nullptr;
    other.prog_read = nullptr;
    other.prog_write = nullptr;
	other.link_openat_ = nullptr;
	other.link_read_ = nullptr;
	other.link_write_ = nullptr;
}

SyscallTraceProgram& SyscallTraceProgram::operator=(SyscallTraceProgram&& other) noexcept
{
	if (this != &other) {
		BpfProgram::operator=(std::move(other));

		this->log_path_ = std::move(other.log_path_);
		this->link_openat_ = other.link_openat_;
		this->link_read_ = other.link_read_;
		this->link_write_ = other.link_write_;
		this->prog_openat = other.prog_openat;
		this->prog_read = other.prog_read;
		this->prog_write = other.prog_write;

        other.prog_openat = nullptr;
        other.prog_read = nullptr;
        other.prog_write = nullptr;
		other.link_openat_ = nullptr;
		other.link_read_ = nullptr;
		other.link_write_ = nullptr;
	}
	return *this;
}

SyscallTraceProgram::~SyscallTraceProgram() noexcept
{}

bool SyscallTraceProgram::attachProgram() noexcept {
    this->prog_openat = bpf_object__find_program_by_name(this->object_, "trace_sys_exit_openat");
    this->prog_read = bpf_object__find_program_by_name(this->object_, "trace_sys_exit_read");
    this->prog_write = bpf_object__find_program_by_name(this->object_, "trace_sys_exit_write");

    if (!this->prog_openat || !this->prog_read || !this->prog_write) {
        fprintf(stderr, "Error: one or more tracepoint programs not found\n");
        return false;
    }

    this->link_openat_ = bpf_program__attach_tracepoint(this->prog_openat, "syscalls", "sys_exit_openat");
    if (!this->link_openat_ || libbpf_get_error(this->link_openat_)) {
        fprintf(stderr, "Error: failed to attach sys_exit_openat: %s\n", strerror(errno));
        return false;
    }

    this->link_read_ = bpf_program__attach_tracepoint(this->prog_read, "syscalls", "sys_exit_read");
    if (!this->link_read_ || libbpf_get_error(this->link_read_)) {
        fprintf(stderr, "Error: failed to attach sys_exit_read: %s\n", strerror(errno));
        return false;
    }

    this->link_write_ = bpf_program__attach_tracepoint(this->prog_write, "syscalls", "sys_exit_write");
    if (!this->link_write_ || libbpf_get_error(this->link_write_)) {
        fprintf(stderr, "Error: failed to attach sys_exit_write: %s\n", strerror(errno));
        return false;
    }
    return true;
}

void SyscallTraceProgram::detachProgram() noexcept
{
    if (this->link_openat_) {
        bpf_link__destroy(this->link_openat_);
        this->link_openat_ = nullptr;
    }
    if (this->link_read_) {
        bpf_link__destroy(this->link_read_);
        this->link_read_ = nullptr;
    }
    if (this->link_write_) {
        bpf_link__destroy(this->link_write_);
        this->link_write_ = nullptr;
    }
    this->link_openat_ = nullptr;
    this->link_read_ = nullptr;
    this->link_write_ = nullptr;
}

auto SyscallTraceProgram::getRingBufferHandler() -> int(*)(void*, void*, size_t)
{
    return &SyscallTraceProgram::ringBufferHandler;
}

int SyscallTraceProgram::ringBufferHandler(void *ctx, void *data, size_t data_sz) {
	if (data_sz < sizeof(syscall_trace_event))
		return 0;

	auto self = static_cast<SyscallTraceProgram*>(ctx);
	auto event = static_cast<const syscall_trace_event*>(data);
	char line[256];
	std::snprintf(line, sizeof(line),
				 "ts_ns=%llu syscall_id=%u pid=%u tid=%u count=%llu",
				 (unsigned long long)event->ts_ns,
				 event->syscall_id,
				 event->pid,
				 event->tid,
				 (unsigned long long)event->count);

	auto action = std::make_unique<LogAction>(std::string(line), self->log_path_);
	ActionLoop::getInstance().pushAction(std::move(action));

	return 0;
}