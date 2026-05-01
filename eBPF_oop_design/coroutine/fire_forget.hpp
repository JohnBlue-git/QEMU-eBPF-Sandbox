#pragma once

#include <coroutine>
#include <iostream>

struct FireForget {
    struct promise_type {
        FireForget get_return_object() {
            return {};
        }

        // Start immediately
        std::suspend_never initial_suspend() noexcept {
            return {};
        }

        // Destroy automatically when done
        std::suspend_never final_suspend() noexcept {
            return {};
        }

        void return_void() noexcept {}

        void unhandled_exception() {
            std::terminate();
        }
    };
};
