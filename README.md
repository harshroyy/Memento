# Multithreaded HTTP Proxy with LRU Cache (C++)

A lightweight, multithreaded **HTTP forward proxy server** written in C++ using raw POSIX sockets. It accepts requests from a client (browser or `curl`), forwards them to the destination web server, relays the response back, and stores responses in a thread-safe **LRU (Least Recently Used) cache** so repeated requests are served instantly from memory.

Built as a learning project to understand sockets, DNS resolution, TCP streaming, concurrency, and cache design from the ground up, with no external libraries.

---

## Features

- **TCP proxy server** on port `8080` (IPv4)
- **HTTP request parsing**: extracts the target host from the `Host:` header
- **DNS resolution** via `getaddrinfo()`
- **Streaming relay**: forwards response data to the client chunk by chunk as it arrives
- **Multithreading**: one detached `std::thread` per client connection
- **LRU cache**: O(1) `get` and `put` using `unordered_map` + doubly linked list (`std::list`)
- **Thread safety**: cache access protected by `std::mutex` / `std::lock_guard`
- **Zero dependencies**: only the C++ standard library and POSIX headers

---

## How It Works

```
 ┌─────────┐   1. HTTP request    ┌──────────────┐   4. forward request   ┌────────────┐
 │ Client  │ ───────────────────► │  Proxy       │ ─────────────────────► │ Web Server │
 │(browser)│                      │  :8080       │                        │  (port 80) │
 │         │ ◄─────────────────── │              │ ◄───────────────────── │            │
 └─────────┘   6. response        └──────┬───────┘   5. response chunks   └────────────┘
                                         │
                              2. check   │   3. miss → DNS lookup + connect
                                         ▼
                                  ┌─────────────┐
                                  │  LRU Cache  │  (capacity: 10)
                                  └─────────────┘
```

**Request lifecycle**

1. The server `accept()`s a client and spawns a worker thread running `handle_client()`.
2. The thread reads the raw HTTP request with `recv()` and extracts the hostname from the `Host:` header.
3. **Cache lookup**
   - **HIT** → the stored response is sent straight to the client; no network call is made.
   - **MISS** → continue to the next steps.
4. The hostname is resolved to an IPv4 address (`getaddrinfo`) and a TCP connection to port 80 is opened.
5. The original request is forwarded; the response is read in 4 KB chunks, **relayed to the client immediately**, and **accumulated** in a buffer.
6. The full response is stored in the LRU cache, and both sockets are closed.

**LRU cache internals**

| Component | Purpose |
|---|---|
| `list<string> lru_list` | Orders keys by recency. Front = most recently used, back = least recently used |
| `unordered_map<string, pair<string, list<string>::iterator>> cache_map` | Maps key → (cached data, iterator to its list node) for O(1) lookup and O(1) move-to-front via `splice` |
| `mutex cache_lock` | Prevents data races when multiple client threads touch the cache |

When the cache is full, the entry at the back of the list (the least recently used) is evicted before a new one is inserted.

---

## Project Structure

```
.
├── proxy.cpp     # Entire implementation (server, parser, LRU cache)
└── README.md
```

Key pieces inside `proxy.cpp`:

| Symbol | Role |
|---|---|
| `LRUCache` | Thread-safe LRU cache class (`get`, `put`) |
| `extract_host()` | Parses the `Host:` header from a raw HTTP request |
| `handle_client()` | Per-connection logic: cache check, DNS, connect, relay, cache store |
| `main()` | Creates the listening socket, binds, listens, and accepts clients in a loop |

---

## Requirements

- Linux or macOS (POSIX sockets; **not** Windows without WSL)
- A C++11-or-newer compiler (`g++` or `clang++`)
- `pthread` support (standard on Linux/macOS)

---

## Build

```bash
g++ -std=c++17 -pthread -Wall -Wextra -o proxy proxy.cpp
```

## Run

```bash
./proxy
```

Expected output:

```
Server is listening on port 8080
```

---

## Usage / Testing

### With `curl`

Use `-x` to route the request through the proxy. Add `Connection: close` so the server closes the connection when the response is complete (see [Known Limitations](#known-limitations)).

```bash
curl -x http://localhost:8080 http://example.com -H "Connection: close"
```

Run it **twice**. The first request logs a cache miss, and the second one is served from memory:

```
Cache Miss! Fetching from the internet . . .
...
Finished relaying and caching data from example.com!

Cache HIT! Serving example.com from memory.
```

### With a browser

Set your browser's (or OS's) **HTTP proxy** to `127.0.0.1` port `8080`, then visit a plain-HTTP site such as `http://example.com`.

> HTTPS sites will not work. See below.

---

## Configuration

Currently set through constants in the source:

| Setting | Location | Default |
|---|---|---|
| Listening port | `#define PORT` | `8080` |
| Cache capacity (number of entries) | `LRUCache proxy_cache(10);` | `10` |
| Listen backlog | `listen(server_fd, 10)` | `10` |
| Request buffer size | `char buffer[4096]` | 4 KB |

---

## Known Limitations

This is an educational project, so it deliberately keeps things simple. Be aware of the following:

- **HTTP only.** HTTPS requires handling the `CONNECT` method and tunnelling encrypted traffic, which is not implemented.
- **Cache key is the hostname only**, not the full URL. Requests for different paths on the same host (e.g. `/a` and `/b`) will currently map to the same cache entry. Using the request line (method + full URL) as the key is the natural fix.
- **No cache validation or expiry.** There is no TTL, and `Cache-Control`, `Expires`, `ETag`, and non-`GET` methods are ignored. Error responses are cached too.
- **Connection handling.** The proxy reads the upstream response until the server closes the connection. Keep-alive connections can therefore make a request appear to hang; use `Connection: close` when testing.
- **Single `recv()` for the request.** Requests larger than 4 KB or split across TCP segments are not fully read.
- **One thread per connection**, with no thread pool or connection limit.
- **Whole responses are buffered in memory** for caching, so very large responses use a lot of RAM.
- **Shared mutable state in a thread-per-request model.** Only the cache is protected; `cout`/`cerr` output from different threads can interleave.
- **`send()` return values are not checked**, and partial sends or `SIGPIPE` on closed client sockets are not handled.

---

## Ideas for Future Improvements

- [ ] Use the full URL (method + path + host) as the cache key
- [ ] HTTPS support via `CONNECT` tunnelling
- [ ] Thread pool instead of detached thread-per-client
- [ ] TTL-based expiry and `Cache-Control` awareness
- [ ] Only cache `200 OK` `GET` responses
- [ ] Robust request parsing (read until `\r\n\r\n`, handle headers properly)
- [ ] Max cache size in bytes, not just entry count
- [ ] Non-blocking I/O with `epoll`/`select`
- [ ] Configurable port and capacity via command-line arguments
- [ ] Graceful shutdown on `SIGINT`
- [ ] Logging with timestamps and log levels

---

## Concepts Covered

`socket()` · `bind()` · `listen()` · `accept()` · `recv()` · `send()` · `connect()` · `getaddrinfo()` · `setsockopt(SO_REUSEADDR)` · TCP stream chunking · `std::thread` · `std::mutex` · `std::list::splice` · LRU eviction · HTTP request structure

---

## License

This project is provided for educational purposes. Add a license of your choice (e.g. MIT) before distributing.
