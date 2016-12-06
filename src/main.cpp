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

#define MESSAGE_QUEUE_SIZE 100

pthread_t ATM;
pthread_t DB_server;
pthread_t DB_editor;
void* run_ATM(void* arg);
void* run_DB(void* arg);

static struct mq_attr mq_attribute;
static mqd_t PIN_MSG, DB_MSG;

void sig_handler(int signum){
	//ASSERT(signum == SIGINT);

	cout << "killing application" << endl;

	pthread_cancel(ATM);
	pthread_cancel(DB_server);
	pthread_cancel(DB_editor);
}

int main(int argc, char const *argv[])
{
	pthread_attr_t attr;

	signal(SIGINT, sig_handler);

	mq_attribute.mq_maxmsg = 10; //mazimum of 10 messages in the queue at the same time
	mq_attribute.mq_msgsize = MESSAGE_QUEUE_SIZE;

	PIN_MSG = mq_open(PIN_MSG_NAME, O_CREAT | O_RDWR /*| O_NONBLOCK*/, 0666, &mq_attribute);
	DB_MSG = mq_open(DB_MSG_NAME, O_CREAT | O_RDWR /*| O_NONBLOCK*/, 0666, &mq_attribute);

	//ASSERT(messageQueue != -1);

	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024*1024);

	long start_arg = 0; //the start argument is unused right now
	pthread_create(&ATM, NULL, run_ATM, (void*) start_arg);
	pthread_create(&DB_server, NULL, run_DB, (void*) start_arg);

	pthread_join(ATM, NULL);
	pthread_join(DB_server, NULL);

	sig_handler(SIGINT);
}

void* run_ATM(void* arg) {
	int status;
	long accountNumber;
	long PIN;

	cout << "ATM is running" << endl;
	cout << "Please input an account number > ";
	cin >> accountNumber;
	status = mq_send(PIN_MSG, (const char*) &accountNumber, sizeof(accountNumber), 1);


	cout << endl << "Please input your PIN > ";
	cin >> PIN;
	cout << endl;
	status = mq_send(PIN_MSG, (const char*) &PIN, sizeof(PIN), 1);
}

void* run_DB(void* arg){
	cout << "Database server running" << endl;
	int status;
	long received_acct_number;
	long received_PIN;

	while(1){
		status = mq_receive(PIN_MSG, (char*) &received_acct_number, sizeof(received_acct_number), NULL);
		cout << "received account number\t" << received_acct_number << endl;
		status = mq_receive(PIN_MSG, (char*) &received_PIN, sizeof(received_PIN), NULL);
		cout << "received PIN\t" << received_PIN << endl;
	}
}
