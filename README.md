# Multi-Client Chat Server

This repository contains a multi-client chat server implemented in C using sockets and pthreads. The server supports multiple clients connecting simultaneously and exchanging messages in real-time.

## Features
- Handles multiple clients using threads.
- Supports basic message broadcasting.
- Uses TCP sockets for reliable communication.
- Implements basic error handling.
- Works on Linux-based systems.

## Prerequisites
- GCC Compiler
- POSIX Threads Library (pthread)
- Basic knowledge of networking concepts

## Installation
Clone the repository:
```sh
git clone https://github.com/your-username/chat-server.git
cd chat-server
```

## Compilation
Compile the server and client using GCC:
```sh
gcc server.c -o server -lpthread
gcc client.c -o client -lpthread
```

## Usage
### Start the Server
```sh
./server <port>
```
Example:
```sh
./server 8080
```

### Connect Clients
Run multiple instances of the client to connect:
```sh
./client <server_ip> <port>
```
Example:
```sh
./client 127.0.0.1 8080
```

