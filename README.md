# Operating Systems – Assignment 2

## Student Details

- **Name:** Hari Prasath V G
- **Register Number:** `1925260053`
- **Course:** CSA0406 – Operating Systems
- **Compiler:** GCC 15.2.0
- **Operating System:** Ubuntu Linux (WSL2)

## About the Repository

This repository contains the C programs, implementations, execution results and screenshots for Operating Systems Assignment 2.

The assignment covers process management, CPU scheduling, multithreading, inter-process communication, process synchronisation and deadlock management.

Each task folder contains its relevant source code and screenshots of compilation, execution and experimental results.

## Assignment Tasks

| Question | Topic | Description |
|---|---|---|
| Question 01 | Processes and Process States | Process creation, execution and process-state observation. |
| Question 02 | CPU Scheduling | Implementation and analysis of CPU scheduling algorithms. |
| Question 03 | Threads | POSIX threads, thread pools and performance comparison. |
| Question 04 | Inter-Process Communication | Pipe-based IPC, shared memory and data-transfer benchmarking. |
| Question 05 | Synchronisation | Mutexes, semaphores, producer-consumer problem and locking benchmarks. |
| Question 06 | Deadlock and Banker's Algorithm | Safety algorithm, resource allocation and performance benchmarking. |

## Software Requirements

- Ubuntu Linux (WSL2)
- GCC compiler
- Visual Studio Code
- POSIX Threads and semaphores where required

## Compiler Information

The programs were compiled and executed using:

```bash
gcc --version
```

Compiler version:

```text
GCC 15.2.0
```

## Compilation and Execution

Open the Ubuntu terminal, navigate to the relevant task folder and compile the required C source file.

### Question 03 – Threads

```bash
gcc -std=c11 -Wall -Wextra threads.c -o threads -pthread
./threads
```

### Question 04 – Inter-Process Communication

Pipe IPC:

```bash
gcc -std=c11 -Wall -Wextra pipe_ipc.c -o pipe_ipc
./pipe_ipc
```

Shared Memory IPC:

```bash
gcc -std=c11 -Wall -Wextra shared_memory_ipc.c -o shared_memory_ipc -pthread -lrt
./shared_memory_ipc
```

IPC Benchmark:

```bash
gcc -std=c11 -Wall -Wextra ipc_benchmark.c -o ipc_benchmark -pthread -lrt
./ipc_benchmark
```

### Question 05 – Synchronisation

Producer–Consumer:

```bash
gcc -std=c11 -Wall -Wextra producer_consumer.c -o producer_consumer -pthread
./producer_consumer
```

Multiple Producers and Consumers:

```bash
gcc -std=c11 -Wall -Wextra producer_consumer_multi.c -o producer_consumer_multi -pthread
./producer_consumer_multi
```

Railway Lock Benchmark:

```bash
gcc -std=c11 -Wall -Wextra railway_lock_benchmark.c -o railway_lock_benchmark -pthread
./railway_lock_benchmark
```

Atomic, Spin Lock and Mutex Benchmark:

```bash
gcc -std=c11 -Wall -Wextra atomic_spin_mutex.c -o atomic_spin_mutex -pthread
./atomic_spin_mutex
```

### Question 06 – Deadlock and Banker's Algorithm

Banker's Algorithm:

```bash
gcc -std=c11 -Wall -Wextra bankers_algorithm.c -o bankers_algorithm
./bankers_algorithm
```

Banker's Algorithm Benchmark:

```bash
gcc -std=c11 -Wall -Wextra bankers_benchmark.c -o bankers_benchmark
./bankers_benchmark
```

## Experimental Results

Performance measurements were obtained by executing the programs on the local Ubuntu WSL2 environment.

Benchmark results may vary depending on the processor, system load and execution environment.

The screenshots inside each task folder provide evidence of compilation, execution and experimental results.

## Learning Outcomes

Through this assignment, the following concepts were implemented and studied:

- Process creation and process states.
- CPU scheduling and waiting-time analysis.
- Multithreading using POSIX threads.
- Inter-process communication using pipes and shared memory.
- Synchronisation using mutexes, semaphores and atomic operations.
- Deadlock avoidance using Banker's Algorithm.
- Performance measurement and algorithm complexity analysis.

## Author

**Hari Prasath V G**
**1925260053**

B.Tech – Artificial Intelligence and Machine Learning
