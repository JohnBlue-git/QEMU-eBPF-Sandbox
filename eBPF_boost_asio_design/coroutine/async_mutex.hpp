#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <fcntl.h>
#include <unistd.h>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>

class AsyncFileStreamManager {
private:
    struct FileState {
        bool locked = false;
        std::deque<boost::asio::any_completion_handler<void()>> waiters;
    };

    std::unordered_map<std::string, std::shared_ptr<boost::asio::posix::stream_descriptor>> stream_cache_;
    std::unordered_map<std::string, FileState> file_states_;
    std::mutex mtx_;

    template <typename CompletionToken = boost::asio::use_awaitable_t<>>
    auto lock(std::string filename, CompletionToken&& token = CompletionToken{})
    {
        return boost::asio::async_initiate<CompletionToken, void()>(
            [this, filename = std::move(filename)](auto handler) mutable {
                std::optional<boost::asio::any_completion_handler<void()>> complete_now;
                {
                    std::lock_guard lock(mtx_);

                    auto& state = file_states_[filename];
                    if (!state.locked) {
                        state.locked = true;
                        complete_now.emplace(std::move(handler));
                    } else {
                        state.waiters.emplace_back(std::move(handler));
                    }
                }

                if (complete_now) {
                    (*complete_now)();
                }
            },
            token);
    }

    void unlock(const std::string& filename)
    {
        std::optional<boost::asio::any_completion_handler<void()>> resume_next;
        {
            std::lock_guard lock(mtx_);
            auto it = file_states_.find(filename);
            if (it == file_states_.end()) {
                return;
            }

            auto& state = it->second;
            if (!state.waiters.empty()) {
                resume_next.emplace(std::move(state.waiters.front()));
                state.waiters.pop_front();
                // Keep locked=true: ownership transfers directly to resumed waiter.
            } else {
                state.locked = false;
                file_states_.erase(it);
            }
        }

        if (resume_next) {
            (*resume_next)();
        }
    }

public:
    class LockedStream {
    public:
        LockedStream() = default;

        LockedStream(AsyncFileStreamManager* owner, std::string filename,
            std::shared_ptr<boost::asio::posix::stream_descriptor> stream)
            : owner_(owner)
            , filename_(std::move(filename))
            , stream_(std::move(stream))
        {}

        LockedStream(const LockedStream&) = delete;
        LockedStream& operator=(const LockedStream&) = delete;

        LockedStream(LockedStream&& other) noexcept
            : owner_(std::exchange(other.owner_, nullptr))
            , filename_(std::move(other.filename_))
            , stream_(std::move(other.stream_))
        {}

        LockedStream& operator=(LockedStream&& other) noexcept
        {
            if (this != &other) {
                release();
                owner_ = std::exchange(other.owner_, nullptr);
                filename_ = std::move(other.filename_);
                stream_ = std::move(other.stream_);
            }
            return *this;
        }

        ~LockedStream()
        {
            release();
        }

        explicit operator bool() const noexcept
        {
            return static_cast<bool>(stream_);
        }

        boost::asio::posix::stream_descriptor& stream() const
        {
            return *stream_;
        }

    private:
        void release()
        {
            if (owner_ != nullptr) {
                owner_->unlock(filename_);
                owner_ = nullptr;
            }
        }

        AsyncFileStreamManager* owner_ = nullptr;
        std::string filename_;
        std::shared_ptr<boost::asio::posix::stream_descriptor> stream_;
    };

    boost::asio::awaitable<LockedStream> acquire_stream(const std::string& filename)
    {
        co_await this->lock(filename);

        std::shared_ptr<boost::asio::posix::stream_descriptor> stream_ptr;
        {
            std::lock_guard lock(mtx_);
            auto it = stream_cache_.find(filename);
            if (it != stream_cache_.end()) {
                stream_ptr = it->second;
            }
        }

        if (!stream_ptr) {
            auto ex = co_await boost::asio::this_coro::executor;
            const int fd = ::open(filename.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) {
                this->unlock(filename);
                co_return LockedStream{};
            }

            stream_ptr = std::make_shared<boost::asio::posix::stream_descriptor>(ex, fd);
            std::lock_guard lock(mtx_);
            auto [it, inserted] = stream_cache_.emplace(filename, stream_ptr);
            if (!inserted) {
                stream_ptr = it->second;
            }
        }

        co_return LockedStream(this, filename, std::move(stream_ptr));
    }
};
