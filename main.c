#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

// Function Declarations
int GetUserInput(char *inputType);
void PushToRequestedPrintJobsQueue(int fileSize);
void PushToQueuedJobsQueue(int jobId, int fileSize);
void ShowQueue();
void ShowQueue2();
int GetRandomFileSize();

// Shared Memory Definitions : https://stackoverflow.com/a/63408673
typedef struct
{
	int id;
	size_t size;
}
shm_t;
shm_t* shm_new(size_t size);
void shm_write(shm_t *shm, void *data);
void shm_read(void *data, shm_t *shm);
void shm_del(shm_t *shm);

// Variables for queues : https://www.journaldev.com/36220/queue-in-c
// For our project we need a simple queue logic so I didnot implement linked list 
#define SIZE 100000
int queuedJobsQueue[SIZE][2];
int requestedPrintJobsQueue[SIZE];
int EnqRear = -1, EnqFront = -1, Rear = -1, Front = -1;

// Other Variables
pid_t wpid;
int pid, status = 0, minFileSize = 50, maxFileSize = 512;
int countPrintJobs, bufferSize, maxBufferSize, jobCounter = 1, cancelledJobCounter = 0, lastPrintedJob = 0;

// Online compiler : https://www.onlinegdb.com/online_c_compiler
// Parent Child Process Reference : https://www.geeksforgeeks.org/using-fork-produce-1-parent-3-child-processes/
int main()
{
	printf("Printer Spooler ( MultiProcess Version with Shared Memory ) \n");
	printf("Bedirhan Bardakci - CSE331 Spring 2021 Assignment 2-b\n");

	// For fixed size testings
	countPrintJobs = 10;
	bufferSize = 1000;

	// Uncomment two below lines if you want interactive inputs
	//countPrintJobs = getUserInput("number of printing jobs");
	//bufferSize = getUserInput("buffer size (Kb) of printer");

	maxBufferSize = bufferSize;

	shm_t *shmBuffer = shm_new(sizeof bufferSize);
	shm_t *shmQueuedJobsQueue = shm_new(sizeof queuedJobsQueue);
    shm_t *shmCancelledJobCounter = shm_new(sizeof cancelledJobCounter);
    	
	shm_write(shmBuffer, &bufferSize);
	shm_write(shmQueuedJobsQueue, &queuedJobsQueue);

	printf("\nTotal Number of Printing Jobs To Simulate is %d\n", countPrintJobs);
	printf("\nFree Buffer is %d Kb\n\n", bufferSize);

	pid = fork();	// The fork system call creates a new process

	if (pid == 0) // If fork() returns zero then it means it is child process.
	{
		int nextJob = 0, nextFileSize = 0;
       
		while (countPrintJobs > 0 && nextJob > -1 && cancelledJobCounter < countPrintJobs)
		{
		    shm_read(&cancelledJobCounter, shmCancelledJobCounter);
			shm_read(&queuedJobsQueue, shmQueuedJobsQueue);

			shm_read(&bufferSize, shmBuffer);
			printf("\nCurrent Available Buffer :%d \n", bufferSize);
			int i = 0;
			for (i = 0; i < countPrintJobs; i++)
			{
			    shm_read(&queuedJobsQueue, shmQueuedJobsQueue);
				if (nextJob < queuedJobsQueue[i][0])
				{
					nextJob = queuedJobsQueue[i][0];
					nextFileSize = queuedJobsQueue[i][1];
					break;
				}
			}

			if (nextJob > 0 && lastPrintedJob != nextJob)
			{
				//Print Time Calculation - 1Kb in 0.01 actual seconds.
				double printTime = nextFileSize *0.01;

				// 1 seconds = 1000000000 nanoseconds
				// nanosleep ref : 
				// https://linuxquestions.org/questions/programming-9/can-someone-give-me-the-example-of-the-simplest-usage-of-nanosleep-in-c-4175429688/
				struct timespec ts = { 0, 0.01 * 1000000000L};

				printf("\nJob %d starts printing for %.2lf seconds\n\n", nextJob, printTime);

				while (nextFileSize > 0)
				{
					// Release buffer for each 1Kb and update shared memory
					nanosleep(&ts, NULL);
					shm_read(&bufferSize, shmBuffer);
					bufferSize++;
					shm_write(shmBuffer, &bufferSize);
					nextFileSize = nextFileSize - 1;
				}

				printf("\nJob %d completed printing, ChildProcee : %d - ParentProcess : %d\n", nextJob, getpid(), getppid());
				lastPrintedJob = nextJob;
			}
			else
			{
				// all Done, exit while
				nextJob = -1;
			}
		}

		printf("\nchild[1] --> Finished All Printing Activities pid = %d and ppid = %d\n", getpid(), getppid());
		shm_read(&cancelledJobCounter, shmCancelledJobCounter);
		printf("\nTotal Printed Pages :  %d", countPrintJobs - cancelledJobCounter);
	}
	else // If fork() not returns zero then it means it is parent process.
	{
		srand(time(NULL));
		int lastCheckedJob = 0, i = 0;

		for (i = 0; i < countPrintJobs; i++)
		{
			PushToRequestedPrintJobsQueue(GetRandomFileSize());
		}

		ShowQueue();	// Check Generated Random File Sizes

		while (jobCounter <= countPrintJobs)
		{
			if (Front == -1 || Front > Rear)
			{
				printf("\nQueue Empty \n");
				return 0;
			}
			else
			{
				int tempFileSize = requestedPrintJobsQueue[Front];

				if (lastCheckedJob != jobCounter)
				{
					printf("\nJob %d arrived with size %d Kb\n", jobCounter, tempFileSize);
					lastCheckedJob = jobCounter;
				}

				if (tempFileSize > maxBufferSize)
				{
				    //dequeue because will never be enqueued
					Front = Front + 1;
					
					printf("\nJob %d cancelled and will not be queued, because it is bigger than printer max buffer size %d Kb\n", jobCounter, maxBufferSize);
					lastCheckedJob = jobCounter;
					
					tempFileSize = requestedPrintJobsQueue[Front];
					cancelledJobCounter++;
					shm_write(shmCancelledJobCounter, &cancelledJobCounter);
					jobCounter++;
				}
                else
                {
                   shm_read(&bufferSize, shmBuffer);

    				if (bufferSize >= tempFileSize)
    				{
    					PushToQueuedJobsQueue(jobCounter, tempFileSize);	// Push new job to queue and 
    					shm_write(shmQueuedJobsQueue, &queuedJobsQueue);	// update queue on shared memory
    
    					shm_read(&bufferSize, shmBuffer);	// get up to date buffer size
    					bufferSize = bufferSize - tempFileSize;	// calculate new buffer 
    					shm_write(shmBuffer, &bufferSize);	// update buffer on shared memory
    
    					printf("\n\nJob %d enqueued now with size %d Kb, process id %u, buffer left : %d Kb\n", jobCounter, tempFileSize, getpid(), bufferSize);
    
    					Front = Front + 1;	//dequeue
    					
    					jobCounter++;
    				}
    				else
    				{
    					// For Testing IF it is actually wait for available buffer
    					struct timespec ts2 = { 0, 0.2 * 1000000000L};	// check evry 0.2 seconds
    					nanosleep(&ts2, NULL);
    					printf("\nJob %d - File size : %d Kb is waiting for available buffer, current empty buffer :%d", jobCounter, tempFileSize, bufferSize);
    				} 
                }
			}
		}

		printf("\nparent --> pid = %d  Completed Queuing Suitable Jobs \n", getpid());

		//the parent waits for all the child processes 
		while ((wpid = wait(&status)) > 0);
	}

	return 0;
}

shm_t* shm_new(size_t size)
{
	shm_t *shm = calloc(1, sizeof *shm);
	shm->size = size;

	if ((shm->id = shmget(IPC_PRIVATE, size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)) < 0)
	{
		perror("shmget");
		free(shm);
		return NULL;
	}

	return shm;
}

void shm_write(shm_t *shm, void *data)
{
	void *shm_data;
	if ((shm_data = shmat(shm->id, NULL, 0)) == (void*) - 1)
	{
		perror("write");
		return;
	}

	memcpy(shm_data, data, shm->size);
	shmdt(shm_data);
}

void shm_read(void *data, shm_t *shm)
{
	void *shm_data;

	if ((shm_data = shmat(shm->id, NULL, 0)) == (void*) - 1)
	{
		perror("read");
		return;
	}

	memcpy(data, shm_data, shm->size);
	shmdt(shm_data);
}

void shm_del(shm_t *shm)
{
	shmctl(shm->id, IPC_RMID, 0);
	free(shm);
}

// getting random numbers in a range
int GetRandomFileSize()
{
	return (rand() % (maxFileSize - minFileSize + 1)) + minFileSize;
}

void PushToRequestedPrintJobsQueue(int fileSize)
{
	if (Rear == SIZE - 1)
	{
		printf("Overflow \n");
	}
	else
	{
		if (Front == -1)
		{
			Front = 0;
		}

		Rear = Rear + 1;
		requestedPrintJobsQueue[Rear] = fileSize;
	}
}

void PushToQueuedJobsQueue(int jobId, int fileSize)
{
	if (EnqRear == SIZE - 1)
	{
		printf("Overflow \n");
	}
	else
	{
		if (EnqFront == -1)
		{
			EnqFront = 0;
		}

		EnqRear = EnqRear + 1;
		queuedJobsQueue[EnqRear][0] = jobId;
		queuedJobsQueue[EnqRear][1] = fileSize;
	}
}

void ShowQueue()
{
	if (Front == -1)
	{
		printf("Empty Queue \n");
	}
	else
	{
		int i = 0;
		for (i = Front; i <= Rear; i++)
		{
			printf("%d Kb, ", requestedPrintJobsQueue[i]);
		}

		printf("\n");
	}
}

void ShowQueue2()
{
	if (EnqFront == -1)
	{
		printf("Empty Queue \n");
	}
	else
	{
		printf("\n");
		int i = 0;
		for (i = EnqFront; i <= EnqRear; i++)
		{
			printf("%d - %d Kb, ", queuedJobsQueue[i][0], queuedJobsQueue[i][1]);
		}

		printf("\n");
	}
}

int GetUserInput(char *inputType)
{
	// pass inputType as parameter to reduce duplicate code for checking inputs
	int temp;
	do {
		printf("\nPlease enter the %s : ", inputType);

		if (scanf("%d", &temp))
		{
			return temp;
		}
		else
		{
			fflush(stdin);
			printf("Invalid input! Please enter value as integer\n");
		}
	} while (1);
}