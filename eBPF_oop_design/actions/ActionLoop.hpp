#pragma once

#include <memory>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>

#include "../coroutine/task.hpp"
#include "../coroutine/fire_forget.hpp"

class IAction {
public:
    virtual ~IAction() = default;
    virtual FireForget execute_async() noexcept = 0;
};

class ActionLoop {
public:
    ActionLoop(const ActionLoop&) = delete;
    ActionLoop& operator=(const ActionLoop&) = delete;

    static ActionLoop& getInstance() noexcept;
    ~ActionLoop() noexcept;

    void pushAction(std::unique_ptr<IAction> action) noexcept;

private:
    ActionLoop() noexcept;

    void pump() noexcept;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::unique_ptr<IAction>> queue_;
    std::thread thread_;
    bool stop_ = false;
};
