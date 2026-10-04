# C++ Concurrent Cache Server

A concurrent caching server built from scratch in C++ using TCP sockets, HTTP/1.1, Linux `epoll`, multithreading, and thread-safe data structures.

## Features

* TCP socket creation, configuration, binding, and listening
* Nonblocking TCP sockets
* Linux epoll-based event handling
* Concurrent client handling using a custom thread pool
* HTTP/1.1 request parsing
* HTTP request method, path, version, headers, and body parsing
* HTTP request validation
* HTTP error responses (400, 404, 405)
* Request routing
* Thread-safe in-memory caching
* PUT, GET, and DELETE operations
* TTL-based cache expiration
* LRU cache eviction
* Configurable cache capacity
* Cache hit and miss statistics
* /stats endpoint for cache statistics
* Handling of partial TCP reads and larger request bodies
* Graceful server shutdown
* Unit and integration testing with GoogleTest

## Architecture

```text id="c8f1wk"
Client
  |
  v
TCP Connection
  |
  v
Server
  |
  v
epoll
  |
  +------------------+
  |                  |
  v                  v
accept()           recv()
                     |
                     v
              Complete Request
                     |
                     v
                Thread Pool
                     |
                     v
                HTTP Parser
                     |
                     v
                  Router
                     |
                     v
                  Cache
                     |
                     v
              HTTP Response
                     |
                     v
                  Client
```

The server uses `epoll` to monitor the listening socket and connected clients without blocking worker threads on network I/O. Partial requests are stored until the complete HTTP request has been received, then the request is passed to the thread pool for processing.

## API

### Store a value

```bash
curl -X PUT http://localhost:8080/cache/name -d "Annalee"
```

### Retrieve a value

```bash
curl http://localhost:8080/cache/name
```

### Delete a value

```bash
curl -X DELETE http://localhost:8080/cache/name
```

### View cache statistics

```bash
curl http://localhost:8080/stats
```

## Cache Behavior

Each entry has a configurable cache capacity and a time-to-live (TTL).

The cache uses **Least Recently Used (LRU)** eviction when its maximum capacity is reached. Accessing an entry moves it to the front of the LRU list, while the least recently used entry is removed when space is needed.

The cache also tracks cache hits and misses.

## Running the Server

Build the project:

```bash
cmake -S . -B build
cmake --build build
```

Start the server:

```bash
./build/server
```

The server listens on port `8080`.

Test the server with:

```bash
curl http://localhost:8080/hello
```

Stop the server with `Ctrl+C`.

## Testing

The project uses GoogleTest for unit and integration testing.

> Note: The server must be running on port 8080 before running the integration tests. Start it in a separate terminal with ./build/server.

Run the tests from the build directory:

```bash
cd build
ctest --output-on-failure
```

The current test suite contains **54 tests** covering:

* Cache behavior and eviction
* HTTP request parsing and validation
* HTTP responses
* Routing
* Thread pool behavior
* Partial TCP requests
* Multiple concurrent clients
* Idle client connections
* End-to-end HTTP operations

## Future Improvements

* Configurable TTL through HTTP requests
* More robust HTTP validation and error handling
* `EPOLLOUT`-based response handling
* Performance benchmarking
* Additional cache performance metrics