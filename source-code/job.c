/***************************************************************
 * job.c — Simulated job that runs for the specified CPU time.
 *
 * Usage: ./job <execution_time_in_seconds>
 *
 * This program sleeps for <execution_time_in_seconds> total seconds,
 * in increments of 1 second, to simulate CPU usage.  
 ***************************************************************/

 #include <stdio.h>
 #include <stdlib.h>
 #include <unistd.h>
 #include <signal.h>
 
 int main(int argc, char *argv[])
 {
     if (argc < 2) {
         fprintf(stderr, "Usage: %s <execution_time_in_seconds>\n", argv[0]);
         return 1;
     }
 
     int remaining_time = atoi(argv[1]);
 
     // Simple simulation: sleep for 'remaining_time' seconds in total.
     // If the scheduler SIGSTOPs us, we get paused automatically; 
     // on SIGCONT, we resume from this loop.
     while (remaining_time > 0) {
         sleep(1);
         remaining_time--;
     }
 
     // Once the loop ends, this job is “complete”.
     return 0;
 }
 