#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <signal.h>

/* Producer/consumer program illustrating conditional variables */

/* Size of shared buffer */
#define QUEUE_KEY 100
#define SHARED_BUFFER_KEY 200
#define SHARED_NUMBER_KEY 300
#define SHARED_BUFFER_SIZE_KEY 500
#define SEMAPHORE_KEY 400
#define BUF_SIZE 30
/* arg for semctl system calls. */
union semun
{
    int val;               /* value for SETVAL */
    struct semid_ds *buf;  /* buffer for IPC_STAT & IPC_SET */
    ushort *array;         /* array for GETALL & SETALL */
    struct seminfo *__buf; /* buffer for IPC_INFO */
    void *__pad;
};

struct msgbuff
{
    long mtype;
    int num;
};

int add = 0; /* place to add next element */
int shmid, numid, semaphoreId, qId, sizeid;
int *buffer, *num;

void initQueue();
void initSemaphores();
void initSharedMemory();
void init();
void down(int);
void up(int);
void clearResources();
void sendMsg();
void rcvMsg();

int main(int argc, char *argv[])
{
    init();

    // adding values to buffer
    for (int i = 1; i <= 200; i++)
    {
        if (*num > BUF_SIZE)
            exit(1); /* overflow */

        if ((*num) == BUF_SIZE)
            rcvMsg();

        down(semaphoreId);
        buffer[add] = i;
        add = (add + 1) % BUF_SIZE;

        if (*num == 0)
            sendMsg();

        (*num)++;
        up(semaphoreId);
        printf("producer: inserted %d - Buffer size: %d\n", i, *num);
        fflush(stdout);
    }
}

void initQueue()
{
    // initializing the message queue
    qId = msgget(QUEUE_KEY, 0666 | IPC_CREAT); //create message queue and return id

    if (qId == -1)
    {
        perror("Error in creating queue");
        exit(-1);
    }
}
void initSemaphores()
{
    // initializing the semaphore
    semaphoreId = semget(SEMAPHORE_KEY, 1, 0666 | IPC_CREAT);

    if (semaphoreId == -1)
    {
        perror("Error in creating semaphore");
        exit(-1);
    }

    // set default value of the semaphore
    union semun semaphoreUn;
    semaphoreUn.val = 1;
    if (semctl(semaphoreId, 0, SETVAL, semaphoreUn) == -1)
    {
        perror("Error in setting initial value of semaphore");
        exit(-1);
    }
}

void initSharedMemory()
{
    shmid = shmget(SHARED_BUFFER_KEY, (BUF_SIZE) * sizeof(int), IPC_CREAT | 0644);
    numid = shmget(SHARED_NUMBER_KEY, sizeof(int), IPC_CREAT | 0644);

    if (shmid == -1 || numid == -1)
    {
        perror("Error in creating shared");
        exit(-1);
    }

    buffer = shmat(shmid, (void *)0, 0);
    num = shmat(numid, (void *)0, 0);

    if (buffer == -1 || num == -1)
    {
        perror("Error in attaching shared memory");
        exit(-1);
    }
}

void init()
{
    signal(SIGINT, clearResources);
    initQueue();
    initSemaphores();
    initSharedMemory();

    (*num) = 0;
}

void down(int sem)
{
    struct sembuf p_op;

    p_op.sem_num = 0;
    p_op.sem_op = -1;
    p_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &p_op, 1) == -1)
    {
        perror("Error in down()");
        exit(-1);
    }
}

void up(int sem)
{
    struct sembuf v_op;

    v_op.sem_num = 0;
    v_op.sem_op = 1;
    v_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &v_op, 1) == -1)
    {
        perror("Error in up()");
        exit(-1);
    }
}

void clearResources()
{
    // deattachig the buffer
    shmdt(buffer);
    // deattaching the num
    shmdt(num);
}

void sendMsg()
{
    printf("SENDING MESSAGE TO CONSUMER \n");
    struct msgbuff message;
    message.mtype = 8;
    int send_val = msgsnd(qId, &message, sizeof(message.num), !IPC_NOWAIT); /* block if buffer empty */
    if (send_val == -1)
    {
        perror("error in sending value to consumer");
        exit(-1);
    }
}

void rcvMsg()
{
    //wait for the message from the producer
    struct msgbuff message;
    printf("Waiting for message from consumer\n");
    fflush(stdout);

    int recievedValue = msgrcv(qId, &message, sizeof(message.num), 8, !IPC_NOWAIT);
    fflush(stdout);
    if (recievedValue == -1)
    {
        perror("Error in receive from queue");
        exit(-1);
    }
    fflush(stdout);
}