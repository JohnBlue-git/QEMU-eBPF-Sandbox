#include <fstream>
#include <filesystem>

#include "LogAction.hpp"

AsyncFileMutex LogAction::g_file_mgr;

LogAction::LogAction(std::string message, std::string path)
    : message_(std::move(message))
    , path_(std::move(path))
{}

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

// This is the critical async implementation of execute_async
// It uses co_await to asynchronously acquire the file lock, then writes to the file, and finally releases the lock.
// The ActionLoop will handle the scheduling and execution of this task, allowing it to run without blocking the main thread.
//
// However, std::ofstream would block the thread, so in a real implementation, you would want to use an asynchronous file I/O library or offload the file writing to a separate thread pool.
// Otherwise, the file writing will block the ActionLoop thread, defeating the purpose of using coroutines for asynchrony.
Task<void> LogAction::execute_async()
{
    co_await LogAction::g_file_mgr.lock(path_);

    std::ofstream file(path_, std::ios::app);
    if (file.is_open()) {
        file << message_ << "\n";
        file.flush();
    }

    LogAction::g_file_mgr.unlock(path_);
}
