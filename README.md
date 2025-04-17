# KTP (KGP Transport Protocol) Library

This repository contains the implementation of **KTP**, a custom reliable, message‑oriented transport protocol built on top of UDP. It guarantees in‑order delivery, loss resilience, and flow control using a sliding‑window mechanism. The library mimics standard BSD socket APIs and is packaged as a static library (`libksocket.a`).

## Repository Structure
```
├── Makefile               # Top‑level build script for library and executables
├── ksocket.h              # Public header defining API, constants (WINDOW_SIZE, T, p, etc.)
├── ksocket.c              # Core implementation of KTP socket functions
├── initksocket.c          # Daemon to initialize shared memory, semaphores, and spawn threads
├── user1.c,...,user6.c    # Sample sender/receiver programs demonstrating file transfer
├── launch.sh              # macOS script to open multiple Terminal windows for testing
├── documentation.md       # High‑level design, data structures, and experimental results
├── libksocket.a           # Prebuilt static library (after running `make`)
└── README.md              # This file
```

## Prerequisites
- UNIX‑like OS (Linux, macOS)
- GCC or Clang toolchain
- POSIX threads, shared memory, and semaphores support
- `osascript` on macOS (for `launch.sh`)

## Configuration
- **T**: Retransmission timeout in seconds (default `5`) — defined in `ksocket.h`
- **p**: Packet drop probability (0.0–1.0) — defined in `ksocket.h`
- **WINDOW_SIZE**: Max outstanding messages (default `10`) — defined in `ksocket.h`

## Building
```bash
# Generate static library and executables
make all
```

This will produce:
- `libksocket.a`
- `initksocket` daemon
- `user1` through `user6` test programs

## Usage
1. **Start the KTP daemon** (in one terminal):
   ```bash
   ./initksocket
   ```
   This sets up shared memory, semaphores, and launches the sender (S) and receiver (R) threads.

2. **Run sender/receiver**
   - In separate terminals, launch pairs of `userX` programs. For example:
     ```bash
     # Terminal A
     ./user1  # acts as sender, reads input file and uses k_sendto()

     # Terminal B
     ./user2  # acts as receiver, uses k_recvfrom() and writes output file
     ```

3. **Automated launch** (macOS):
   ```bash
   ./launch.sh
   ```
   Opens six terminals arranged in a 2×3 grid running `user1`–`user6` for load testing.

## Testing & Benchmarking
- Vary packet drop probability `p` in `ksocket.h` (e.g., 0.05 to 0.5) and rebuild.
- Use your own files (>100 KB) to measure average retransmissions per message.
- Results and methodology are documented in `documentation.md`.

## API Summary
- `int k_socket(int domain, int type, int protocol);`
- `int k_bind(int sock_fd, char *src_ip, int src_port, char *des_ip, int des_port);`
- `int k_sendto(int sock_fd, const void *buf, size_t len, int flags, const struct sockaddr *dest, socklen_t addrlen);`
- `int k_recvfrom(int sock_fd, void *buf, size_t len, int flags, struct sockaddr *src, socklen_t *addrlen);`
- `int k_close(int sock_fd);`
