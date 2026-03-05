# Multithreaded HTTP Server

A high-performance, concurrent C-based web server designed to handle multiple simultaneous client requests using a worker-pool architecture. This project emphasizes thread safety, efficient resource management, and atomic file operations.

## Features

- **Worker-Thread Pool**: Utilizes a producer-consumer model with a thread-safe queue to manage and distribute incoming connections across a pool of worker threads.
- **Thread-Safe Synchronization**: Implements mutexes and condition variables to ensure data integrity within the shared request queue and audit logging.
- **Advisory File Locking**: Uses `flock` to manage concurrent access to files, preventing race conditions between simultaneous `GET` and `PUT` requests.
- **Modular Design**: Separates core server logic from low-level networking utilities and data structures for better maintainability.

## Architecture

- **`httpserver.c`**: The main driver containing the dispatcher loop and request handlers.
- **`queue.c`**: A thread-safe FIFO queue implementation for managing connection file descriptors.
- **`net_utils.h`**: A custom networking library providing abstraction for socket initialization and request parsing.
- **`libnet_utils.a`**: Pre-compiled binary utility library for optimized network communication.

## Build and Run

### Requirements
- Clang compiler
- POSIX-compliant environment (Linux/macOS)
- Pthreads library

### Installation
To compile the server using the provided Makefile:
```bash
make
```

### Usage
Start the server by specifying the number of worker threads and the target port:
```bash
./httpserver [-t threads] <port>
```
- -t threads: (Optional) The number of worker threads to spawn (default: 4).
- <port>: The port number for the server to listen on.
