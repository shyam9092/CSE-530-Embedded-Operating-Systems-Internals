#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <linux/ioctl.h>
#include <linux/rtc.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <pthread.h>

#define MAX_ITER 150

struct rb_object {
	int key;
	int data;
};

pthread_mutex_t rb1_lock, rb2_lock;
  
void* rb1_callback(void* arg)
{
	int iter = 0,order=0,fd,rs,flag;
	int rCounter=0,wCounter=0;
	char *path = (char *)(arg);
	struct rb_object *rb_obj = (struct rb_object *)(malloc(sizeof(struct rb_object)));

	// get the thread id to print the data in the logs
	pthread_t threadid = pthread_self();

	// open the file descriptor
	fd = open(path, O_RDWR);
	srand(time(0));
	//Created 50 write calls
	for(iter = 0; iter<50; iter++)
	{
		rb_obj->data = (rand()%1000);
		rb_obj->key  = (rand()%256);
		pthread_mutex_lock(&rb1_lock);
		rs = write(fd,rb_obj,sizeof(struct rb_object));
		pthread_mutex_unlock(&rb1_lock);
		if(rs == 0) {
			printf("Thread [%ld] Write Success, Data [%d] Key [%d]\n", threadid, rb_obj->data, rb_obj->key);
		} 
		else {
			printf("Thread [%ld] Write Error, Data [%d] Key [%d]\n", threadid, rb_obj->data, rb_obj->key);
		}
	}
	
	for(iter =0;iter<MAX_ITER;iter++)
	{
		flag = (rand()%5);
		if((flag == 1 || flag == 3))
		{
			rb_obj->data = (rand()%1000);
			rb_obj->key  = (rand()%256);
			pthread_mutex_lock(&rb1_lock);
			rs = write(fd,rb_obj,sizeof(struct rb_object));
			pthread_mutex_unlock(&rb1_lock);
			if(rs == 0) {
				printf("Thread [%ld] Write Success, Data [%d] Key [%d]\n", threadid, rb_obj->data, rb_obj->key);
				wCounter++;
			} else {
				printf("Thread [%ld] Write Error, Data [%d] Key [%d]\n", threadid, rb_obj->data, rb_obj->key);
			}
		}
		else
		{ 
			if(flag == 2) {
				order = order ? 0 : 1;
				ioctl(fd,0,order);
			}
			pthread_mutex_lock(&rb1_lock);
			rs = read(fd,rb_obj,sizeof(struct rb_object));
			pthread_mutex_unlock(&rb1_lock);
			if(rs == 0) {
				printf("Thread [%ld] Read Success, Order [%s] Data [%d] Key [%d]\n", threadid, order ? "Descending" : "Ascending",rb_obj->data, rb_obj->key);
				rCounter++;
			} else {
				printf("Thread [%ld] Read Error, Order [%s] Data [%d] Key [%d]\n", threadid, order ? "Descending" : "Ascending",rb_obj->data, rb_obj->key);
			}
		}

		// go for some sleep
		sleep(1);
	}

	printf("Thread [%ld] Total Write Happend [%d] Total Read Happened [%d]\n", threadid, wCounter, rCounter);

	// close the file desriptor before leaving
	close(fd);

	return NULL; 
}

void* rb2_callback(void* arg)
{
	int iter = 0,order=0,fd,rs,flag;
	int rCounter=0,wCounter=0;
	char *path = (char *)(arg);
	struct rb_object *rb_obj = (struct rb_object *)(malloc(sizeof(struct rb_object)));

	// get the thread id to print the data in the logs
	pthread_t threadid = pthread_self();

	// open the file descriptor
	fd = open(path, O_RDWR);
	srand(time(0));

	//Created 50 write calls
	for(iter = 0; iter<50; iter++)
	{
		rb_obj->data = (rand()%1000);
		rb_obj->key  = (rand()%256);
		pthread_mutex_lock(&rb2_lock);
		rs = write(fd,rb_obj,sizeof(struct rb_object));
		pthread_mutex_unlock(&rb2_lock);
		if(rs == 0) {
			printf("Thread [%ld] Write Success, Data [%d] Key [%d]\n", threadid, rb_obj->data, rb_obj->key);
		} 
		else {
			printf("Thread [%ld] Write Error, Data [%d] Key [%d]\n", threadid, rb_obj->data, rb_obj->key);
		}
	}
	


	for(iter =0;iter<MAX_ITER;iter++)
	{
		flag = (rand()%5);
		if((flag == 1 || flag == 3))
		{
			rb_obj->data = (rand()%1000);
			rb_obj->key  = (rand()%256);
			pthread_mutex_lock(&rb2_lock);
			rs = write(fd,rb_obj,sizeof(struct rb_object));
			pthread_mutex_unlock(&rb2_lock);
			if(rs == 0) {
				printf("Thread [%ld] Write Success, Data [%d] Key [%d]\n", threadid, rb_obj->data, rb_obj->key);
				wCounter++;
			} else {
				printf("Thread [%ld] Write Error, Data [%d] Key [%d]\n", threadid, rb_obj->data, rb_obj->key);
			}
		}
		else
		{ 
			if(flag == 2) {
				order = order ? 0 : 1;
				ioctl(fd,0,order);
			}
			pthread_mutex_lock(&rb2_lock);
			rs = read(fd,rb_obj,sizeof(struct rb_object));
			pthread_mutex_unlock(&rb2_lock);
			if(rs == 0) {
				printf("Thread [%ld] Read Success, Order [%s] Data [%d] Key [%d]\n", threadid, order ? "Descending" : "Ascending",rb_obj->data, rb_obj->key);
				rCounter++;
			} else {
				printf("Thread [%ld] Read Error, Order [%s] Data [%d] Key [%d]\n", threadid, order ? "Descending" : "Ascending",rb_obj->data, rb_obj->key);
			}
		}

		// go for some sleep
		sleep(1);
	}

	printf("Thread [%ld] Total Write Happend [%d] Total Read Happened [%d]\n", threadid, wCounter, rCounter);

	// close the file desriptor before leaving
	close(fd);

	return NULL; 
}
  
int main(void)
{
  int error;
	int i = 0;
	pthread_t tid[4];
	char *rb1_path = "/dev/rbt530_dev1";
	char *rb2_path = "/dev/rbt530_dev2";

    if (pthread_mutex_init(&rb1_lock, NULL) != 0) { 
        printf("\n mutex init has failed\n"); 
        return 1; 
    }

	if (pthread_mutex_init(&rb2_lock, NULL) != 0) { 
        printf("\n mutex init has failed\n"); 
        return 1; 
    }

	for(i=0;i<4;i++) {
		if(i%2) {
			error = pthread_create(&(tid[i]), NULL, &rb1_callback, rb1_path);
			if (error != 0)
				printf("\nThread can't be created :[%s]", strerror(error));
		} else {
			error = pthread_create(&(tid[i]), NULL, &rb2_callback, rb2_path);
			if (error != 0)
				printf("\nThread can't be created :[%s]", strerror(error));
		}
	}

	for(i=0;i<4;i++) {
		pthread_join(tid[i], NULL); 
	}
  
	pthread_mutex_destroy(&rb1_lock);
	pthread_mutex_destroy(&rb2_lock);
  
    return 0;
}

