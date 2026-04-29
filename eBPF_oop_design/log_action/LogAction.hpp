#ifndef EBPF_OOP_DESIGN_LOGACTION_HPP
#define EBPF_OOP_DESIGN_LOGACTION_HPP

#include <string>

#include "../action/ActionLoop.hpp"

class LogAction final : public IAction {
public:
    LogAction(std::string message, std::string path);

    task<void> execute_async() override;

    static bool ensure_log_directory(const std::string& path) noexcept;

private:
    std::string message_;
    std::string path_;
};

#endif // EBPF_OOP_DESIGN_LOGACTION_HPP
