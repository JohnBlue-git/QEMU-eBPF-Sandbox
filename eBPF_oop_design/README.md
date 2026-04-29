# eBPF OOP Design

This folder contains a C++ wrapper design for a user-space loader that loads an XDP eBPF object and dispatches event logging through an action queue.

## Structure

- `CMakeLists.txt` — build project for the new design
- `include/` — C++ headers for the OOP wrapper
- `src/` — C++ implementation files
- `xdp_drop/` — kernel-side eBPF program source

## Build

```bash
mkdir -p eBPF_oop_design/build
cd eBPF_oop_design/build
cmake ..
cmake --build .
```

## Install

To install artifacts under `/opt/ebpf_oop_design`:

```bash
sudo cmake --install .
```

## Runtime behavior

- The loader searches for `xdp_drop.bpf.o` in `/opt/ebpf_oop_design`, `./`, and `build/`.
- Event logs are written to `/var/log/ebpf_oop_design/xdp_drop.events.log`.
- `ActionLoop` dispatches `IAction` implementations through a thread-safe queue.
- `LoggingAction` packages event text and writes it asynchronously via `LogWriter`.

## Notes

The new design intentionally separates object loading from attaching by placing the attach logic inside `loadFilter()` rather than the constructor.
