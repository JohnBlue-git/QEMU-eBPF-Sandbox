#pragma once

#include <coroutine>
#include <exception>
#include <utility>

template <typename T>
struct Task {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    Task(handle_type h) : coro(h) {}
    Task(Task &&other) noexcept : coro(other.coro) { other.coro = nullptr; }
    Task(const Task &) = delete;
    ~Task() {
        if (coro) {
            coro.destroy();
        }
    }

    Task &operator=(Task &&other) noexcept {
        if (this != &other) {
            if (coro) coro.destroy();
            coro = other.coro;
            other.coro = nullptr;
        }
        return *this;
    }

    bool ready() const { return !coro || coro.done(); }

    void resume() { if (coro) coro.resume(); }

    T get() {
        while (coro && !coro.done()) {
            coro.resume();
        }
        if (coro.promise().exception) {
            std::rethrow_exception(coro.promise().exception);
        }
        return coro.promise().value;
    }

    struct awaiter {
        handle_type coro;

        bool await_ready() const noexcept {
            return coro.done();
        }

        bool await_suspend(std::coroutine_handle<> awaiting) {
            coro.resume();
            return false;
        }

        T await_resume() {
            if (coro.promise().exception) {
                std::rethrow_exception(coro.promise().exception);
            }
            return coro.promise().value;
        }
    };

    auto operator co_await() & noexcept {
        return awaiter{coro};
    }

    auto operator co_await() && noexcept {
        return awaiter{coro};
    }

    struct promise_type {
        T value;
        std::exception_ptr exception;

        auto get_return_object() {
            return Task{handle_type::from_promise(*this)};
        }

        auto initial_suspend() noexcept {
            return std::suspend_always{};
        }

        auto final_suspend() noexcept {
            return std::suspend_always{};
        }

        void return_value(T v) noexcept {
            value = std::move(v);
        }

        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }
    };

    handle_type coro;
};

template <>
struct Task<void> {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    Task(handle_type h) : coro(h) {}
    Task(Task &&other) noexcept : coro(other.coro) { other.coro = nullptr; }
    Task(const Task &) = delete;
    ~Task() {
        if (coro) {
            coro.destroy();
        }
    }

    Task &operator=(Task &&other) noexcept {
        if (this != &other) {
            if (coro) coro.destroy();
            coro = other.coro;
            other.coro = nullptr;
        }
        return *this;
    }

    bool ready() const { return !coro || coro.done(); }

    void resume() { if (coro) coro.resume(); }

    void get() {
        while (coro && !coro.done()) {
            coro.resume();
        }
        if (coro.promise().exception) {
            std::rethrow_exception(coro.promise().exception);
        }
    }

    struct awaiter {
        handle_type coro;

        bool await_ready() const noexcept {
            return coro.done();
        }

        bool await_suspend(std::coroutine_handle<> awaiting) {
            coro.resume();
            return false;
        }

        void await_resume() {
            if (coro.promise().exception) {
                std::rethrow_exception(coro.promise().exception);
            }
        }
    };

    auto operator co_await() & noexcept {
        return awaiter{coro};
    }

    auto operator co_await() && noexcept {
        return awaiter{coro};
    }

    struct promise_type {
        std::exception_ptr exception;

        auto get_return_object() {
            return Task{handle_type::from_promise(*this)};
        }

        auto initial_suspend() noexcept {
            return std::suspend_always{};
        }

        auto final_suspend() noexcept {
            return std::suspend_always{};
        }

        void return_void() noexcept {}

        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }
    };

    handle_type coro;
};
