#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define MAX_JOBS 100

typedef struct {
    char name[64];
    int arrival_time;
    int priority;         // lower => higher priority
    int total_exec_time;
    int remaining_time;
    pid_t pid;

    int started;          // 0 if not yet forked
    int finished;         // 0 if not finished
    int first_run_done;   // 0 if never actually run, 1 if we've resumed it at least once
} Job;

static Job jobs[MAX_JOBS];
static int job_count = 0;
static int time_quantum = 0;
static int current_time = 0;

/*
 * Write a log line:
 * [YYYY-mm-dd HH:MM:SS] [INFO] message
 */
static void write_log(const char *message)
{
    FILE *fp = fopen("scheduler.log", "a");
    if (!fp) return;
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", lt);

    fprintf(fp, "[%s] [INFO] %s\n", ts, message);
    fclose(fp);
}

/*
 * Fork job i, log:
 *   Forking new process for jobX
 *   Executing jobX (PID: ???) using exec
 * Then SIGSTOP so it won't run until we pick it.
 */
static void fork_job(int i)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }
    if (pid == 0) {
        // child
        char arg[16];
        snprintf(arg, sizeof(arg), "%d", jobs[i].total_exec_time);
        execl("./job", "./job", arg, (char *)NULL);
        perror("execl failed");
        exit(1);
    } else {
        // parent
        jobs[i].pid = pid;
        jobs[i].started = 1;
        jobs[i].first_run_done = 0; // hasn't actually run yet

        char buf[256];
        snprintf(buf, sizeof(buf), "Forking new process for %s", jobs[i].name);
        write_log(buf);

        snprintf(buf, sizeof(buf), "Executing %s (PID: %d) using exec",
                 jobs[i].name, pid);
        write_log(buf);

        // keep it stopped until we schedule it
        kill(pid, SIGSTOP);
    }
}

/*
 * Return 1 if we must "delay" forking 'newIdx' because there's an older partial
 * job with the same priority that arrived earlier.
 */
static int should_delay_fork(int newIdx)
{
    for (int i = 0; i < job_count; i++) {
        if (i == newIdx) continue;
        // older partial job => started && not finished
        if (jobs[i].started && !jobs[i].finished) {
            if (jobs[i].priority == jobs[newIdx].priority &&
                jobs[i].arrival_time < jobs[newIdx].arrival_time)
            {
                // must wait for older job to finish first
                return 1;
            }
        }
    }
    return 0;
}

/*
 * Fork any job that arrived by 'current_time' if we do NOT have to delay it.
 */
static void fork_new_jobs(void)
{
    for (int i = 0; i < job_count; i++) {
        if (!jobs[i].started && !jobs[i].finished) {
            if (jobs[i].arrival_time <= current_time) {
                if (!should_delay_fork(i)) {
                    fork_job(i);
                }
            }
        }
    }
}

/*
 * pick_next_job:
 *   1) skip 'just_preempted' if ANY other job is available (no same-job-back-to-back).
 *   2) among the rest, pick the job with the lowest priority, tie => earliest arrival_time
 *   3) if none found, pick the just_preempted if still valid
 */
static int pick_next_job(int just_preempted)
{
    int best_idx = -1;
    int best_pri = 999999;

    // see if there's a different job
    for (int i = 0; i < job_count; i++) {
        if (i == just_preempted) continue;
        if (jobs[i].started && !jobs[i].finished) {
            if (jobs[i].priority < best_pri) {
                best_pri = jobs[i].priority;
                best_idx = i;
            } else if (jobs[i].priority == best_pri) {
                // tie => earliest arrival
                if (jobs[i].arrival_time < jobs[best_idx].arrival_time) {
                    best_idx = i;
                }
            }
        }
    }
    if (best_idx >= 0) {
        return best_idx;
    }
    // if no other job, check just_preempted
    if (just_preempted >= 0 &&
        jobs[just_preempted].started &&
        !jobs[just_preempted].finished)
    {
        return just_preempted;
    }
    return -1;
}

int main(void)
{
    // Clear old logs
    FILE *c = fopen("scheduler.log", "w");
    if (c) fclose(c);

    // read jobs.txt
    FILE *fp = fopen("jobs.txt", "r");
    if (!fp) {
        perror("jobs.txt");
        exit(1);
    }
    // "TimeSlice <n>"
    char ignore[64];
    fscanf(fp, "%s %d", ignore, &time_quantum);

    // lines: jobName arrival priority execTime
    while (fscanf(fp, "%s", jobs[job_count].name) == 1) {
        fscanf(fp, "%d %d %d",
               &jobs[job_count].arrival_time,
               &jobs[job_count].priority,
               &jobs[job_count].total_exec_time);
        jobs[job_count].remaining_time = jobs[job_count].total_exec_time;
        jobs[job_count].pid = -1;
        jobs[job_count].started = 0;
        jobs[job_count].finished = 0;
        jobs[job_count].first_run_done = 0;
        job_count++;
        if (job_count >= MAX_JOBS) break;
    }
    fclose(fp);

    int completed_jobs = 0;
    int running_job = -1;

    while (completed_jobs < job_count) {
        // 1) see if we can fork newly arrived jobs (which are not delayed)
        fork_new_jobs();

        // 2) if no running job, pick one
        if (running_job < 0) {
            running_job = pick_next_job(-1);
            if (running_job >= 0) {
                // If first time we actually run it, do not log "Resuming"
                if (!jobs[running_job].first_run_done) {
                    jobs[running_job].first_run_done = 1;
                } else {
                    // must be resuming from prior preemption
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                             "Resuming %s (PID: %d) - SIGCONT",
                             jobs[running_job].name,
                             jobs[running_job].pid);
                    write_log(msg);
                }
                kill(jobs[running_job].pid, SIGCONT);
            }
        }

        // If still no job, idle
        if (running_job < 0) {
            sleep(1);
            current_time++;
            continue;
        }

        // 3) Run this job up to 'time_quantum'
        int used = 0;
        while (used < time_quantum) {
            sleep(1);
            current_time++;
            jobs[running_job].remaining_time--;
            used++;

            // If finishes earlier
            if (jobs[running_job].remaining_time <= 0) {
                jobs[running_job].finished = 1;
                completed_jobs++;

                // log "jobA completed execution. Terminating (PID: ???)"
                waitpid(jobs[running_job].pid, NULL, WNOHANG);

                char doneMsg[256];
                snprintf(doneMsg, sizeof(doneMsg),
                         "%s completed execution. Terminating (PID: %d)",
                         jobs[running_job].name, jobs[running_job].pid);
                write_log(doneMsg);

                // ensure it's dead
                kill(jobs[running_job].pid, SIGCONT);
                kill(jobs[running_job].pid, SIGTERM);
                waitpid(jobs[running_job].pid, NULL, 0);

                running_job = -1;
                break; // out of while(used < time_quantum)
            }
        } // end while(used < time_quantum)

        // 4) If job isn't finished, we preempt it
        if (running_job >= 0 && !jobs[running_job].finished) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "%s ran for %d seconds. Time slice expired - Sending SIGSTOP",
                     jobs[running_job].name, used);
            write_log(msg);

            kill(jobs[running_job].pid, SIGSTOP);

            // Now pick a different job if possible
            int old = running_job;

            // 5) see if new jobs arrived in the meantime
            fork_new_jobs();

            running_job = pick_next_job(old);
            if (running_job >= 0) {
                if (!jobs[running_job].first_run_done) {
                    jobs[running_job].first_run_done = 1;
                } else {
                    char rmsg[256];
                    snprintf(rmsg, sizeof(rmsg),
                             "Resuming %s (PID: %d) - SIGCONT",
                             jobs[running_job].name,
                             jobs[running_job].pid);
                    write_log(rmsg);
                }
                kill(jobs[running_job].pid, SIGCONT);
            } else {
                running_job = -1;
            }
        }
    }

    // Removed the log "All jobs have completed."
    // Everything else is unchanged.

    return 0;
}
