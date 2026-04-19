#include "headers.h"
#include "PriQueue.h"
#include <math.h>
#define LOG_FILE_NAME "scheduler.log"
#define PERF_FILE_NAME "scheduler.perf"

int msg_id;
struct PriQueue ready_queue;
int clk;
FILE* log_file = NULL;
int semsyncid, semsyncid2;

// Performance tracking metrics
int count_processes = 0;
int total_useful_time = 0;
float total_waiting_time = 0;
float total_WTA = 0;
float sum_squared_WTA = 0;

pid_t create_process(struct Process* p) {
    if (p->pid > 0) {
        kill(p->pid, SIGCONT);
        return p->pid;
    }
    pid_t pid = fork();
    if (pid == 0) {
        char remaining[10];
        sprintf(remaining, "%d", p->remaining_time);
        execl("./process.out", "process.out", remaining, NULL);
        exit(1);
    }
    p->pid = pid;
    return pid;
}

void stop_process(pid_t pid) {
    kill(pid, SIGSTOP);
}

void logProcessState(int time, int id, const char* state, struct Process* p) {
    if (!log_file) {
        log_file = fopen(LOG_FILE_NAME, "w");
        if (log_file) {
            fprintf(log_file, "#At time x process y state arr w total z remain y wait k\n");
        }
    }
    if (strcmp(state, "finished") == 0) {
        int TA = time - p->arrival_time;
        float WTA = (float)TA / p->running_time;
        
        count_processes++;
        total_waiting_time += p->waiting_time;
        total_useful_time += p->running_time;
        total_WTA += WTA;
        sum_squared_WTA += WTA * WTA;
        
        fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d TA %d WTA %.2f\n",
                time, id, state, p->arrival_time, p->running_time, p->remaining_time, p->waiting_time, TA, WTA);
    } else {
        fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d\n",
                time, id, state, p->arrival_time, p->running_time, p->remaining_time, p->waiting_time);
    }
    fflush(log_file);
}
void HPF() {
    static struct Process* current_process = NULL;
    static pid_t current_pid = -1;
    static int last_clk = -1;

    if (current_process != NULL) {
        if (last_clk != -1 && clk > last_clk) {
            int ticks = clk - last_clk;
            for(int i = 0; i < ticks; i++) {
                // Check if process has already finished
                int status;
                if (waitpid(current_pid, &status, WNOHANG) > 0) {
                    break;
                }
                down(semsyncid);
                up(semsyncid2);
                current_process->remaining_time--;
                if (current_process->remaining_time <= 0) {
                    current_process->remaining_time = 0;
                    break;
                }
            }
        }
    }
    last_clk = clk;
    
    struct msgbuff msg;
    while (msgrcv(msg_id, &msg, sizeof(struct Process), 0, IPC_NOWAIT) != -1) {
        printf("Scheduler received a message at time %d\n", clk);
        
        struct Process* new_process = (struct Process*)malloc(sizeof(struct Process));
        memcpy(new_process, &msg.process, sizeof(struct Process));
        new_process->start_time = -1;
        new_process->last_stop_time = -1;
        new_process->finish_time = -1;
        new_process->remaining_time = new_process->running_time;
        new_process->pid = -1;
        
        printf("Process received: ID=%d, arrival=%d, runtime=%d, priority=%d, remaining=%d\n", 
               new_process->id, new_process->arrival_time, 
               new_process->running_time, new_process->priority,
               new_process->remaining_time);
        
        enqueuePri(new_process, &ready_queue);
    }
    if (current_process == NULL) {
        if (!isEmptyPri(&ready_queue)) {
            struct Process* next_process = peekPri(&ready_queue);
            if (next_process->arrival_time > clk) {
                return;
            }
        
            current_process = dequeuePri(&ready_queue);
            
            if (current_process->start_time == -1) {
                current_process->start_time = clk;
            }
            
            current_process->waiting_time = (clk - current_process->arrival_time) - (current_process->running_time - current_process->remaining_time);
            current_process->state = 1;  
            current_pid = create_process(current_process);
            
            if (current_process->start_time == clk) {
                logProcessState(clk, current_process->id, "started", current_process);
            } else {
                logProcessState(clk, current_process->id, "resumed", current_process);
            }
        }
    }
    else {
        int status;
        pid_t wait_result = waitpid(current_pid, &status, WNOHANG);
        
        if (wait_result > 0 || current_process->remaining_time <= 0) {
            printf("Process %d has finished\n", current_process->id);
            current_process->remaining_time = 0; // ensure it logs as 0
            
            if(current_process->finish_time < 0) {
                logProcessState(clk, current_process->id, "finished", current_process);
            } else {
                logProcessState(current_process->finish_time, current_process->id, "finished", current_process);
            }
            
            free(current_process);
            current_process = NULL;
            current_pid = -1;
            
            if (!isEmptyPri(&ready_queue)) {
                struct Process* next_process = peekPri(&ready_queue);
                if (next_process->arrival_time <= clk) {
                    current_process = dequeuePri(&ready_queue);
                    
                    if (current_process->start_time == -1) {
                        current_process->start_time = clk;
                    }
                    current_process->waiting_time = (clk - current_process->arrival_time) - (current_process->running_time - current_process->remaining_time);
                    current_process->state = 1;
                    current_pid = create_process(current_process);
                    
                    if (current_process->start_time == clk) {
                        logProcessState(clk, current_process->id, "started", current_process);
                        printf("Immediately starting next process %d at time %d\n", current_process->id, clk);
                    } else {
                        logProcessState(clk, current_process->id, "resumed", current_process);
                        printf("Immediately resuming next process %d at time %d\n", current_process->id, clk);
                    }
                }
            }
        } else {
            if (!isEmptyPri(&ready_queue)) {
                struct Process* next_process = peekPri(&ready_queue);
                if (next_process->arrival_time <= clk && next_process->priority < current_process->priority) {
                    printf("Preempting process %d (priority %d) with process %d (priority %d) at time %d\n",
                           current_process->id, current_process->priority,
                           next_process->id, next_process->priority, clk);
                    
                    stop_process(current_pid);
                    
                    current_process->last_stop_time = clk;
                    current_process->state = 2;
                    logProcessState(clk, current_process->id, "stopped", current_process);
                    
                    enqueuePri(current_process, &ready_queue);
                    
                    current_process = dequeuePri(&ready_queue);
                    
                    if (current_process->start_time == -1) {
                        current_process->start_time = clk;
                    }
                    current_process->waiting_time = (clk - current_process->arrival_time) - (current_process->running_time - current_process->remaining_time);
                    current_process->state = 1;
                    current_pid = create_process(current_process);
                    
                    if (current_process->start_time == clk) {
                        logProcessState(clk, current_process->id, "started", current_process);
                    } else {
                        logProcessState(clk, current_process->id, "resumed", current_process);
                    }
                }
            }
        }
    }
}

void cleanup_and_exit(int signum) {
    if (count_processes > 0) {
        FILE* perf_file = fopen(PERF_FILE_NAME, "w");
        if (perf_file) {
            float cpu_util = ((float)total_useful_time / clk) * 100.0;
            float avg_wta = total_WTA / count_processes;
            float avg_waiting = total_waiting_time / count_processes;
            
            float variance = (sum_squared_WTA / count_processes) - (avg_wta * avg_wta);
            float std_wta = 0;
            if (variance > 0) {
                std_wta = sqrt(variance);
            }
            
            fprintf(perf_file, "CPU utilization = %.2f%%\n", cpu_util);
            fprintf(perf_file, "Avg WTA = %.2f\n", avg_wta);
            fprintf(perf_file, "Avg Waiting = %.2f\n", avg_waiting);
            fprintf(perf_file, "Std WTA = %.2f\n", std_wta);
            fclose(perf_file);
        }
    }
    
    destroyClk(true);
    exit(0);
}

int main(int argc, char * argv[])
{
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);
    
    initClk();
    initPriQueue(&ready_queue);
    
    key_t msgKey = ftok("process_generator.c", 65);
    msg_id = msgget(msgKey, 0666);
    
    semsyncid = semget(ftok("process_generator.c", 110), 1, IPC_CREAT | 0666);
    semsyncid2 = semget(ftok("process_generator.c", 111), 1, IPC_CREAT | 0666);
    
    int algo = (argc > 1) ? atoi(argv[1]) : 1;
      
    while(1) {
        clk = getClk();
        if (algo == 1) {
            HPF();
        }
        usleep(10000);
    }
    
    destroyClk(true);
}