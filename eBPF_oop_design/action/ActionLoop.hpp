#ifndef EBPF_OOP_DESIGN_ACTION_LOOP_HPP
#define EBPF_OOP_DESIGN_ACTION_LOOP_HPP

#include <memory>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>

#include "../coroutine/task.hpp"

class IAction {
public:
    virtual ~IAction() = default;
    virtual task<void> execute_async() = 0;
};

class ActionLoop {
public:
    ActionLoop(const ActionLoop&) = delete;
    ActionLoop& operator=(const ActionLoop&) = delete;

    static ActionLoop& getInstance() noexcept;
    ~ActionLoop() noexcept;

    // Push a new action to the queue
    void pushAction(std::unique_ptr<IAction> action) noexcept;

private:
    ActionLoop() noexcept;

    // 背景 event loop
    void pump() noexcept;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::unique_ptr<IAction>> queue_;
    std::thread thread_;
    bool stop_ = false;
};

#endif // EBPF_OOP_DESIGN_ACTION_LOOP_HPP
