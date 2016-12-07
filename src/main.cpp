#include <iostream>
#include <string.h>
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

#define DATABASE_FILENAME "../database.db"

typedef struct Pin_msg_struct{
	char acctNum[6];
	char PIN[4];
} Pin_msg_struct;

typedef struct DB_msg_struct{
	char message[6]; //only thing for now
} DB_msg_struct;

#define PIN_MESSAGE_QUEUE_SIZE sizeof(Pin_msg_struct)
#define DB_MESSAGE_QUEUE_SIZE sizeof(DB_msg_struct)

pthread_t ATM;
pthread_t DB_server;
pthread_t DB_editor;
void* run_ATM(void* arg);
void* run_DB(void* arg);

static struct mq_attr mq_attribute;
static mqd_t PIN_MSG = -1;
static mqd_t DB_MSG = -1;

pthread_mutex_t printing_lock;

/*****************UTILITIES*************/
void lockAndPrint(string printThis){
	pthread_mutex_lock(&printing_lock);
	cout << printThis;
	pthread_mutex_unlock(&printing_lock);
}

int arrayEquals(char* array1, char* array2, int length){
	for (int i = 0; i < length; ++i)
	{
		if(array1[i] != array2[i]) return 0;
	}
	return 1;
}

char* decryptPIN(char* encryptedPIN){
	// cout << "original array: " << encryptedPIN << endl;
	int num = atoi(encryptedPIN);
	char* decryptedPin = (char*) calloc(4, sizeof(char));
	num--;
	for (int i = 2; i >= 0; --i, num /= 10)
	{
		decryptedPin[i] = (num % 10) + '0';
	}
	decryptedPin[3] = '\0';
	return decryptedPin;
}

/******************MISC******************/
void sig_handler(int signum){
	//ASSERT(signum == SIGINT);
	if (signum == SIGINT){
		cout << "killing application" << endl;

		pthread_cancel(ATM);
		pthread_cancel(DB_server);
		pthread_cancel(DB_editor);

		mq_close(PIN_MSG);
		mq_unlink(PIN_MSG_NAME);
		mq_close(DB_MSG);
		mq_unlink(DB_MSG_NAME);
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
	mq_attribute.mq_msgsize = (size_t) PIN_MESSAGE_QUEUE_SIZE;

	PIN_MSG = mq_open(PIN_MSG_NAME , O_CREAT | O_RDWR, 0666, &mq_attribute);
	if (PIN_MSG == -1){
		perror("creating pin message queue failed ");
	}


	mq_attribute.mq_msgsize = (size_t) DB_MESSAGE_QUEUE_SIZE;

	DB_MSG = mq_open(DB_MSG_NAME, O_CREAT | O_RDWR, 0666, &mq_attribute);
	if (DB_MSG == -1){
		perror("creating db message queue failed ");
	}
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
	Pin_msg_struct message;
	DB_msg_struct rcv_message;
	
	lockAndPrint("ATM is running\n");

	while(1){
		lockAndPrint("Please input account number > ");
		cin >> accountNumber;
		lockAndPrint("Please input PIN > ");
		cin >> PIN;

		for (int i = 0; i < 5; ++i)
		{
			message.acctNum[i] = accountNumber[i];
		}
		for (int i = 0; i < 3; ++i)
		{
			message.PIN[i] = PIN[i];
		}

		status = mq_send(PIN_MSG, (const char*) &message, sizeof(message), 1);
		if (status < 0){
			perror("sending PIN message failed");
			exit(0);
		}

		status = mq_receive(DB_MSG, (char*) &rcv_message, sizeof(rcv_message), NULL);
		if (status < 0){
			perror("receiving DB message failed");
			exit(0);
		}
		if(arrayEquals(rcv_message.message, "OKAY", 4)) cout << "OKAY" << endl;


	}
}


void* run_DB(void* arg){
	FILE* database;
	char working_acct_number[5];
	int working_acct_number_attempts = 0;

	char line[20];

	lockAndPrint("Database server running\n");
	int status;
	Pin_msg_struct rcv_message;
	DB_msg_struct message;

	database = fopen(DATABASE_FILENAME, "rw");
	if(database == NULL){
		lockAndPrint("ERROR - FILE NOT FOUND");
		exit(0);
	}

	while(1){
		status = mq_receive(PIN_MSG, (char*) &rcv_message, sizeof(rcv_message), NULL); //we have the PIN and acctNum here, for safety, only use the first 5 and 3 chars respectively
		if (status < 0){
			perror("receiving PIN message failed ");
			exit(0);
		}
		lockAndPrint("Message received - checking for valid input\n");
		char* validReturn;
		do{
			validReturn = fgets(line, 20, database);
			if (validReturn == NULL) break;
		} while(!arrayEquals(line, rcv_message.acctNum, 5)); //if the first 5 chars of the line match the 5 chars in accountNum)
		//if we get here, either we broke from the loop due to EOF, or we matched successfully
		if(validReturn == NULL) {
			memcpy(message.message, "NOT OK", strlen("NOT OK") + 1); //send a NOT OKAY message
		} else {
			char decryptedPin[3];
			memcpy(decryptedPin, decryptPIN(rcv_message.PIN), 3 + 1);
			if (arrayEquals(&line[6], decryptedPin, 3)){
				memcpy(message.message, "OKAY", strlen("OKAY") + 1); //send an OKAY message
			} else cout << "pin was not valid "; //add counter here
		}

		status = mq_send(DB_MSG, (const char*) &message, sizeof(message), 1);
		if (status < 0) perror("error sending db message");

	}
}
