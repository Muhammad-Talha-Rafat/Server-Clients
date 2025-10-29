# Server-Client

**A Simple Dropbox Clone**

---

## Project Description
This project implements a simple server-client file management system, similar to a lightweight Dropbox. Clients can signup/login, upload, download, delete, and list files in their personal directories on the server. The server handles multiple clients with a task queue and enforces a maximum client limit.  

**Key features:**
- Handles up to **5 tasks** in the task queue.
- Handles up to **2 clients at a time**.
- User authentication (`signup` and `login`).
- File operations limited to each user's personal folder.
- Multi-threaded server using `pthreads`.
- Task queue for client commands (`UPLOAD`, `DOWNLOAD`, `DELETE`, `LIST`).

---

## File Structure

```
Server-Clients
├── Makefile
├── server.c
├── client.c
└── assets
    ├── users.txt
    ├── files
    │   ├── river.txt
    │   └── ... (+9 other files)
    └── users
        ├── Talha
        │   ├── forest.txt
        │   └── winter.txt
        └───Ronaldo
            └── letter.txt
```

---

## Dependencies

- GCC (with `pthread` support)
- Make

Optional tools for debugging:

- Valgrind `--leak-check` – memory leak detection.
- Valgrind `helgrind` – detect race conditions.

**Note:** ThreadSanitizer (TSAN) was also attempted but not executed successfully due to compatibility constraints.

---

## Running Instructions

1. Open **two separate terminals** – one for the server, one for the client(s).
2. **Start the server**:

```bash
make server_run
```

3. **Run a client**:

```bash
make client_run
```

4. **Client commands**:

- `signup <username> <password>` – Register a new user.
- `login <username> <password>` – Login as an existing user.
- `UPLOAD <filename>` – Upload a file from the server's `assets/files/` folder to your user folder.
- `DOWNLOAD <filename>` – Download a file from your user folder.
- `DELETE <filename>` – Delete a file from your user folder.
- `LIST` – List files in your user folder.
- `EXIT` – Disconnect from the server.

5. **Clean compiled files**:

```bash
make clean
```

---

## Usage Example

```text
authorize > signup Talha 123
Talha > LIST
name        size
-------------------------
forest.txt  2048 bytes
winter.txt  1024 bytes

Talha > UPLOAD river.txt
server > File successfully uploaded

Talha > DOWNLOAD forest.txt
(server sends file content)

Talha > DELETE winter.txt
server > File successfully deleted
```

---

## Notes

- The server must be started **before** any clients can connect.
- The server enforces a maximum of **2 active clients** at a time. Additional clients are temporarily rejected with a queue message.
- The server can handle **up to 5 tasks** in the task queue. If the queue is full, clients must wait before their tasks are processed.
