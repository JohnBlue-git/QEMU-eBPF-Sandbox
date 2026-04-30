#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include "socket_filter.hpp"
#include "../action/ActionLoop.hpp"
#include "../log_action/LogAction.hpp"

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
    if (level == LIBBPF_DEBUG)
        return 0;
    std::fprintf(stderr, "libbpf: ");
    return std::vfprintf(stderr, format, args);
}

static volatile sig_atomic_t exiting = 0;

static void handleSignal(int sig)
{
    (void)sig;
    exiting = 1;
}

static bool findObject(const std::vector<std::string>& candidates, std::string& result)
{
    for (const auto& candidate : candidates) {
        if (access(candidate.c_str(), R_OK) == 0) {
            result = candidate;
            return true;
        }
    }
    return false;
}

int main(int argc, char* argv[])
{
    const char* interface = argc > 1 ? argv[1] : "lo";

    std::vector<std::string> object_candidates = {
        "/opt/ebpf_oop_design/socket_filter.bpf.o",
        "./socket_filter.bpf.o",
        "build/socket_filter.bpf.o",
    };

    std::vector<std::string> log_candidates = {
        "/var/log/ebpf_oop_design/socket_filter.events.log",
        "./socket_filter.events.log",
    };

    std::string object_path;
    if (!findObject(object_candidates, object_path)) {
        std::fprintf(stderr, "Error: socket_filter.bpf.o not found\n");
        return 1;
    }

    std::string log_path;
    if (!findObject(log_candidates, log_path)) {
        log_path = log_candidates.front();
    }

    // Ensure log directory exists
    if (!LogAction::ensure_log_directory(log_path)) {
        std::fprintf(stderr, "Error: failed to create log directory for %s\n", log_path.c_str());
        return 1;
    }

    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    std::string log_path_full = "/var/log/ebpf_oop_design/socket_filter.events.log";

    SocketFilterProgram program(object_path, interface, log_path_full);
    if (!program.loadFilter()) {
        std::fprintf(stderr, "Error: failed to load filter\n");
        return 1;
    }

    std::printf("Socket filter program loaded using %s\n", object_path.c_str());
    std::printf("Writing events asynchronously to %s\n", log_path.c_str());
    std::printf("Press Ctrl+C to exit.\n");

    while (!exiting) {
        int poll_ret = program.pollEvents(200);
        if (poll_ret < 0 && poll_ret != -EINTR) {
            std::fprintf(stderr, "Error: ring buffer poll failed: %d\n", poll_ret);
            break;
        }
    }

    return 0;
}
