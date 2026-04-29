#include <fstream>
#include <filesystem>

#include "LogAction.hpp"

LogAction::LogAction(std::string message, std::string path)
    : message_(std::move(message))
    , path_(std::move(path))
{
    if (!ensure_log_directory(path_)) {
        throw std::runtime_error("Failed to create log directory for " + path_);
    }
}

bool LogAction::ensure_log_directory(const std::string& path) noexcept
{
    if (path.empty()) {
        return false;
    }

    std::filesystem::path dir = std::filesystem::path(path).parent_path();
    if (dir.empty()) {
        return true;
    }

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return !ec;
}

task<void> LogAction::execute_async()
{
    std::ofstream file(path_, std::ios::app);
    if (file.is_open()) {
        file << message_ << "\n";
        file.flush();
    }
    co_return;
}
