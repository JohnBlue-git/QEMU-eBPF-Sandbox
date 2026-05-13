#include "ActionLoop.hpp"

ActionLoop::ActionLoop() noexcept : stop_(false) {
    thread_ = std::thread([this] { this->pump(); });
}

ActionLoop::~ActionLoop() noexcept {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
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
    {
        std::lock_guard lock(mutex_);
        queue_.push(std::move(action));
    }
    cv_.notify_one();
}

void ActionLoop::pump() noexcept
{
    while (true) {
        std::unique_ptr<IAction> action;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock,
                [this]{ return stop_ || !queue_.empty(); });

            if (stop_ && queue_.empty()) {
                break;
            }
            action = std::move(queue_.front());
            queue_.pop();
        }

        if (action) {
            action->execute_async();
        }
    }
}
