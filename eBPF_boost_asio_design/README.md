# eBPF Boost.Asio Design

This project implements an event-driven design for loading and managing eBPF programs in user space using **Boost.Asio** as the async execution engine. It combines the modularity benefits of OOP design with the production-grade async capabilities of Boost.Asio, providing superior performance and scalability for high-throughput eBPF event processing.

## Why Boost.Asio Design?

Building on the OOP principles while leveraging Boost.Asio offers significant advantages:

- **Production-Grade Async Framework**: Boost.Asio is a battle-tested, industry-standard library used in many production systems for async I/O and coroutine-based programming.
- **Superior Performance**: Boost.Asio's io_context provides optimized event multiplexing (epoll/kqueue/IOCP), reducing CPU overhead compared to manual thread management.
- **Stream Resource Caching**: Built-in stream descriptor caching improves performance by reusing file handles across multiple async operations.
- **Flexible Completion Handlers**: Multiple completion token support (callbacks, futures, coroutines) enables seamless integration with different async patterns.
- **Scalability**: Handles thousands of concurrent operations efficiently without the overhead of dedicated threads per operation.
- **Cross-Platform**: Boost.Asio abstracts platform-specific async I/O (epoll on Linux, IOCP on Windows), making code portable.
- **Rich Ecosystem**: Integrates with other Boost libraries and is widely adopted in production middleware.

## Comparison with OOP Design

Both designs share the same OOP principles and event-driven architecture, but differ in their async execution engine:

| Feature | OOP Design | Boost.Asio Design |
|---------|-----------|------------------|
| **Coroutine Engine** | Custom C++20 coroutines | Boost.Asio awaitables |
| **Event Multiplexing** | Manual condition_variable + queue | io_context (epoll/kqueue) |
| **Thread Model** | Single pump thread | io_context thread pool |
| **File Locking** | AsyncFileMutex (simple) | AsyncFileStreamManager (cached) |
| **Stream Caching** | No | Yes (automatic) |
| **Completion Handlers** | Direct coroutine resumption | Boost.Asio completion tokens |
| **Dependencies** | Only libbpf | libbpf + Boost.Asio + OpenSSL |
| **Learning Curve** | Simpler, educational | Steeper, production-ready |
| **Use Case** | Learning, simple apps | High-throughput, production systems |

## Project Structure

- `CMakeLists.txt` — Build configuration for Boost.Asio and eBPF tooling.
- `actions/` — Boost.Asio-based action loop and logging action implementation.
  - `ActionLoop.hpp/cpp` — Singleton dispatcher using io_context and executor work guards.
  - `LogAction.hpp/cpp` — Async logging writer using Boost.Asio.
- `coroutine/` — Boost.Asio async utilities.
  - `async_mutex.hpp` — AsyncFileStreamManager for concurrent file access with stream caching.
- `xdp_drop/`, `socket_filter/`, `cgroup_egress/`, `syscall_trace/` — Specific eBPF program implementations, each containing:
  - `main.cpp` — Entry point for the loader.
  - References eBPF kernel programs and wrappers from `../eBPF_oop_design/`.
- `build/` — Build artifacts and compiled binaries.

## Boost.Asio Architecture

The design uses Boost.Asio's `io_context` as the central async scheduler, managing event dispatch and coroutine resumption:

```
eBPF Kernel Program
        |
        | (generates events)
        v
Ring Buffer / Perf Event
        |
        | (polls events)
        v
User Space Loader (Boost.Asio threads)
        |
        | (creates actions)
        v
ActionLoop (io_context-based dispatcher)
        |
        | (schedules with io_context)
        v
Boost.Asio Executor Pool
        |
        | (multiplexes operations)
        v
LogAction (awaitable coroutine)
        |
        | (async_write via stream descriptor)
        v
Asynchronous File I/O Completion
```

This architecture ensures efficient handling of thousands of concurrent events with minimal thread overhead.

## Key Classes

### ActionLoop

`ActionLoop` is a singleton that wraps Boost.Asio's `io_context`, providing a single entry point for asynchronous action dispatch. It uses a work guard to prevent the io_context from exiting until all pending work is complete.

```
ActionLoop (Singleton)
    |
    +-- io_context (event multiplexer)
    |
    +-- work_guard (keeps io_context alive)
    |
    +-- Background Thread (io_context.run())
    |
    +-- pushAction(unique_ptr<IAction>) : void
            |
            +-- post() action execution to io_context
            +-- io_context dispatches via executor
            +-- Action::execute_async() runs as awaitable
```

**Key Differences from OOP Design**:
- Uses `boost::asio::io_context` instead of manual queue + condition_variable
- Leverages `executor_work_guard` to manage lifecycle
- `post()` operations instead of manual queue management
- Single thread runs `io_context.run()`, which handles event multiplexing internally

### AsyncFileStreamManager

Manages thread-safe file access with automatic stream descriptor caching and queuing of concurrent writers. Unlike the OOP design's simple AsyncFileMutex, this provides production-grade concurrent access patterns:

```
AsyncFileStreamManager (Global Singleton)
    |
    +-- stream_cache_ : map<filename, shared_ptr<stream_descriptor>>
    |       |
    |       +-- Caches open file handles for reuse
    |       +-- Reduces open/close syscalls
    |
    +-- file_states_ : map<filename, FileState>
    |       |
    |       +-- Tracks lock ownership per file
    |       +-- Queues waiting coroutines (deque<completion_handler>)
    |
    +-- acquire_stream(filename)
            |
            +-- co_await lock(filename)
            +-- Returns LockedStream with cached descriptor
            +-- On destruction: co_await unlock() resumes next waiter
```

**Features**:
- Automatic stream descriptor pooling
- FIFO queue for fair scheduling of waiting writers
- Lock-free read paths where possible
- Seamless integration with Boost.Asio's completion handlers

### LogAction

`LogAction` implements asynchronous file I/O using Boost.Asio's `async_write` and awaitable coroutines:

```
LogAction : IAction
    |
    +-- execute_async() : boost::asio::awaitable<void>
    |       |
    |       +-- co_await g_file_mgr.acquire_stream(path)
    |       +-- co_await asio::async_write(stream, buffer, ...)
    |       +-- Handles errors gracefully via redirect_error
    |
    +-- Boost.Asio Benefits:
            |
            +-- async_write handles partial writes
            +-- Error redirection prevents exceptions
            +-- Uses platform-optimal I/O (epoll on Linux)
            +-- Stream caching improves throughput
```

## Building and Dependencies

### System Dependencies

Boost.Asio design requires additional dependencies beyond the OOP design:

```bash
# Build dependencies
sudo apt-get install -y \
  libboost-all-dev \
  libssl-dev \
  cmake \
  clang \
  llvm \
  linux-headers-$(uname -r) \
  libbpf-dev \
  libelf-dev \
  zlib1g-dev
```

### Build Steps

1. Create a build directory:
   ```bash
   mkdir -p eBPF_boost_asio_design/build
   cd eBPF_boost_asio_design/build
   ```

2. Configure with CMake:
   ```bash
   cmake ..
   ```

3. Build the project:
   ```bash
   cmake --build . -- -j1
   ```

   > **Note**: Due to container resource constraints, use `-j1` if you encounter linker crashes (signal 15). The project links against Boost libraries, which can be memory-intensive.

4. (Optional) Install artifacts:
   ```bash
   sudo cmake --install .
   ```

### Build Configuration

The CMakeLists.txt automatically:
- Configures Boost.Asio with C++20 coroutine support
- Sets up libbpf and elfutils linking
- Handles cross-directory includes for eBPF kernel programs from `../eBPF_oop_design/`

## How to Run and Verify

### Basic Usage

1. **Run a program** (e.g., xdp_drop):
   ```bash
   cd eBPF_boost_asio_design/build
   sudo ./xdp_drop_loader [interface]
   ```
   Replace `[interface]` with a network interface like `lo` or `eth0`.

2. **Verify operation**:
   - Check console output for successful loading and attachment
   - Monitor log files (e.g., `/var/log/ebpf_boost_asio_design/xdp_drop.events.log`)
   - Use `tcpdump` or `ping` to generate traffic and observe logged events

3. **Stop the program**:
   - Press `Ctrl+C` to gracefully detach and exit

### Performance Verification

Boost.Asio design is optimized for high-throughput scenarios. To verify performance improvements:

1. **Generate traffic**:
   ```bash
   # In another terminal, run ping to generate continuous traffic
   ping -f 127.0.0.1  # or another host
   ```

2. **Monitor resource usage**:
   ```bash
   top -p $(pidof xdp_drop_loader)
   ```
   Observe that CPU usage remains low due to epoll-based event multiplexing.

3. **Check log throughput**:
   ```bash
   tail -f /var/log/ebpf_boost_asio_design/xdp_drop.events.log | wc -l
   ```
   Count lines per second to measure logging throughput.

### Debugging

Enable verbose logging to troubleshoot:

```bash
# Set debug level in CMake configuration
cmake .. -DCMAKE_BUILD_TYPE=Debug

# View libbpf debug output
LIBBPF_DEBUG=1 sudo ./xdp_drop_loader lo

# Check kernel logs for eBPF errors
sudo dmesg | tail -20 | grep -i ebpf
```

## Advanced Topics

### Custom Actions

To create a custom action using Boost.Asio awaitables:

1. Inherit from `IAction`
2. Implement `execute_async()` returning `boost::asio::awaitable<void>`
3. Use `co_await` for async operations:
   ```cpp
   class MyAction : public IAction {
   public:
       boost::asio::awaitable<void> execute_async() override {
           co_await asio::post(asio::use_awaitable);
           // Perform work
           co_return;
       }
   };
   ```

### Thread Pool Configuration

To modify the io_context thread model:

1. Edit `actions/ActionLoop.cpp`
2. Add multiple threads to `io_context.run()`:
   ```cpp
   threads_.emplace_back([&] { io_context_.run(); });  // Repeat for multiple threads
   ```
3. Rebuild and test concurrent event handling

## Troubleshooting

### Build Failures

**Error: "signal 15 (SIGTERM) during linking"**
- Cause: Insufficient memory for parallel linking
- Fix: Use `-j1` in cmake build: `cmake --build . -- -j1`

**Error: "boost/asio/awaitable.hpp: No such file or directory"**
- Cause: Boost.Asio not installed
- Fix: `sudo apt-get install libboost-all-dev`

### Runtime Issues

**Program hangs after loading eBPF program**
- Check: `sudo ps aux | grep xdp_drop_loader`
- If stalled: `Ctrl+C` to send SIGINT; ActionLoop should gracefully exit
- Review: kernel logs for eBPF errors: `sudo dmesg | tail -20`

**Events not being logged**
- Verify eBPF program attachment: `sudo bpftool prog list`
- Check log directory exists: `ls -la /var/log/ebpf_boost_asio_design/`
- Ensure permission to write: `sudo touch /var/log/ebpf_boost_asio_design/test.log`

**High CPU usage despite Boost.Asio**
- Review: Check if eBPF program is filtering correctly or generating too many events
- Tuning: Adjust ring buffer size or sampling rate in eBPF kernel program

## Related Documentation

- [eBPF OOP Design](../eBPF_oop_design/README.md) — Custom coroutine-based async design
- [eBPF Basic Design](../eBPF_basic_design/README.md) — Procedural C implementation
- [Boost.Asio Documentation](https://www.boost.org/doc/libs/master/doc/html/boost_asio.html)
- [eBPF Linux Kernel Documentation](https://docs.kernel.org/bpf/)
