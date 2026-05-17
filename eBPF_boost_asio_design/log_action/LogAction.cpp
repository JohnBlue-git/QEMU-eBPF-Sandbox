#include <filesystem>
#include <iostream>
#include <utility>

#include <boost/asio/buffer.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include "../coroutine/async_mutex.hpp"
#include "LogAction.hpp"

namespace asio = boost::asio;

AsyncFileStreamManager g_file_mgr;

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

// The ActionLoop will handle the scheduling and execution of this task
// , allowing it to run without blocking the main thread.
asio::awaitable<void> LogAction::execute_async()
{
    // We acquire a locked stream to the log file using the AsyncFileStreamManager.
    auto locked_stream = co_await g_file_mgr.acquire_stream(path_);
    if (!locked_stream) {
        co_return;
    }

    // We use Boost.Asio's asynchronous write operation
    //  to write the log message to the file without blocking the thread.
    std::string payload = message_ + "\n";
    boost::system::error_code ec;
    co_await asio::async_write(
        locked_stream.stream(),
        asio::buffer(payload),
        asio::redirect_error(asio::use_awaitable, ec)
    );
    // We use redirect_error to capture any errors that occur during the asynchronous write operation
    //  and prevent them from throwing exceptions, which allows us to handle errors gracefully without crashing the ActionLoop.
    if (ec) {
        std::cerr << "async_write failed for " << path_ << ": " << ec.message() << " (" << ec.value() << ")\n";
    }
}
