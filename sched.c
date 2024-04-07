#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <sys/sem.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/msg.h>
#include <string.h>
#include <sys/ipc.h>
#include <limits.h>
#include <sys/shm.h>

#define Signal(s) semop(s, &signalop, 1)

struct Message
{
    long type;
    int pid;
};

int main(int argc, char *argv[])
{
    printf("\033[1;31m");
    printf("\nSCHED: Scheduler Starting\n");
    printf("\033[0m");

    int num_process = atoi(argv[3]);
    int msg1_shm = atoi(argv[1]);
    int msg2_shm = atoi(argv[2]);

    key_t key = ftok("master.c", 4);
    int psem = semget(key, 1, IPC_CREAT | 0666);

    key = ftok("master.c", 7);
    int finalsem = semget(key, 1, IPC_CREAT | 0666);

    int pid = getpid();

    struct Message message1, message2;

    struct sembuf signalop = {0, 1, 0};
    struct sembuf waitop = {0, -1, 0};

    while (num_process > 0)
    {

        msgrcv(msg1_shm, (void *)&message1, sizeof(struct Message), 0, 0);
        printf("\033[1;32mSCHED: Scheduling process with pid:%d\n\033[0m", message1.pid);

        Signal(psem);

        msgrcv(msg2_shm, (void *)&message2, sizeof(struct Message), 0, 0);

        if (message2.type == 1)
        {
            printf("\033[1;32m");
            printf("SCHED: Process (pid-%d )rescheduled to end of queue\n", message2.pid);
            printf("\033[0m");

            message1.pid = message2.pid;
            message1.type = 1;
            msgsnd(msg1_shm, (void *)&message1, sizeof(struct Message), 0);
        }
        else if (message2.type == 2)
        {
            printf("\033[1;32m");
            printf("\nSCHED: Process (pid-%d) terminated\n", message2.pid);
            printf("\033[0m");
            num_process--;
        }
    }
    printf("\033[1;31m");
    printf("SCHED: Scheduler Succesfully Terminated\n");
    printf("\033[0m");

    Signal(finalsem);

    return 0;
}
