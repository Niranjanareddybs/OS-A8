#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <signal.h>

#define Signal(s) semop(s, &signalop, 1)

struct message_type_1
{
    long type;
    int pid;
} message_type_1;

struct message_type_2
{
    long type;
    int pageorframe;
    int pid;
} message_type_2;

int main(int argc, char *argv[])
{
    // signal(SIGKILL,sighand);
    printf("Scheduler Starting\n");
    int num_process = atoi(argv[3]);
    int msg1_shm = atoi(argv[1]);
    int msg2_shm = atoi(argv[2]);

    key_t key = ftok("master.c", 4);
    int psem = semget(key, 1, IPC_CREAT | 0666);


    int pid = getpid();

    struct message_type_1 message1, message2;
    struct sembuf signalop = {0, 1, 0};
    struct sembuf waitop = {0, -1, 0};
    while (num_process > 0)
    {

        msgrcv(msg1_shm, (void *)&message1, sizeof(struct message_type_1), 0, 0);

        printf("\t\tScheduling process with pid:%d\n", message1.pid);


        Signal(psem);

        msgrcv(msg2_shm, (void *)&message2, sizeof(struct message_type_1), 0, 0);

        if (message2.type == 1)
        {
            printf("\t\tProcess (pid-%d )rescheduled to end of queue\n", message2.pid);
            message1.pid = message2.pid;
            message1.type = 1;
            msgsnd(msg1_shm, (void *)&message1, sizeof(struct message_type_1), 0);
        }
        else if (message2.type == 2)
        {
            printf("\t\tProcess (pid-%d) terminated\n", message2.pid);
            num_process--;
        }
    }

    printf("\t\tScheduler Succesfully Terminated\n");

    key = ftok("master.c", 7);
    int finalsem = semget(key, 1, IPC_CREAT | 0666);
    Signal(finalsem);

    return 0;
}
