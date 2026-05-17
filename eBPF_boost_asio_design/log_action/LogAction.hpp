#pragma once

#include <string>

#include <boost/asio/awaitable.hpp>

#include "../action/ActionLoop.hpp"

class LogAction final : public IAction {
public:
    LogAction(std::string message, std::string path);

    boost::asio::awaitable<void> execute_async() override;

    static bool ensure_log_directory(const std::string& path) noexcept;

private:
    std::string message_;
    std::string path_;
};
