#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include "../xdp_drop/xdp_drop.hpp"
#include "../action/ActionLoop.hpp"
#include "../log_action/LogAction.hpp"

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
        std::fprintf(stderr, "Error: xdp_drop.bpf.o not found\n");
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

    XdpDropProgram program(object_path, interface, log_path);
    if (!program.loadFilter()) {
        std::fprintf(stderr, "Error: failed to load filter\n");
        return 1;
    }

    std::printf("XDP program loaded on %s using %s\n", interface, object_path.c_str());
    std::printf("Writing events asynchronously to %s\n", log_path.c_str());
    std::printf("Press Ctrl+C to exit.\n");

    while (!exiting) {
        int poll_ret = program.pollEvents(200);
        if (poll_ret < 0 && poll_ret != -EINTR) {
            std::fprintf(stderr, "Error: ring buffer poll failed: %d\n", poll_ret);
            break;
        }
        // Process pending actions (coroutines)
        ActionLoop::getInstance().pump();
    }

    return 0;
}
