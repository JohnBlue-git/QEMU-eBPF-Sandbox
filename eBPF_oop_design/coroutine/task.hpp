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

        // Traditional implementation of `await_suspend`:
        // bool await_suspend(std::coroutine_handle<> awaiting) {
        //     coro.resume();
        //     return true;
        // }
        //
        // This is the CRITICAL fix
        // Now, the awaiter will correctly suspend and hand over control.
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) {
            coro.promise().continuation = awaiting; // Set the parent
            return coro; // Symmetric transfer: switch to the child task
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

        // The "parent" coroutine
        std::coroutine_handle<> continuation;

        // Use a custom final_awaiter to resume the continuation
        struct final_awaiter {
            bool await_ready() noexcept { return false; }
            std::coroutine_handle<> await_suspend(handle_type h) noexcept {
                if (h.promise().continuation) {
                    return h.promise().continuation; // Resume parent
                }
                return std::noop_coroutine(); // Return to ActionLoop
            }
            void await_resume() noexcept {}
        };

        auto get_return_object() {
            return Task{handle_type::from_promise(*this)};
        }

        auto initial_suspend() noexcept {
            return std::suspend_always{};
        }

        auto final_suspend() noexcept {
            return final_awaiter{};
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

    /*
    ### What happens when you hit `co_await`?

    When the CPU reaches a `co_await` line, it performs a three-step check on your `awaiter` object:

    #### Step A: `await_ready()`
    The compiler asks: "Is the data already here?"
    *   If **`true`**: The coroutine doesn't even pause. It keeps running immediately.
    *   If **`false`**: The coroutine prepares to pause (suspends).

    #### Step B: `await_suspend(handle)`
    This is the "Decision Point." The coroutine has already saved its variables to the heap. Now it asks: "Should I actually yield control back to the caller?"

    *   **If you return `true`:**
        The coroutine **suspends**. Control immediately returns to the caller (your `ActionLoop`). The coroutine is now "parked" and waiting for someone to call `.resume()` on its handle later. This is the **True Async** path.
    *   **If you return `false`:**
        The coroutine **wakes back up immediately**. Even though it went through the effort of suspending, returning `false` tells the compiler: "Never mind, I'm ready to continue right now on the current thread."

    #### Step C: `await_resume()`
    The coroutine is now officially "back." This function returns the result of the `co_await` expression.
    */
    struct awaiter {
        handle_type coro;

        bool await_ready() const noexcept {
            return coro.done();
        }

        /* Traditional implementation of `await_suspend`:
        bool await_suspend(std::coroutine_handle<> awaiting) {
            coro.resume();
            // Tells the compiler to suspend current coroutin and resume later
            return true;
            // Tells the compiler NOT to suspend current courotine and continue immediately
            return false;
        }*/
        //
        // Use Symmetric Transfer (returning a handle) instead of bool
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) noexcept {
            // Tell the child task to resume the parent ('awaiting') when finished
            coro.promise().continuation = awaiting;
            // Return the child's handle to start executing it
            return coro; 
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

        // The "parent" coroutine
        std::coroutine_handle<> continuation;

        // Use a custom final_awaiter to resume the continuation
        struct final_awaiter {
            bool await_ready() noexcept { return false; }
            std::coroutine_handle<> await_suspend(handle_type h) noexcept {
                if (h.promise().continuation) {
                    return h.promise().continuation; // Resume parent
                }
                return std::noop_coroutine(); // Return to ActionLoop
            }
            void await_resume() noexcept {}
        };

        auto get_return_object() {
            return Task{handle_type::from_promise(*this)};
        }

        auto initial_suspend() noexcept {
            return std::suspend_always{};
        }

        auto final_suspend() noexcept {
            return final_awaiter{};
        }

        void return_void() noexcept {}

        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }
    };

    handle_type coro;
};
