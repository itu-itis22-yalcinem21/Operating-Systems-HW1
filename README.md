# BLG312E - Operating Systems HW1  
### Preemptive Priority-Based Scheduler

**Student Name:** ___Your Name Here___  
**Student Number:** ___Your Number Here___  

---

## 1. Overview

This repository contains a **preemptive priority-based process scheduler** developed in C. It was created for Homework 1 of BLG312E (Operating Systems). The scheduler:

- Reads a list of jobs from `jobs.txt`.  
- Uses `fork()` and `exec()` to create child processes (`job` program).  
- Relies on signals (`SIGSTOP`, `SIGCONT`) to implement time slicing (3-second quantum).  
- Logs all key actions (forking, execution, preemption, resumption, termination) to `scheduler.log`.

The core logic is in **`scheduler.c`**, and a **dummy job** is in **`job.c`**.

---

## 2. File Descriptions

- **`scheduler.c`**  
  The main scheduler implementation. It:  
  1. Parses `jobs.txt` to get the time slice and job details.  
  2. Forks/execs new processes at the appropriate arrival times (or defers them, if same-priority older job is unfinished).  
  3. Uses a preemptive priority-based algorithm with time slicing.  
  4. Writes logs to `scheduler.log`.

- **`job.c`**  
  A trivial “CPU-bound” program that simply sleeps for the required CPU time. Our scheduler stops and restarts it via signals.

- **`jobs.txt`**  
  Example input. First line is `TimeSlice 3`, then lines specifying each job:  
  ```txt
  jobA 0 1 6
  jobB 2 2 9
  jobC 4 1 4
