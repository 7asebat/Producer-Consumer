#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include<errno.h>


void down(int, int);
void up(int, int);
void clearResources();
void init();
void initSharedMemory();
void initSemaphores();
int getSemaphore(int, int);


#define BUF_SIZE 20
#define QUEUE_KEY 100
#define SHARED_BUFFER_KEY 200
#define SEMAPHORE_KEY 400
#define REMAINING_KEY 500

int qId, buffId, remId, semId;
int *buff, *rem;

union semun
{
    int val;               /* value for SETVAL */
    struct semid_ds *buf;  /* buffer for IPC_STAT & IPC_SET */
    ushort *array;         /* array for GETALL & SETALL */
    struct seminfo *__buf; /* buffer for IPC_INFO */
    void *__pad;
};


int main()
{
    init();
    int i;
    while(1)
    {
        down(semId,1);
        down(semId,0);
        
        i = buff[*rem];
        (*rem) = ((*rem) + 1) % BUF_SIZE;

        printf("consumer: consumed value %d\n", i);
        fflush(stdout);
        up(semId,0);
        up(semId,2);
    }
}


void init()
{
    signal(SIGINT, clearResources);
    initSemaphores();
    up(semId,3);
    initSharedMemory();
}



void initSemaphores()
{
    int exists = semget(SEMAPHORE_KEY,5,0666|IPC_CREAT|IPC_EXCL);
    if (exists == -1)
    {
        semId = semget(SEMAPHORE_KEY, 5, 0666 | IPC_CREAT);
    }
    else{
        semId = exists;
        // set default value of the semaphore
        union semun semaphoreUn;
        semaphoreUn.val = 1;
        if (semctl(semId, 0, SETVAL, semaphoreUn) == -1)
        {
            perror("Error in setting initial value of semaphore");
            exit(-1);
        }
        //Initialize num of filled positions in buffer
        semaphoreUn.val = 0;
        if (semctl(semId, 1, SETVAL, semaphoreUn) == -1)
        {
            perror("Error in setting initial value of semaphore");
            exit(-1);
        }
        //Initialize num of filled positions in buffer
        semaphoreUn.val = BUF_SIZE;
        if (semctl(semId, 2, SETVAL, semaphoreUn) == -1)
        {
            perror("Error in setting initial value of semaphore");
            exit(-1);
        }
        //Semaphore to keep count of consumers connected
        semaphoreUn.val = 0;
        if (semctl(semId, 3, SETVAL, semaphoreUn) == -1)
        {
            perror("Error in setting initial value of semaphore");
            exit(-1);
        }
        //Semaphore to keep count of producers connected
        semaphoreUn.val = 0;
        if (semctl(semId, 4, SETVAL, semaphoreUn) == -1)
        {
            perror("Error in setting initial value of semaphore");
            exit(-1);
        }
    }

}

void initSharedMemory()
{
    buffId = shmget(SHARED_BUFFER_KEY, (BUF_SIZE) * sizeof(int), IPC_CREAT | 0644);
    remId = shmget(REMAINING_KEY, sizeof(int), IPC_CREAT | 0644 | IPC_EXCL);
    int remIsNew = 1;

    //Check if rem exists to determine initialization
    if(errno == EEXIST){
        remId = shmget(REMAINING_KEY, sizeof(int), IPC_CREAT | 0644);
        remIsNew = 0;
    }

    if (buffId == -1)
    {
        perror("Error in creating shared");
        exit(-1);
    }
   

    buff = shmat(buffId, (void *)0, 0);
    rem = shmat(remId, (void *)0, 0);

    if (buff == -1 || rem == -1)
    {
        perror("Error in attaching shared memory");
        exit(-1);
    }

    if(remIsNew == 1){
        *rem = 0;
    }
}


void down(int sem,int idx)
{
    struct sembuf p_op;

    p_op.sem_num = idx;
    p_op.sem_op = -1;
    p_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &p_op, 1) == -1)
    {
        perror("Error in down()");
        exit(-1);
    }
}

void up(int sem, int idx)
{
    struct sembuf v_op;

    v_op.sem_num = idx;
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
    down(semId,3);
    //Check if there's other processes still running
    if(getSemaphore(semId,3) != 0){
        //detaching the buffer
        shmdt(buff);
        //detaching add
        shmdt(rem);
    }
    else{
        //Destructing shared memories
        shmctl(buffId,IPC_RMID,(struct shmid_ds *)0);
        shmctl(remId,IPC_RMID,(struct shmid_ds *)0);
        //Removing semaphores
        semctl(semId,0,IPC_RMID);
        semctl(semId,1,IPC_RMID);
        semctl(semId,2,IPC_RMID);
        semctl(semId,3,IPC_RMID);
        semctl(semId,4,IPC_RMID);

    }
    exit(1);
}

int getSemaphore(int semId,int idx){
    union semun argument;
    return semctl(semId,idx,GETVAL,argument);
}