# RelayX 🚀

A lightweight, low-level HTTP forward proxy server written in C++ using POSIX network sockets.

## 📌 Features

- **Socket Programming**: Implemented from scratch using POSIX sockets (`sys/socket.h`, `netinet/in.h`).
- **Host Extraction**: Parses raw HTTP request streams to extract target host information.
- **DNS Resolution**: Dynamically resolves target domain names to IPv4 addresses using `getaddrinfo`.
- **Traffic Forwarding & Relaying**: Connects to the target web server, forwards client requests, and streams response chunks back to the client.
- **Port Reuse**: Configured with `SO_REUSEADDR` to enable fast server restarts without port binding locks.

---

## 🛠️ Prerequisites

- C++ compiler (`g++` or `clang++` with C++17 support)
- POSIX-compliant operating system (Linux, macOS)

---

## 🚀 Getting Started

### 1. Clone the repository
```bash
git clone https://github.com/harshroyy/RelayX.git
cd RelayX
```

### 2. Build the server
```bash
g++ -std=c++17 proxy.cpp -o proxy
```

### 3. Run the proxy
```bash
./proxy
```
By default, RelayX will listen on port `8080`.

---

## 🧪 Testing the Proxy

You can test forwarding through the proxy using `curl`:

```bash
curl -x http://localhost:8080 http://example.com
```

Or configure your browser/system network settings to route HTTP traffic through `127.0.0.1:8080`.

---

## 🗺️ Roadmap

- [x] Basic TCP socket server & client connection handling
- [x] Host header parsing and DNS lookup
- [x] Request forwarding and chunked response relaying
- [ ] Multi-threading / Event loop (epoll / kqueue) for concurrent client connections
- [ ] Support for HTTPS tunneling (`CONNECT` method)
- [ ] Caching mechanism for static responses
- [ ] Configurable access control / domain blocklists

---

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.
