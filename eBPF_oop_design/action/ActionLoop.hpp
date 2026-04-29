#ifndef EBPF_OOP_DESIGN_ACTION_LOOP_HPP
#define EBPF_OOP_DESIGN_ACTION_LOOP_HPP

#include <memory>
#include <queue>
#include <mutex>

#include "../coroutine/task.hpp"

class IAction {
public:
    virtual ~IAction() = default;
    virtual task<void> execute_async() = 0;
};

class ActionLoop {
public:
    static ActionLoop& getInstance() noexcept;

    ActionLoop(const ActionLoop&) = delete;
    ActionLoop& operator=(const ActionLoop&) = delete;

    ~ActionLoop() noexcept = default;

    void pushAction(std::unique_ptr<IAction> action) noexcept;
    // Process pending actions synchronously
    void pump() noexcept;

private:
    ActionLoop() noexcept = default;

    std::mutex mutex_;
    std::queue<std::unique_ptr<IAction>> queue_;
};

#endif // EBPF_OOP_DESIGN_ACTION_LOOP_HPP
