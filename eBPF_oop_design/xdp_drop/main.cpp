#include <csignal>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

#include <bpf/libbpf.h>

#include "xdp_drop.hpp"
#include "../actions/LogAction.hpp"

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
    if (level == LIBBPF_DEBUG)
        return 0;
    std::cerr << "libbpf: ";
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
        "/opt/ebpf_oop_design/xdp_drop.bpf.o",
        "./xdp_drop.bpf.o",
        "build/xdp_drop.bpf.o",
    };

    std::vector<std::string> log_candidates = {
        "/var/log/ebpf_oop_design/xdp_drop.events.log",
        "./xdp_drop.events.log",
    };

    std::string object_path;
    if (!findObject(object_candidates, object_path)) {
        std::cerr << "Error: xdp_drop.bpf.o not found\n";
        return 1;
    }

    std::string log_path;
    if (!findObject(log_candidates, log_path)) {
        log_path = log_candidates.front();
    }

    // Ensure log directory exists
    if (!LogAction::ensure_log_directory(log_path)) {
        std::cerr << "Error: failed to create log directory for " << log_path << '\n';
        return 1;
    }

    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    XdpDropProgram program(object_path, interface, log_path);
    if (!program.loadFilter()) {
        std::cerr << "Error: failed to load filter\n";
        return 1;
    }

    std::cout << "XDP program loaded on " << interface << " using " << object_path << '\n';
    std::cout << "Writing events asynchronously to " << log_path << '\n';
    std::cout << "Press Ctrl+C to exit.\n";

    while (!exiting) {
        int poll_ret = program.pollEvents(200);
        if (poll_ret < 0 && poll_ret != -EINTR) {
            std::cerr << "Error: ring buffer poll failed: " << poll_ret << '\n';
            break;
        }
    }

    return 0;
}
