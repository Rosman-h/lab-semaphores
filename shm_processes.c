#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

typedef struct {
    int BankAccount;
    sem_t mutex;
} Shared;

void ParentProcess(Shared *shm);
void ChildProcess(Shared *shm);

static int g_shmid;
static Shared *g_shm;
static pid_t g_child;

/* Ctrl+C or kill on the parent: stop the child and clean up */
void handle_sig(int sig)
{
    (void)sig;
    if (g_child > 0)
        kill(g_child, SIGTERM);
    sem_destroy(&g_shm->mutex);
    shmdt((void *)g_shm);
    shmctl(g_shmid, IPC_RMID, NULL);
    _exit(0);
}

int main(void)
{
    pid_t pid;

    setbuf(stdout, NULL);

    g_shmid = shmget(IPC_PRIVATE, sizeof(Shared), IPC_CREAT | 0666);
    if (g_shmid < 0)
    {
        perror("shmget");
        exit(1);
    }

    g_shm = (Shared *)shmat(g_shmid, NULL, 0);
    if (g_shm == (Shared *)-1)
    {
        perror("shmat");
        exit(1);
    }

    g_shm->BankAccount = 0;
    if (sem_init(&g_shm->mutex, 1, 1) == -1)   /* pshared = 1, initial value 1 */
    {
        perror("sem_init");
        exit(1);
    }

    srandom(time(NULL));

    pid = fork();
    if (pid < 0)
    {
        perror("fork");
        exit(1);
    }

    if (pid == 0)
    {
        ChildProcess(g_shm);
        exit(0);
    }

    g_child = pid;
    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    ParentProcess(g_shm);

    wait(NULL);
    return 0;
}

/* Dear Old Dad */
void ParentProcess(Shared *shm)
{
    int localBalance = 0;

    while (1)
    {
        sleep(random() % 6);                       /* 0 - 5 seconds */
        printf("Dear Old Dad: Attempting to Check Balance\n");

        if (random() % 2 == 0)
        {
            if (localBalance < 100)
            {
                /* Deposit Money */
                sem_wait(&shm->mutex);
                localBalance = shm->BankAccount;
                int amount = random() % 100 + 1;
                if (random() % 2 == 0)
                {
                    localBalance += amount;
                    printf("Dear old Dad: Deposits $%d / Balance = $%d\n", amount, localBalance);
                    shm->BankAccount = localBalance;
                }
                else
                {
                    printf("Dear old Dad: Doesn't have any money to give\n");
                }
                sem_post(&shm->mutex);
            }
            else
            {
                printf("Dear old Dad: Thinks Student has enough Cash ($%d)\n", localBalance);
            }
        }
        else
        {
            sem_wait(&shm->mutex);
            localBalance = shm->BankAccount;
            sem_post(&shm->mutex);
            printf("Dear Old Dad: Last Checking Balance = $%d\n", localBalance);
        }
    }
}

/* Poor Student */
void ChildProcess(Shared *shm)
{
    int localBalance = 0;

    srandom(time(NULL) ^ getpid());                /* different numbers than the parent */

    while (1)
    {
        sleep(random() % 6);                       /* 0 - 5 seconds */
        printf("Poor Student: Attempting to Check Balance\n");

        if (random() % 2 == 0)
        {
            /* Withdraw Money */
            sem_wait(&shm->mutex);
            localBalance = shm->BankAccount;
            int need = random() % 50 + 1;
            printf("Poor Student needs $%d\n", need);
            if (need <= localBalance)
            {
                localBalance -= need;
                printf("Poor Student: Withdraws $%d / Balance = $%d\n", need, localBalance);
                shm->BankAccount = localBalance;
            }
            else
            {
                printf("Poor Student: Not Enough Cash ($%d)\n", localBalance);
            }
            sem_post(&shm->mutex);
        }
        else
        {
            sem_wait(&shm->mutex);
            localBalance = shm->BankAccount;
            sem_post(&shm->mutex);
            printf("Poor Student: Last Checking Balance = $%d\n", localBalance);
        }
    }
}