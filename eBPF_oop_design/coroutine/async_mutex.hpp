#pragma once

#include <coroutine>
#include <queue>
#include <map>
#include <mutex>
#include <string>

struct AsyncFileMutex {
    struct Awaiter {
        AsyncFileMutex& parent;
        std::string filename;
        bool grabbed = false;

        // Check if the file is free immediately
        bool await_ready() {
            std::lock_guard lock(parent.mtx);
            if (!parent.file_states[filename].is_locked) {
                parent.file_states[filename].is_locked = true;
                grabbed = true;
                return true; // Don't suspend
            }
            return false; // Suspend and go to await_suspend
        }

        // If busy, save the coroutine handle to resume later
        void await_suspend(std::coroutine_handle<> handle) {
            parent.file_states[filename].waiters.push(handle);
        }

        void await_resume() {}
    };

    Awaiter lock(std::string filename) { return Awaiter{*this, filename}; }

    void unlock(std::string filename) {
        std::coroutine_handle<> next_task = nullptr;
        {
            std::lock_guard lock(mtx);
            auto& state = file_states[filename];
            if (!state.waiters.empty()) {
                next_task = state.waiters.front();
                state.waiters.pop();
            } else {
                state.is_locked = false;
            }
        }
        // Resume the next waiting coroutine
        if (next_task) next_task.resume();
    }

private:
    struct FileState {
        bool is_locked = false;
        std::queue<std::coroutine_handle<>> waiters;
    };
    std::mutex mtx; // Protects the map and queues
    std::map<std::string, FileState> file_states;
};
