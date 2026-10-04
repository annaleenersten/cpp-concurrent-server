# C++ Concurrent Cache Server

A concurrent caching server built from scratch in C++ using TCP sockets, HTTP, epoll, multithreading, and thread-safe data structures.

## Features

* TCP socket creation, configuration, binding, and listening
* Nonblocking TCP sockets
* Linux `epoll`-based event handling
* Concurrent client handling using a custom thread pool
* HTTP/1.1 request parsing
* HTTP request method, path, version, headers, and body parsing
* HTTP request validation
* HTTP error responses (`400`, `404`, `405`)
* Request routing
* Thread-safe in-memory caching
* PUT, GET, and DELETE operations
* TTL-based cache expiration
* LRU cache eviction
* Configurable cache capacity
* Cache hit and miss statistics
* `/stats` endpoint for cache statistics
* Handling of partial TCP reads and larger request bodies
* Graceful server shutdown
* Unit and integration testing with GoogleTest

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

The `/stats` endpoint reports the number of cache hits and misses.

## Cache Behavior

Each cache entry has a time-to-live (TTL). Expired entries are removed when accessed.

The cache uses **Least Recently Used (LRU)** eviction when it reaches its maximum capacity. Accessing an entry moves it to the front of the LRU list, while the least recently used entry is removed when space is needed.

The cache tracks:

* Cache hits
* Cache misses

### Request Flow

```text
Client
  |
  v
TCP Socket
  |
  v
epoll Event Loop
  |
  v
HTTP Request
  |
  v
Thread Pool
  |
  v
Router
  |
  v
Cache
  |
  v
HTTP Response
```

The server uses `epoll` to monitor the listening socket and connected client sockets. Client sockets are nonblocking, allowing the server to handle partial TCP reads without blocking worker threads.

Once a complete HTTP request has been received, it is passed to the thread pool for request processing.

```text
                         epoll
                           |
              +------------+------------+
              |                         |
       Listening Socket           Client Sockets
              |                         |
           accept()                  EPOLLIN
                                        |
                                      recv()
                                        |
                              Request complete?
                                /          \
                              No            Yes
                              |              |
                         Wait for more   Thread Pool
                           data              |
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

## Running the Server

Build the project from the project root:

```bash
cmake -S . -B build
cmake --build build
```

Start the server:

```bash
./build/server
```

The server listens on port `8080`.

In another terminal, send requests using `curl`:

```bash
curl http://localhost:8080/hello
```

Store and retrieve a value:

```bash
curl -X PUT http://localhost:8080/cache/name -d "Annalee"

curl http://localhost:8080/cache/name
```

Stop the server with:

```text
Ctrl+C
```

The server performs a graceful shutdown when interrupted.

## Testing

The project uses GoogleTest for automated unit and integration testing.

> **Note:** The server must be running on port `8080` before running the integration tests. Start it in a separate terminal with `./build/server`.

Build and run the tests with:

```bash
cmake --build build
cd build
ctest --output-on-failure
```

The test suite currently contains **54 tests** covering:

### Cache

* Storage and retrieval
* Updating existing values
* Missing keys
* Deletion
* TTL expiration
* LRU eviction
* LRU order updates
* Cache hit/miss tracking

### HTTP Request Parsing

* Request line parsing
* HTTP headers
* Request bodies
* Content-Length
* Missing headers
* Malformed requests
* Unsupported HTTP versions

### HTTP Responses

* Status information
* Not found responses
* Empty response bodies

### Routing

* Root, hello, and about routes
* Unknown routes
* PUT, GET, and DELETE operations
* Missing cache keys
* Invalid requests
* Unsupported methods
* Cache statistics

### Thread Pool

* Task execution
* Multiple tasks

### Integration

* PUT → GET
* Missing keys
* DELETE → GET
* Unsupported methods
* Large request bodies
* Multiple client connections
* Empty cache keys
* Unknown routes
* Invalid HTTP versions
* Malformed requests
* Empty values
* Updating existing values
* Partial TCP requests
* Multiple concurrent clients
* Idle clients not blocking active requests

## Future Improvements

* Configurable TTL through HTTP requests
* More robust HTTP validation and error handling
* `EPOLLOUT`-based response handling for fully event-driven socket writes
* Performance benchmarking
* Benchmark different thread-pool sizes
* Measure request throughput and latency
* Additional cache performance metrics
* Document benchmark and performance results