# HTTPlite

The goal is to build a functional HTTP/1.1 server from scratch to understand the fundamentals of the protocol and low-level network programming using TCP sockets in C++. It follows the incremental stages provided by the Codecrafters platform, starting from basic socket binding to handling file uploads and concurrency.

## Features Implemented (So Far)

*   **TCP Server Setup:** Binds to a port (4221) and listens for incoming TCP connections. Uses `SO_REUSEADDR` for quick restarts during development.
*   **HTTP Request Parsing:**
    *   Parses the request line to extract the HTTP method (GET, POST) and the request target (path).
    *   Reads and parses HTTP headers into a case-insensitive map (key-value pairs). Handles the end-of-headers marker (`\r\n\r\n`).
    *   Reads the HTTP request body based on the `Content-Length` header, necessary for POST requests.
*   **HTTP Response Generation:**
    *   Constructs valid HTTP/1.1 responses with status line, headers, and body.
    *   Supports status codes: `200 OK`, `201 Created`, `400 Bad Request`, `404 Not Found`, `500 Internal Server Error`.
*   **Routing:** Handles requests based on the path and method:
    *   `GET /`: Responds with `200 OK`.
    *   `GET /echo/{string}`: Responds with `200 OK` and echoes `{string}` in the body with `Content-Type: text/plain`.
    *   `GET /user-agent`: Responds with `200 OK` and the value of the `User-Agent` request header in the body.
    *   `GET /files/{filename}`: Responds with `200 OK`, `Content-Type: application/octet-stream`, and the file content if the file exists in the specified directory. Responds with `404 Not Found` otherwise. Includes basic path traversal prevention.
    *   `POST /files/{filename}`: Reads the request body and writes it to a file named `{filename}` in the specified directory. Responds with `201 Created` on success. Includes basic path traversal prevention.
*   **Concurrency:** Handles multiple client connections concurrently using C++ `std::thread` (thread-per-connection model). The main thread accepts connections and launches detached worker threads to handle each client independently.
*   **File System Interaction:**
    *   Reads files from a specified directory (passed via `--directory` command-line argument) for `GET /files/...` requests.
    *   Writes files to the specified directory for `POST /files/...` requests.
*   **Modularity:** Code is structured into `server.cpp` (main server loop, threading) and `request_handler.h`/`.cpp` (request reading, parsing, routing, response generation).

## Technologies Used

*   **Language:** C++ (using C++17/C++23 features like `std::thread`, `std::string`, `std::unordered_map`, etc.)
*   **Build System:** CMake
*   **Core Libraries:**
    *   Standard C++ Library (`<iostream>`, `<string>`, `<vector>`, `<sstream>`, `<fstream>`, `<unordered_map>`, `<thread>`, `<algorithm>`, `<cctype>`)
    *   POSIX Sockets API (`<sys/socket.h>`, `<netdb.h>`, `<arpa/inet.h>`, `<unistd.h>`) for low-level networking.
    *   Zlib (Linked via CMake for potential future compression stage).



