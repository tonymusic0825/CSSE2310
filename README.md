# CSSE2310 - Introduction to Computer Systems Programming (C)

This repository contains a collection of systems programming projects developed in C, focusing on low-level operating system interactions, process management, and networked concurrency for CSSE2310 coursework at the University of Queensland.

---

## Repository Structure

The portfolio is divided into three core assignments, each targeting specific systems-level challenges:

* **/A1-uqunscramble**: A command-line word game utilizing dynamic memory and file I/O.
* **/A3-uqfindexec**: A process management utility that replicates the `find -exec` functionality.
* **/A4-uqimage-service**: A multi-threaded TCP/IP image processing suite.

---

## A1: uqunscramble
**uqunscramble** is a Scrabble-style game that validates user guesses against a dictionary based on a set of provided letters.

### Technical Implementation
* **Dynamic Memory Management**: Uses a custom `WordList` struct to manage variable-sized arrays of strings for dictionary storage and user guess tracking.
* **Input Validation**: Implements robust error checking for command-line arguments and handles real-time input from `stdin`.
* **State Management**: Tracks game progress, scoring (including length-based bonuses), and provides sorted output of valid words upon exit.

---

## A3: uqfindexec
**uqfindexec** traverses a directory and executes specified commands or command pipelines on every file found, mirroring the behavior of `find [dir] -exec [cmd]`.

### Key Features
* **Process Management**: Orchestrates the execution of child processes using the `fork()`, `execvp()`, and `wait()` family of system calls.
* **Pipeline Support**: Capable of handling complex command sequences involving inter-process communication (IPC).
* **Parallel Execution**: Includes an optional `--parallel` mode to spawn multiple concurrent child processes, significantly improving performance for batch tasks.
* **Signal Handling**: Implements a `SIGINT` handler to ensure the program remains responsive and can report progress statistics even if the user interrupts the execution.
* **Directory Traversal**: Recursively navigates file systems using `dirent.h` while respecting filters for hidden files.

---

## A4: uqimageclient & uqimageproc
This project implements a high-concurrency image processing service using a custom network protocol over TCP/IP.

### uqimageproc (Server)
* **Concurrency & Multi-threading**: Utilizes `pthreads` to handle multiple client connections simultaneously. Each request is processed in an isolated thread to maximize throughput.
* **Synchronization**: Uses POSIX semaphores (`sem_t`) to limit the total number of active connections and protect shared server statistics from race conditions.
* **Signal Handling**: Implements a dedicated signal thread to catch `SIGHUP`. Upon receiving the signal, the server logs its current performance metrics (connections, success/fail counts) to `stderr`.
* **Image Transformation**: Integrates with the `FreeImage` library to perform rotations, flips, and scaling on binary image data received over the network.

### uqimageclient (Client)
* **TCP/IP Networking**: Manages socket connections and implements a request/response protocol for transmitting binary image data.
* **Error Resilience**: Designed to handle network failures, such as `SIGPIPE` or unexpected server disconnects, with descriptive exit codes.

---

## Technical Skills & Tools
* **Language**: C (C99/C11 standards)
* **IPC & Concurrency**: Multi-threading (`pthreads`), Semaphores, Process Forking, and Pipes.
* **Networking**: TCP/IP Sockets, Socket I/O, and Protocol Implementation.
* **System Utilities**: Signal Handling (`sigaction`), File Descriptors, and Directory Management.
* **Environment**: Developed and tested on **Fedora Linux** using `gcc`, `make`, and `valgrind` for memory safety.
