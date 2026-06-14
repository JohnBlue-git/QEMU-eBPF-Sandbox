#include <exception>
#include <iostream>
#include <utility>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include "ActionLoop.hpp"

namespace asio = boost::asio;

ActionLoop::ActionLoop() noexcept
    : io_context_()
    , work_guard_(asio::make_work_guard(io_context_))
{
    try {
        thread_ = std::thread([this] { io_context_.run(); });
    } catch (...) {
        stop_.store(true, std::memory_order_relaxed);
        work_guard_.reset();
        io_context_.stop();
    }
}

ActionLoop::~ActionLoop() noexcept {
    stop_.store(true, std::memory_order_relaxed);
    work_guard_.reset();
    io_context_.stop();
    if (thread_.joinable()) thread_.join();
}

ActionLoop& ActionLoop::getInstance() noexcept
{
    static ActionLoop instance;
    return instance;
}

// Tough push would be blocked by the mutex, it is still sufficient for our use case since the ActionLoop is designed to be a single producer (main thread) and single consumer (background thread) model.
// If we want to support multiple producers, we would need to implement a more sophisticated event loop for ActionLoop
void ActionLoop::pushAction(std::unique_ptr<IAction> action) noexcept
{
    // If the action is null or the loop is stopping, we simply ignore the push request.
    if (!action || stop_.load(std::memory_order_relaxed)) {
        return;
    }

    try {
        asio::co_spawn(io_context_,
            [this, action = std::move(action)]() mutable -> asio::awaitable<void> {
            // If the action is null or the loop is stopping, we simply ignore the execution request.
            if (!action || stop_.load(std::memory_order_relaxed)) {
                co_return;
            }

            // We execute the action asynchronously and catch any exceptions to prevent them from propagating and potentially crashing the ActionLoop.
            try {
                co_await action->execute_async();
            } catch (std::exception& e) {
                std::cerr << "Error: failed to execute action: " << e.what() << '\n';
            } catch (...) {
                std::cerr << "Error: failed to execute action due to unknown error\n";
            }

            co_return;
            },
            asio::detached);

    } catch (std::exception& e) {
        std::cerr << "Error: failed to push action to ActionLoop: " << e.what() << '\n';
    } catch (...) {
        std::cerr << "Error: failed to push action to ActionLoop due to unknown error\n";
    }
}
