#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <fcntl.h>
#include <errno.h>


using namespace std;

#define PIN_MSG_NAME "/pin_msg"
#define DB_MSG_NAME "/db_msg"

#define MESSAGE_QUEUE_SIZE 9

pthread_t ATM;
pthread_t DB_server;
pthread_t DB_editor;
void* run_ATM(void* arg);
void* run_DB(void* arg);

static struct mq_attr mq_attribute;
static mqd_t PIN_MSG = -1;

pthread_mutex_t printing_lock;

void lockAndPrint(string printThis){
	pthread_mutex_lock(&printing_lock);
	cout << printThis;
	pthread_mutex_unlock(&printing_lock);
}

void sig_handler(int signum){
	//ASSERT(signum == SIGINT);
	if (signum == SIGINT){
		cout << "killing application" << endl;

		pthread_cancel(ATM);
		pthread_cancel(DB_server);
		pthread_cancel(DB_editor);
	}
}

int main(int argc, char const *argv[])
{
	pthread_attr_t attr;

	signal(SIGINT, sig_handler);


	/*************************MUTEX INIT***************/
	int rc = pthread_mutex_init(&printing_lock, NULL);
	if (rc < 0){
		perror("Error creating mutex");
	}


	/******************MESSAGE QUEUE INIT*************/
	mq_attribute.mq_maxmsg = 10; //mazimum of 10 messages in the queue at the same time
	mq_attribute.mq_msgsize = (size_t) MESSAGE_QUEUE_SIZE;

	PIN_MSG = mq_open(PIN_MSG_NAME , O_CREAT | O_RDWR, 0666, &mq_attribute);
	if (PIN_MSG == -1){
		perror("creating message queue failed ");
	}

	// DB_MSG = mq_open(DB_MSG_NAMEl, O_CREAT | O_RDWR, 0666, &mq_attribute);
	/*************************************************/

	/****************PTHREAD INIT AND RUN************/
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024*1024);

	long start_arg = 0; //the start argument is unused right now
	pthread_create(&ATM, NULL, run_ATM, (void*) start_arg);
	pthread_create(&DB_server, NULL, run_DB, (void*) start_arg);
	/***************************************************/


	pthread_join(ATM, NULL);
	pthread_join(DB_server, NULL);

	/******************MUTEX DESTROY******************/
	pthread_mutex_destroy(&printing_lock);

	sig_handler(SIGINT);
}

void* run_ATM(void* arg) {
	int status;
	char accountNumber[6];
	char PIN[4];
	char accountNumAndPIN[10];
	
	lockAndPrint("ATM is running\n");
	lockAndPrint("Please input account number > ");
	cin >> accountNumber;
	lockAndPrint("Please input PIN > ");
	cin >> PIN;

	for (int i = 0; i < 6; ++i)
	{
		accountNumAndPIN[i] = accountNumber[i];
	}

	accountNumAndPIN[5] = ',';

	for (int i = 6; i < 10; ++i)
	{
		accountNumAndPIN[i] = PIN[i-6];
	}

	pthread_mutex_lock(&printing_lock);
	status = mq_send(PIN_MSG, accountNumAndPIN, sizeof(accountNumAndPIN), 1);
	if (status < 0){
		perror("sending message failed");
	}
}


void* run_DB(void* arg){
	lockAndPrint("Database server running\n");
	int status;
	char received_acct_number[30];

	while(1){
		status = mq_receive(PIN_MSG, received_acct_number, sizeof(received_acct_number), NULL);
		if (status < 0){
			perror("error ");
		} else {
			for (int i = 0; i < 10; ++i)
			{
				printf("%c", received_acct_number[i]);
			}
		}
	}
}
