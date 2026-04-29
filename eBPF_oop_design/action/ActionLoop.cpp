#include "ActionLoop.hpp"

ActionLoop& ActionLoop::getInstance() noexcept
{
    static ActionLoop instance;
    return instance;
}

void ActionLoop::pushAction(std::unique_ptr<IAction> action) noexcept
{
    std::lock_guard lock(mutex_);
    queue_.push(std::move(action));
}

void ActionLoop::pump() noexcept
{
    while (true) {
        std::unique_ptr<IAction> action;
        {
            std::lock_guard lock(mutex_);
            if (queue_.empty()) break;
            action = std::move(queue_.front());
            queue_.pop();
        }
        if (action) {
            auto t = action->execute_async();
            t.get(); // Run synchronously
        }
    }
}
