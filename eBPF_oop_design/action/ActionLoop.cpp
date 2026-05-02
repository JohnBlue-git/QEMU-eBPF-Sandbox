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
    // Vector to hold active tasks so they remain alive while suspended
    std::vector<Task<void>> active_tasks;
    // Main loop to process actions:
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
            // Execute the action asynchronously:
            //  action->execute_async()
            // Push task to keep tasks alive while they are suspended:
            //  active_tasks.push_back()
            active_tasks.push_back(action->execute_async());
            // To be truly async, the task should handle its own continuation
            // or be resumed by a dedicated background executor.
            active_tasks.back().resume();
            // Loop immediately continues to the next item in queue_
        }
        // Cleanup finished tasks
        std::erase_if(active_tasks, [](auto& t) { return t.ready(); });
    }
}
