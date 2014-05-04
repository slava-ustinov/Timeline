#include <stdio.h>
#include "stdlib.h"
#include <semaphore.h>
#include <fcntl.h>

char SEM_NAME[]= "vik";

int main()
{
	sem_t *mutex;
	sem_t *mutex1;	
	mutex = sem_open(SEM_NAME,O_CREAT,0644,1);

	if(0==fork())
	{
		mutex1 = sem_open(SEM_NAME,0,0644,0);
		sem_wait(mutex1);
		printf("child 1\n");
		sleep(5);
		printf("child 2\n");		
		sem_post(mutex1);
		sem_close(mutex1);
		exit(0);
	}
	sem_wait(mutex);
	printf("parent 1\n");
	sleep(5);
	printf("parent 2\n");
	sem_post(mutex);
	sem_close(mutex);
	sem_unlink(SEM_NAME);
	exit(0);
}
