# A simple Makefile for compiling the scheduler and job executables.

CC = gcc
CFLAGS = -Wall -Wextra -g

all: scheduler job

# Build the scheduler executable from scheduler.c
scheduler: scheduler.c
	$(CC) $(CFLAGS) -o scheduler scheduler.c

# Build the job executable from job.c
job: job.c
	$(CC) $(CFLAGS) -o job job.c

# 'make clean' will remove generated files:
clean:
	rm -f scheduler job scheduler.log
