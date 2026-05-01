# eBPF OOP Design

This project implements an object-oriented (OOP) design for loading and managing eBPF programs in user space, focusing on modularity, reusability, and asynchronous event handling. It provides a C++ wrapper that separates concerns between eBPF object loading, program attachment, and event processing through an action-based architecture.

## Why OOP Design?

Using an OOP design offers several advantages for eBPF program management:

- **Modularity**: Encapsulates eBPF loading, attachment, and event handling into reusable classes, making the code easier to maintain and extend.
- **Separation of Concerns**: Decouples event generation (from eBPF kernel programs) from event processing (user-space actions), allowing independent development and testing.
- **Reusability**: Common components like action queues and logging can be reused across different eBPF programs without duplication.
- **Asynchronous Processing**: Leverages coroutines for non-blocking event handling, improving performance and responsiveness in high-throughput scenarios.
- **Type Safety**: Strong typing in C++ reduces runtime errors and enhances code reliability compared to procedural approaches.

## Project Structure

- `CMakeLists.txt` — Build configuration for the project.
- `action/` — Core action loop implementation for asynchronous event processing.
  - `ActionLoop.hpp/cpp` — Singleton action dispatcher with thread-safe queue.
- `coroutine/` — Custom coroutine utilities for async operations.
  - `task.hpp` — Task class for coroutine-based asynchronous tasks.
  - `fire_forget.hpp` — Fire-and-forget coroutine wrapper.
  - `async_mutex.hpp` — Asynchronous mutex for thread-safe file operations.
- `ebpf/` — Base eBPF program wrapper.
  - `BpfProgram.hpp/cpp` — Abstract base class for eBPF program management.
- `log_action/` — Logging action implementation.
  - `LogAction.hpp/cpp` — Asynchronous logging action using coroutines.
- `xdp_drop/`, `socket_filter/`, `cgroup_egress/`, `syscall_trace/` — Specific eBPF program implementations, each containing:
  - `main.cpp` — Entry point for the loader.
  - `[program].bpf.c` — eBPF kernel program source.
  - `[program].hpp/cpp` — User-space program wrapper.
- `build/` — Build artifacts and compiled binaries.

## OOP Design: Event -> Action

The design follows an event-driven architecture where eBPF kernel programs generate events, which are processed asynchronously through actions in user space:

```
eBPF Kernel Program
        |
        | (generates events)
        v
Ring Buffer / Perf Event
        |
        | (polls events)
        v
User Space Loader
        |
        | (creates actions)
        v
ActionLoop (Queue)
        |
        | (dispatches asynchronously)
        v
IAction Implementations
        |
        | (execute tasks)
        v
Asynchronous Completion
```

This flow ensures that event processing is decoupled from event generation, allowing for scalable and responsive handling of eBPF events.

## Key Classes

### ActionLoop

`ActionLoop` is a singleton class that manages a thread-safe queue of actions and dispatches them asynchronously using a background thread. It leverages C++20 coroutines for efficient task scheduling.

```
ActionLoop (Singleton)
    |
    +-- pushAction(unique_ptr<IAction>) : FireForget
    |       |
    |       +-- Enqueues action to thread-safe queue
    |       +-- Notifies background thread
    |
    +-- Background Thread (pump())
            |
            +-- Waits for actions in queue
            +-- Dequeues and resumes IAction::execute_async()
            +-- Handles coroutine suspension/resumption
```

### LogAction

`LogAction` is a concrete implementation of `IAction` that handles asynchronous logging of eBPF events to files. It uses coroutines to perform I/O operations without blocking the main thread.

```
LogAction : IAction
    |
    +-- execute_async() : Task<void>
    |       |
    |       +-- Acquires async file lock (AsyncFileMutex)
    |       +-- Opens log file in append mode
    |       +-- Writes formatted message
    |       +-- Flushes and closes file
    |       +-- Releases lock
    |
    +-- Coroutine Design:
            |
            +-- co_await lock() -> Suspends until lock acquired
            +-- Perform I/O -> Non-blocking file operations
            +-- co_return -> Resumes caller, completes async
```

The coroutine design allows for efficient asynchronous I/O, where file operations can suspend and resume without tying up threads, enabling high-concurrency event processing.

## How to Build

1. Create a build directory:
   ```bash
   mkdir -p build
   cd build
   ```

2. Configure with CMake:
   ```bash
   cmake ..
   ```

3. Build the project:
   ```bash
   cmake --build .
   ```

4. (Optional) Install artifacts:
   ```bash
   sudo cmake --install .
   ```

## How to Run and Verify

1. **Run a program** (e.g., xdp_drop):
   ```bash
   cd build
   sudo ./xdp_drop_loader [interface]
   ```
   Replace `[interface]` with a network interface like `lo` or `eth0`.

2. **Verify operation**:
   - Check console output for successful loading and attachment.
   - Monitor log files (e.g., `/var/log/ebpf_oop_design/xdp_drop.events.log`) for event entries.
   - Use tools like `tcpdump` or `ping` to generate traffic and observe logged events.

3. **Stop the program**:
   - Press `Ctrl+C` to gracefully detach and exit.

Each program loader will output its status and log path. Events are processed asynchronously, ensuring minimal impact on system performance.
