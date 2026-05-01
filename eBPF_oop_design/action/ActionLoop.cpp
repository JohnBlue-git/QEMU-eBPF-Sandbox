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

FireForget ActionLoop::pushAction(std::unique_ptr<IAction> action) noexcept
{
    {
        std::lock_guard lock(mutex_);
        queue_.push(std::move(action));
    }
    cv_.notify_one();
    co_return;
}

void ActionLoop::pump() noexcept
{
    while (true) {
        std::unique_ptr<IAction> action;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this]{ return stop_ || !queue_.empty(); });
            if (stop_ && queue_.empty()) break;
            if (!queue_.empty()) {
                action = std::move(queue_.front());
                queue_.pop();
            }
        }
        if (action) {
            // Execute the action asynchronously
            auto t = action->execute_async();
            // To be truly async, the task should handle its own continuation
            // or be resumed by a dedicated background executor.
            t.resume();
            // Loop immediately continues to the next item in queue_
        }
    }
}
