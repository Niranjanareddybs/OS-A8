#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/msg.h>
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <sys/sem.h>

#define PROB 0.1

#define Wait(s) semop(s, &waitop, 1)
#define Signal(s) semop(s, &signalop, 1)

typedef struct PageTable
{
    int pid;
    int pages_req;
    int frames_alloted;
    int pagetable[200][3];
    int total_page_faults;
    int total_illegal_access;
} PageTable;

int pidmmu;
int pidscheduler;
int msgid1, msgid2, msgid3;
int shmid1, shmid2, shmid3;
int psem, semid2, semid3, finalsem;
char msgid1str[10], msgid2str[10], msgid3str[10];
char shmid1str[10], shmid2str[10], shmid3str[10];

PageTable *SM1;
int *SM2;
int *SM3;

void sighand(int signum)
{
    if (signum == SIGINT)
    {
        kill(pidscheduler, SIGINT);
        kill(pidmmu, SIGINT);

        // detach and remove shared memory
        shmdt(SM1);
        shmctl(shmid1, IPC_RMID, NULL);

        shmdt(SM2);
        shmctl(shmid2, IPC_RMID, NULL);

        shmdt(SM3);
        shmctl(shmid3, IPC_RMID, NULL);

        // Remove semaphores
        semctl(psem, 0, IPC_RMID, 0);
        semctl(semid2, 0, IPC_RMID, 0);
        semctl(semid3, 0, IPC_RMID, 0);
        semctl(finalsem, 0, IPC_RMID, 0);

        // Remove message queues
        msgctl(msgid1, IPC_RMID, NULL);
        msgctl(msgid2, IPC_RMID, NULL);
        msgctl(msgid3, IPC_RMID, NULL);
        exit(1);
    }
}

int main()
{
    srand(time(0));
    signal(SIGINT, sighand);

    struct sembuf waitop = {0, -1, 0};
    struct sembuf signalop = {0, 1, 0};

    int k, m, f;
    printf("Total number of processes ( k ) : ");
    scanf("%d", &k);
    printf("Virtual Address Space size ( m ) : ");
    scanf("%d", &m);
    printf("Physical Address Space size ( f ) : ");
    scanf("%d", &f);

    // page table for k processes
    key_t key = ftok("master.c", 1);
    int shmid1 = shmget(key, k * sizeof(PageTable), IPC_CREAT | 0666);
    SM1 = (PageTable *)shmat(shmid1, NULL, 0);

    // free frames list
    key = ftok("master.c", 20);
    int shmid2 = shmget(key, (f + 1) * sizeof(int), IPC_CREAT | 0666);
    SM2 = (int *)shmat(shmid2, NULL, 0);

    // process to page mapping
    key = ftok("master.c", 3);
    int shmid3 = shmget(key, k * sizeof(int), IPC_CREAT | 0666);
    SM3 = (int *)shmat(shmid3, NULL, 0);

    // semaphore 1 for Processes
    key = ftok("master.c", 4);
    psem = semget(key, 1, IPC_CREAT | 0666);
    semctl(psem, 0, SETVAL, 0);

    // semaphore 2 for Scheduler
    key = ftok("master.c", 5);
    semid2 = semget(key, 1, IPC_CREAT | 0666);
    semctl(semid2, 0, SETVAL, 0);

    // semaphore 3 for Memory Management Unit
    key = ftok("master.c", 6);
    semid3 = semget(key, 1, IPC_CREAT | 0666);
    semctl(semid3, 0, SETVAL, 0);

    // semaphore 4 for Master
    key = ftok("master.c", 7);
    finalsem = semget(key, 1, IPC_CREAT | 0666);
    semctl(finalsem, 0, SETVAL, 0);

    // Message Queue 1 for Ready Queue
    key = ftok("master.c", 8);
    msgid1 = msgget(key, IPC_CREAT | 0666);

    // Message Queue 2 for Scheduler
    key = ftok("master.c", 9);
    msgid2 = msgget(key, IPC_CREAT | 0666);

    // Message Queue 3 for Memory Management Unit
    key = ftok("master.c", 10);
    msgid3 = msgget(key, IPC_CREAT | 0666);

    // convert msgid to string
    sprintf(msgid1str, "%d", msgid1);
    sprintf(msgid2str, "%d", msgid2);
    sprintf(msgid3str, "%d", msgid3);

    // convert shmids to string
    sprintf(shmid1str, "%d", shmid1);
    sprintf(shmid2str, "%d", shmid2);
    sprintf(shmid3str, "%d", shmid3);

    char strk[10];
    sprintf(strk, "%d", k);

    for (int i = 0; i < k; i++)
    {
        SM1[i].total_page_faults = 0;
        SM1[i].total_illegal_access = 0;
        SM1[i].pages_req = rand() % m + 1;
        SM1[i].frames_alloted = 0;
        for (int j = 0; j < m; j++)
        {
            SM1[i].pagetable[j][0] = -1;
            SM1[i].pagetable[j][1] = 0;
            SM1[i].pagetable[j][2] = INT_MAX;
        }
    }

    for (int i = 0; i < f; i++)
    {
        SM2[i] = 1;
    }
    SM2[f] = -1;

    for (int i = 0; i < k; i++)
    {
        SM3[i] = 0;
    }

    pidscheduler = fork();
    if (pidscheduler == 0)
    {
        printf("scheduler started\n");
        execlp("./sched", "./sched", msgid1str, msgid2str, strk, NULL);
    }
    pidmmu = fork();
    if (pidmmu == 0)
    {
        printf("Memory Management Unit started\n");
        execlp("xterm", "xterm", "-T", "Memory Management Unit", "-e", "./mmu", msgid2str, msgid3str, shmid1str, shmid2str, NULL);
        execlp("./mmu", "./mmu", msgid2str, msgid3str, shmid1str, shmid2str, NULL);
    }

    int refi[100][1000];
    char refstr[100][1000];

    for (int i = 0; i < k; i++)
    {
        int y = 0;
        int x = rand() % (8 * SM1[i].pages_req + 1) + 2 * SM1[i].pages_req;
        for (int j = 0; j < x; j++)
        {
            refi[i][j] = rand() % SM1[i].pages_req;
            int temp = refi[i][j];
            while (temp > 0)
            {
                temp /= 10;
                y++;
            }
            y++;
        }
        y++;
        for (int j = 0; j < x; j++)
        {
            if ((double)rand() / RAND_MAX <= PROB)
            {
                refi[i][j] = rand() % m;
            }
        }
        memset(refstr[i], '\0', y);

        for (int j = 0; j < x; j++)
        {
            char temp[12] = {'\0'};
            sprintf(temp, "%d.", refi[i][j]);
            strcat(refstr[i], temp);
        }
    }

    sleep(1);

    for (int i = 0; i < k; i++)
    {
        // usleep(250000);
        sleep(1);
        int pid = fork();
        if (pid != 0)
        {
            SM1[i].pid = pid;
        }
        else
        {
            execlp("./process", "./process", refstr[i], msgid1str, msgid3str, NULL);
        }
    }

    Wait(finalsem);

    for (int i = 0; i < k; i++)
    {
        printf("Process %d: pid-%d \ttotal_page_faults=%d \ttotal_illegal_pages=%d\n", i + 1, SM1[i].pid, SM1[i].total_page_faults, SM1[i].total_illegal_access);
    }

    kill(pidscheduler, SIGINT);
    kill(pidmmu, SIGINT);

    // detach and remove shared memory
    shmdt(SM1);
    shmctl(shmid1, IPC_RMID, NULL);

    shmdt(SM2);
    shmctl(shmid2, IPC_RMID, NULL);

    shmdt(SM3);
    shmctl(shmid3, IPC_RMID, NULL);

    // remove message queues
    msgctl(msgid1, IPC_RMID, NULL);
    msgctl(msgid2, IPC_RMID, NULL);
    msgctl(msgid3, IPC_RMID, NULL);

    // remove semaphores
    semctl(psem, 0, IPC_RMID, 0);
    semctl(semid2, 0, IPC_RMID, 0);
    semctl(semid3, 0, IPC_RMID, 0);
    semctl(finalsem, 0, IPC_RMID, 0);

    return 0;
}
