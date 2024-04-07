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
#include <time.h>
#include <limits.h>
#include <signal.h>

void sighand(int signo)
{
    if (signo == SIGKILL)
    {
        fprintf(stderr, "An error occurred: %s\n", "File not found");
        sleep(10);
    }
}

typedef struct PageTable
{
    int pid;           
    int pages_req;      
    int frames_alloted; 
    int pagetable[200][3];
    int total_page_faults;
    int total_illegal_access;
} PageTable;

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

#define Wait(s) semop(s, &waitop, 1)
#define Signal(s) semop(s, &signalop, 1)

int main(int argc, char *argv[])
{
    signal(SIGKILL, sighand);

    struct sembuf waitop = {0, -1, 0};
    struct sembuf signalop = {0, 1, 0};

    int timestamp = 0;

    printf("\033[1;31m");
    printf("MMU: Memory Management Unit Started\n");
    printf("\033[0m");

    int msg1_shm = atoi(argv[1]);
    int msg2_shm = atoi(argv[2]);

    struct message_type_1 message1;
    struct message_type_2 message2;

    int shm1 = atoi(argv[3]);
    PageTable *SM1 = (PageTable *)shmat(shm1, NULL, 0);

    int shm2 = atoi(argv[4]);
    int *SM2 = (int *)shmat(shm2, NULL, 0);

    while (1)
    {
        timestamp++;
        msgrcv(msg2_shm, (void *)&message2, sizeof(message_type_2), 0, 0);

        int i = 0;
        while (1)
        {
            if (SM1[i].pid == message2.pid)
            {
                break;
            }
            i++;
        }

        printf("\033[1;33mMMU: Global Ordering(pid-%d) - (Timestamp %d, Process %d, Page %d)\n \033[0m", SM1[i].pid, timestamp, i + 1, message2.pageorframe);

        int page = message2.pageorframe;

        if (page == -9)
        {
            for (int j = 0; j < SM1[i].pages_req; j++)
            {
                if (SM1[i].pagetable[j][0] != -1)
                {
                    SM2[SM1[i].pagetable[j][0]] = 1;
                    SM1[i].pagetable[j][2] = INT_MAX;
                    SM1[i].pagetable[j][0] = -1;
                    SM1[i].pagetable[j][1] = 0;
                }
            }

            message1.pid = message2.pid;
            message1.type = 2;
            msgsnd(msg1_shm, (void *)&message1, sizeof(message_type_1), 0);
        
        }
        else if (SM1[i].pagetable[page][1] == 1 && SM1[i].pagetable[page][0] != -1)
        {
            message2.pageorframe = SM1[i].pagetable[page][0];
            SM1[i].pagetable[page][2] = timestamp;
            msgsnd(msg2_shm, (void *)&message2, sizeof(message_type_2), 0);
        }
        else if (page >= SM1[i].pages_req)
        {
            message2.pageorframe = -2;
            msgsnd(msg2_shm, (void *)&message2, sizeof(message_type_2), 0);

            SM1[i].total_illegal_access++;

            printf("\033[1;33mMMU: Invalid Page Reference(pid-%d) - (Process %d, Page %d)\n\033[0m", SM1[i].pid, i + 1, page);

            for (int j = 0; j < SM1[i].pages_req; j++)
            {
                if (SM1[i].pagetable[j][0] != -1)
                {
                    SM2[SM1[i].pagetable[j][0]] = 1;
                    SM1[i].pagetable[j][0] = -1;
                    SM1[i].pagetable[j][1] = 0;
                    SM1[i].pagetable[j][2] = INT_MAX;
                }
            }

            message1.pid = message2.pid;
            message1.type = 2;
            msgsnd(msg1_shm, (void *)&message1, sizeof(message_type_1), 0);
        }
        else
        {
            message2.pageorframe = -1;
            msgsnd(msg2_shm, (void *)&message2, sizeof(message_type_2), 0);
            SM1[i].total_page_faults++;

            printf("\033[1;33mMMU: Page fault sequence(pid-%d)- (Process %d, Page %d)\n\033[0m", SM1[i].pid, + +1, page);
            int j = 0;
            while (1)
            {
                if (SM2[j] == -1)
                {
                    break;
                }
                else if (SM2[j] == 1)
                {
                    SM2[j] = 0;
                    break;
                }
                j++;
            }

            if (SM2[j] != -1)
            {
                SM1[i].pagetable[page][2] = timestamp;
                SM1[i].pagetable[page][0] = j;
                SM1[i].pagetable[page][1] = 1;
                message1.pid = message2.pid;
                message1.type = 1;
                msgsnd(msg1_shm, (void *)&message1, sizeof(message_type_1), 0);
            }

            else
            {
                int page_replaced = -1;
                int minimum = INT_MAX;

                for (int replace = 0; replace < SM1[i].pages_req; replace++)
                {
                    if (SM1[i].pagetable[replace][2] < minimum)
                    {
                        minimum = SM1[i].pagetable[replace][2];
                        page_replaced = replace;
                    }
                }

                SM1[i].pagetable[page_replaced][1] = 0;
                SM1[i].pagetable[page][0] = SM1[i].pagetable[page_replaced][0];
                SM1[i].pagetable[page][1] = 1;
                SM1[i].pagetable[page][2] = timestamp;
                SM1[i].pagetable[page_replaced][2] = INT_MAX;

                message1.type = 1;
                message1.pid = message2.pid;
                msgsnd(msg1_shm, (void *)&message1, sizeof(message_type_1), 0);
            }
        }
    }
}