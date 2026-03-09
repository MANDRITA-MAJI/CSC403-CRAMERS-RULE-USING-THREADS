
# Parallel Cramer's Rule using threads

This repository contains an Operating Systems project demonstrating multithreaded mathematical computations using POSIX threads (`pthreads`) in C. It solves a system of linear equations using Cramer's Rule combined with O(n^3) Gaussian Elimination to efficiently handle large matrices (e.g., N >= 1000).

The project is split into two implementations to demonstrate the performance differences between mutex locking and lock-free atomic operations.

## 📂 Repository Structure
* `/human_code` - The baseline implementation using a Thread Pool controlled by a standard `pthread_mutex_t` lock.
* `/ai_code` - An optimized implementation replacing the mutex with C11 lock-free atomics (`stdatomic.h`) to eliminate queue contention.

## ⚙️ Prerequisites
To compile and run this project, your system must have:
* A Linux environment (Ubuntu or WSL works perfectly).
* GCC Compiler.
* Standard POSIX thread (`pthread`) and Math (`m`) libraries.

---

## 🚀 How to Clone and Run

### Step 1: Clone the Repository
Open your terminal and clone the repository to your local machine:

```bash
git clone https://github.com/MANDRITA-MAJI/CSC403-CRAMERS-RULE-USING-THREADS.git
cd CSC403-CRAMERS-RULE-USING-THREADS

```

### Step 2: Running the Baseline (Human Code)
This version compares a standard Thread Pool (where threads = # of hardware CPUs) against a Thread-Per-Equation approach to demonstrate OS context-switching overhead.

Navigate to the folder, compile, and execute:

```bash
cd Our Code
gcc main.c -lpthread -lm -o cramer_exec
./cramer_exec
```
When prompted, enter the number of equations (e.g., 1000).

### Step 3: Running the Optimized (AI Code)
This version tests the same concepts but uses lock-free C11 atomics. We compile this with the `-O3` flag to unleash maximum compiler optimizations.

Navigate to the folder, compile, and execute:

```bash
cd ../AI CODE
gcc -O3 main.c -lpthread -lm -o ai_exec
./ai_exec
```
When prompted, enter the number of equations.

---

## 📊 Viewing the Results

The program automatically detects your hardware threads and Linux username. After execution, it generates two text files in the active directory:

1. **`equation.txt`**: Contains the randomly generated system of equations. 
2. **`solution.txt`**: An appended logbook of all your execution runs. It stores your username, hardware threads, execution times for the different OS scheduling approaches, and the final calculated variables.

To quickly view your results without leaving the terminal, run:

```bash
cat solution.txt
```