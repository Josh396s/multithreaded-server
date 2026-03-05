# Multithreaded HTTP Server

A concurrent web server implemented in C that utilizes a worker-thread pool architecture to handle multiple HTTP client requests simultaneously. This project demonstrates advanced systems programming concepts, including manual socket management, thread-safe synchronization, and atomic file I/O.

## Core Components

- **Thread-Pool Architecture**: Uses a dispatcher thread to accept connections and a pool of worker threads to process requests (GET and PUT).
- **Thread-Safe Queue**: A custom FIFO queue using mutexes and condition variables to manage workload distribution.
- **Atomic File Access**: Implements advisory file locking (`flock`) to prevent race conditions during concurrent read/write operations.
- **Manual HTTP Parsing**: Custom-built request parsing logic to handle URIs, methods, and headers without external libraries.

## Building and Running

### Build
```bash
make
```

### Usage
```bash
./httpserver [-t threads] <port>
```
- -t threads: Set the number of worker threads (default is 4).
- <port>: The port the server will listen on.

### Technical Details
- Languages: C
- Libraries: Pthreads (POSIX Threads)
- Concurrency Model: Producer-Consumer
