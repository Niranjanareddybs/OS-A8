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

#define PROB 0.1

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

typedef struct SM1
{
    int pid;              // process id
    int mi;               // number of required pages
    int fi;               // number of frames allocated
    int pagetable[100][3]; // page table
    int totalpagefaults;
    int totalillegalaccess;
} SM1;

int pidscheduler;
int pidmmu;
char msgid1str[10], msgid2str[10], msgid3str[10];
char shmid1str[10], shmid2str[10], shmid3str[10];
int msgid1, msgid2, msgid3;
int semid1, semid2, semid3, semid4;
int shmid1, shmid2, shmid3;

SM1 *sm1;
int *sm2;
int *sm3;

void sighand(int signum)
{
    if (signum == SIGINT)
    {
        // kill scheduler,mmu and all the processes
        kill(pidscheduler, SIGINT);
        kill(pidmmu, SIGINT);

        // detach and remove shared memory
        shmdt(sm1);
        shmctl(shmid1, IPC_RMID, NULL);

        shmdt(sm2);
        shmctl(shmid2, IPC_RMID, NULL);

        shmdt(sm3);
        shmctl(shmid3, IPC_RMID, NULL);

        // remove semaphores
        semctl(semid1, 0, IPC_RMID, 0);
        semctl(semid2, 0, IPC_RMID, 0);
        semctl(semid3, 0, IPC_RMID, 0);
        semctl(semid4, 0, IPC_RMID, 0);

        // remove message queues
        msgctl(msgid1, IPC_RMID, NULL);
        msgctl(msgid2, IPC_RMID, NULL);
        msgctl(msgid3, IPC_RMID, NULL);

        printf("CHECK 16\n");
        exit(1);
    }
}

int main()
{
    signal(SIGINT, sighand);
    srand(time(0));

    struct sembuf pop = {0, -1, 0};
    struct sembuf vop = {0, 1, 0};

    int k, m, f;
    printf("Enter the number of processes: ");
    scanf("%d", &k);
    printf("Enter the Virtual Address Space size: ");
    scanf("%d", &m);
    printf("Enter the Physical Address Space size: ");
    scanf("%d", &f);

    // page table for k processes
    key_t key = ftok("master.c", 1);
    int shmid1 = shmget(key, k * sizeof(SM1), IPC_CREAT | 0666);
    sm1 = (SM1 *)shmat(shmid1, NULL, 0);

    for (int i = 0; i < k; i++)
    {
        // page table for k processes

        // sm1[i].pagetable = (int **)malloc(m * sizeof(int *));
        // for (int j = 0; j < m; j++)
        // {
        //     sm1[i].pagetable[j] = (int *)malloc(3 * sizeof(int));
        // }
        sm1[i].totalpagefaults = 0;
        sm1[i].totalillegalaccess = 0;
    }

    // free frames list
    key = ftok("master.c", 20);
    int shmid2 = shmget(key, (f + 1) * sizeof(int), IPC_CREAT | 0666);
    sm2 = (int *)shmat(shmid2, NULL, 0);

    // initialize the frames, 1 means free, 0 means occupied, -1 means end of list
    //doubt
    for (int i = 0; i < f; i++)
    {
        sm2[i] = 1;
        // printf("CHECK\n");
    }
    sm2[f] = -1;

    // process to page mapping
    key = ftok("master.c", 3);
    int shmid3 = shmget(key, k * sizeof(int), IPC_CREAT | 0666);
    sm3 = (int *)shmat(shmid3, NULL, 0);

    // initially no frames are allocated to any process
    for (int i = 0; i < k; i++)
    {
        sm3[i] = 0;
    }

    // semaphore 1 for Processes
    key = ftok("master.c", 4);
    semid1 = semget(key, 1, IPC_CREAT | 0666);
    semctl(semid1, 0, SETVAL, 0);

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
    semid4 = semget(key, 1, IPC_CREAT | 0666);
    semctl(semid4, 0, SETVAL, 0);

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

    // pass number of processes for now
    char strk[10];
    sprintf(strk, "%d", k);

    // printf("CHECK 11\n");
    // create Scheduler process, pass msgid1str and msgid2str
    pidscheduler = fork();
    if (pidscheduler == 0)
    {
        printf("scheduler started\n");
        execlp("./sched", "./sched", msgid1str, msgid2str, strk, NULL);
    }

    // create Memory Management Unit process, pass msgid2str and msgid3str, shmid1str and shmid2str
    pidmmu = fork();
    if (pidmmu == 0)
    {
        printf("Memory Management Unit started\n");
        // execlp("xterm", "xterm", "-T", "Memory Management Unit", "-e", "./mmu", msgid2str, msgid3str, shmid1str, shmid2str, NULL);
        execlp("./mmu", "./mmu", msgid2str, msgid3str, shmid1str, shmid2str, NULL);
    }

    // int **refi = (int **)malloc((k) * sizeof(int *));
    // char **refstr = (char **)malloc((k) * sizeof(char *));
    int refi[100][602];
    char refstr[100][602];

    // printf("CHECK 12\n");
    // initialize the Processes
    for (int i = 0; i < k; i++)
    {
        // generate random number of pages between 1 to m
        sm1[i].mi = rand() % m + 1;
        sm1[i].fi = 0;
        for (int j = 0; j < m; j++)
        {
            sm1[i].pagetable[j][0] = -1;      // no frame allocated
            sm1[i].pagetable[j][1] = 0;       // invalid
            sm1[i].pagetable[j][2] = INT_MAX; // timestamp
        }

        int y = 0;
        int x = rand() % (8 * sm1[i].mi + 1) + 2 * sm1[i].mi;


        // refi[i] = (int *)malloc(x * sizeof(int));
        for (int j = 0; j < x; j++)
        {
            refi[i][j] = rand() % sm1[i].mi;
            int temp = refi[i][j];
            while (temp > 0)
            {
                temp /= 10;
                y++;
            }
            y++;
        }
        y++;

        // with probability PROB, corrupt the reference string, by putting illegal page number
        for (int j = 0; j < x; j++)
        {
            if ((double)rand() / RAND_MAX < PROB)
            {
                refi[i][j] = rand() % m;
            }
        }

        // refstr[i] = (char *)malloc(y * sizeof(char));
        memset(refstr[i], '\0', y);
        // for(int i=0;i<y;i++){
        //     refstr[i]='\0';
        // }

        for (int j = 0; j < x; j++)
        {
            char temp[12] = {'\0'};
            sprintf(temp, "%d.", refi[i][j]);
            strcat(refstr[i], temp);
        }
    }

    sleep(1);
    // printf("CHECK 13\n");
    // create Processes
    for (int i = 0; i < k; i++)
    {
        printf("CHECK 113\n");
        sleep(1);
        int pid = fork();
        if (pid != 0)
        {
            sm1[i].pid = pid;
        }
        else
        {
            // pass ref[i], msgid1str, msgid3str
            execlp("./process", "./process", refstr[i], msgid1str, msgid3str, NULL);
        }
    }
    // printf("CHECK 14\n");

    // wait for Scheduler to signal
    P(semid4);

    printf("CHECK 15\n");
    // terminate Scheduler
    kill(pidscheduler, SIGINT);

    // terminate Memory Management Unit
    kill(pidmmu, SIGINT);

    // detach and remove shared memory
    shmdt(sm1);
    shmctl(shmid1, IPC_RMID, NULL);

    shmdt(sm2);
    shmctl(shmid2, IPC_RMID, NULL);

    shmdt(sm3);
    shmctl(shmid3, IPC_RMID, NULL);

    // remove semaphores
    semctl(semid1, 0, IPC_RMID, 0);
    semctl(semid2, 0, IPC_RMID, 0);
    semctl(semid3, 0, IPC_RMID, 0);
    semctl(semid4, 0, IPC_RMID, 0);

    // remove message queues
    msgctl(msgid1, IPC_RMID, NULL);
    msgctl(msgid2, IPC_RMID, NULL);
    msgctl(msgid3, IPC_RMID, NULL);

    printf("CHECK 16\n");
    return 0;
}
