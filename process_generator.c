#include "headers.h"

pid_t clkpid, schedulerpid;

key_t msgKey, shmKey;
int msgq_id, shm_id;
int *shmaddrinfo;

struct Process *processes;

#define DEBUG 1

#define DBG(fmt, ...) \
    if (DEBUG) printf(fmt "\n", ##__VA_ARGS__);

/////////////////////// CLEANUP //////////////////////
void clearResources(int signum)
{
    DBG("\n[GEN] SIGINT received : cleaning resources");

    kill(clkpid, SIGTERM);
    kill(schedulerpid, SIGTERM);

    DBG("[GEN] Sent SIGTERM : clkpid=%d schedulerpid=%d", clkpid, schedulerpid);

    msgctl(msgq_id, IPC_RMID, NULL);
    shmctl(shm_id, IPC_RMID, NULL);

    DBG("[GEN] IPC removed (msg + shm)");

    free(processes);

    destroyClk(true);

    DBG("[GEN] Clock destroyed : exiting");

    exit(0);
}


int main(int argc, char *argv[])
{
    signal(SIGINT, clearResources);

    DBG("\n//////////////// PROCESS GENERATOR START ////////\n");

    /////////////////////// USER INPUT //////////////
    int algorithm;
    int quantum = 0;

    printf("Choose Scheduling Algorithm:\n");
    printf("1) HPF\n");
    printf("2) RR\n");
    printf("Enter choice: ");
    scanf("%d", &algorithm);

    while (algorithm != 1 && algorithm != 2)
    {
        printf("Invalid choice. Enter 1 (HPF) or 2 (RR): ");
        scanf("%d", &algorithm);
    }

    if (algorithm == 2)
    {
        printf("Enter Quantum: ");
        scanf("%d", &quantum);
    }

    DBG("[INPUT] Algorithm = %d | Quantum = %d", algorithm, quantum);

    //////////////////////MESSAGE QUEUE /////////////////////
    msgKey = ftok("process_generator", 65);
    DBG("[MSG] ftok key = %d", msgKey);

    msgq_id = msgget(msgKey, IPC_CREAT | 0666);
    if (msgq_id == -1) {
        perror("[MSG] msgget failed");
        exit(1);
    }
    DBG("[MSG] Message queue created : id = %d", msgq_id);

    ///////////////////// SHARED MEMORY ///////////////////
    shmKey = ftok("process_generator", 66);
    DBG("[SHM] ftok key = %d", shmKey);

    shm_id = shmget(shmKey, sizeof(int) * 3, IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("[SHM] shmget failed");
        exit(1);
    }
    DBG("[SHM] Shared memory created : id = %d", shm_id);

    shmaddrinfo = (int *) shmat(shm_id, NULL, 0);
    if (shmaddrinfo == (void *) -1) {
        perror("[SHM] shmat failed");
        exit(1);
    }
    DBG("[SHM] Attached at address %p", shmaddrinfo);

    /////////////////////// FILE INPUT ////////////////////
    const char *filename = "processes.txt";
    if (argc > 1) filename = argv[1];

    DBG("[FILE] Opening file: %s", filename);

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("[FILE] fopen failed");
        exit(1);
    }

    int N = 0;
    int capacity = 16;
    int i = 0;

    processes = malloc(capacity * sizeof(struct Process));
    if (!processes) {
        perror("[FILE] malloc failed");
        exit(1);
    }

    DBG("[FILE] Initial capacity = %d", capacity);

    /////////////////////// READ FILE ////////////////////
    char line[256];
    int lineNum = 0;

    while (fgets(line, sizeof(line), file)) {
        lineNum++;

        DBG("[FILE] Line %d: %s", lineNum, line);

        char *p = line;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            p++;

        if (*p == '#' || *p == '\0' || *p == '\n') {
            DBG("[FILE] Skipped comment/empty line %d", lineNum);
            continue;
        }

        int maybeCount;
        char extra[16];

        if (sscanf(p, "%d %15s", &maybeCount, extra) == 1 && i == 0 && N == 0) {
            N = maybeCount;
            DBG("[FILE] Detected N = %d", N);
            continue;
        }

        struct Process proc;

        int parsed = sscanf(p, "%d %d %d %d %d",
                            &proc.id,
                            &proc.arrival_time,
                            &proc.running_time,
                            &proc.priority,
                            &proc.MEMSIZE);

        DBG("[FILE] Parsed fields = %d", parsed);

        if (parsed == 4) {
            proc.MEMSIZE = 0;
            DBG("[FILE] MEMSIZE defaulted → P%d", proc.id);
        } else if (parsed != 5) {
            fprintf(stderr, "[ERROR] Invalid line %d: %s", lineNum, line);
            exit(1);
        }

        DBG("[FILE] Process loaded → ID=%d AT=%d BT=%d PR=%d MEM=%d",
            proc.id, proc.arrival_time, proc.running_time,
            proc.priority, proc.MEMSIZE);

        if (i >= capacity) {
            capacity *= 2;
            processes = realloc(processes, capacity * sizeof(struct Process));

            DBG("[FILE] realloc → new capacity = %d", capacity);

            if (!processes) {
                perror("realloc failed");
                exit(1);
            }
        }

        processes[i++] = proc;
    }

    fclose(file);

    DBG("[FILE] Total processes loaded = %d", i);

    if (N == 0) N = i;
    if (i < N) N = i;

    DBG("[FILE] Final N = %d", N);

    /////////////////////// START CLOCK /////////////////////
    clkpid = fork();
    if (clkpid == 0) {
        DBG("[CLK] Starting clock...");
        execl("./clk.out", "clk.out", NULL);
        perror("execl clk failed");
        exit(1);
    }

    DBG("[CLK] Clock PID = %d", clkpid);

    /////////////////////// START SCHEDULER /////////////////////
    schedulerpid = fork();
    if (schedulerpid == 0) {
        DBG("[SCH] Starting scheduler...");

        char algoStr[10], quantumStr[10];
        sprintf(algoStr, "%d", algorithm);
        sprintf(quantumStr, "%d", quantum);

        execl("./scheduler.out", "scheduler.out", algoStr, quantumStr, NULL);

        perror("execl scheduler failed");
        exit(1);
    }

    DBG("[SCH] Scheduler PID = %d", schedulerpid);

    ////////////////////////INIT CLOCK ////////////////////////
    initClk();
    DBG("[CLK] Clock initialized");

    
    int current = 0;

    DBG("[GEN] Entering dispatch loop...");

    while (current < N) {

        int now = getClk();

        DBG("[CLK] time=%d | next=P%d AT=%d",
            now,
            processes[current].id,
            processes[current].arrival_time);

        if (processes[current].arrival_time <= now) {

            struct msgbuff msg;
            msg.mtype = 1;
            msg.process = processes[current];

            DBG("[MSG] Sending : P%d", msg.process.id);

            if (msgsnd(msgq_id, &msg, sizeof(msg.process), 0) == -1) {
                perror("[MSG] msgsnd failed");
                exit(1);
            }

            DBG("[MSG] Sent P%d at time %d",
                msg.process.id, now);

            current++;

        } else {
            DBG("[GEN] Waiting for next arrival...");
            sleep(1);
        }
    }

    ////////////////////// CLEAN /////////////////////
    DBG("[GEN] All processes dispatched");

    waitpid(clkpid, NULL, 0);
    waitpid(schedulerpid, NULL, 0);

    DBG("[GEN] Children terminated");

    destroyClk(true);

    DBG("[GEN] Generator finished cleanly");

    return 0;
}
