# Internet Text Chat System

This project implements a server-client internet text chat system in C, supporting multiple chat rooms, user authentication, and robust communication over TCP sockets.

---

## Features

### Server-Side:
- **Multi-Client Support**: Handles multiple simultaneous clients using the `poll()` system call.
- **Chat Rooms**:
  - Create and manage chat rooms dynamically.
  - Broadcast messages within a chat room to all participants.
- **User Authentication**:
  - Validates login credentials against a predefined file.
  - Ensures only authenticated users can interact with the server.
- **Command Handling**:
  - `login <username> <password>`: Authenticate the user.
  - `create <room_name>`: Create a new chat room.
  - `enter <room_name>`: Join an existing chat room.
  - `who`: List all connected users and their current chat rooms.
  - `logout`: Log out and disconnect from the server.
- **Graceful Shutdown**: Handles unexpected disconnections and cleans up resources on termination.

### Client-Side:
- Displays messages with the sender's username and timestamp.
- Handles disconnections and performs automatic reconnections.
- Supports disguised password entry during login.

---

## Setup and Usage

### Prerequisites
1. **C Compiler**:
   - Linux: `gcc` (pre-installed on most distributions).
   - Windows: MinGW or a compatible compiler.
2. **POSIX Compatibility**:
   - Linux: Native support for sockets and `poll()`.
   - Windows: Adjustments for Winsock (use `WSAStartup` and `-lws2_32`).

---

### Compilation

#### On Linux:
```bash
gcc server.c -o server
gcc client.c -o client
