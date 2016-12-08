#include <iostream>
#include <math.h>
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
#include <unistd.h>


using namespace std;

#define PIN_MSG_NAME "/pin_msg"
#define DB_MSG_NAME "/db_msg"

#define DATABASE_FILENAME "../database.db"
#define TEMPFILE_FILENAME "../tempFile.db"

typedef struct Pin_msg_struct{ //sent over PIN_MSG
	char acctNum[6];
	char PIN[4];
} Pin_msg_struct;

typedef struct Fund_request_struct{ //send over PIN_MSG
	char requestType[2]; //FR for request funds, WF for withdraw, RR returned request, RW returned withdraw
	char fundRequest[8];
} Fund_request_struct;

typedef struct DB_msg_struct{ //sent over DB_MSG
	char message[6]; //only thing for now
} DB_msg_struct;

#define PIN_MESSAGE_QUEUE_SIZE sizeof(Pin_msg_struct)
#define DB_MESSAGE_QUEUE_SIZE sizeof(DB_msg_struct)

pthread_t ATM;
pthread_t DB_server;
pthread_t DB_editor;
void* run_ATM(void* arg);
void* run_DB(void* arg);
void* run_DB_editor(void* arg);

static struct mq_attr mq_attribute;
static mqd_t PIN_MSG = -1;
static mqd_t DB_MSG = -1;

pthread_mutex_t printing_lock;
pthread_mutex_t file_working_lock;
pthread_mutex_t editorAwake;

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

char* intToCharArray(int num){
	int n = log10(num);
	char* arr = (char*) calloc(n + 1, sizeof(char));
	for (int i = n; i >= 0; --i, num /= 10)
	{
		arr[i] = (num % 10) + '0';
	}
	return arr;
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
	cout.precision(9);	
	pthread_attr_t attr;

	signal(SIGINT, sig_handler);


	/*************************MUTEX INIT***************/
	int rc = pthread_mutex_init(&printing_lock, NULL);
	if (rc < 0){
		perror("Error creating mutex");
	}
	rc = pthread_mutex_init(&file_working_lock, NULL);
	if (rc < 0){
		perror("Error creating mutex");
	}
	rc = pthread_mutex_init(&editorAwake, NULL);
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

	pthread_mutex_lock(&editorAwake); //the editor starts asleep

	/****************PTHREAD INIT AND RUN************/
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024*1024);

	long start_arg = 0; //the start argument is unused right now
	pthread_create(&ATM, NULL, run_ATM, (void*) start_arg);
	pthread_create(&DB_server, NULL, run_DB, (void*) start_arg);
	pthread_create(&DB_editor, NULL, run_DB_editor, (void*) start_arg);
	/***************************************************/


	pthread_join(ATM, NULL);
	pthread_join(DB_server, NULL);
	pthread_join(DB_editor, NULL);

	/******************MUTEX DESTROY******************/
	pthread_mutex_destroy(&printing_lock);
	pthread_mutex_destroy(&file_working_lock);
	pthread_mutex_destroy(&editorAwake);

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
		if (arrayEquals(accountNumber, "DBEDT", 5)){
			lockAndPrint("Waking up DB editor\n");
			pthread_mutex_unlock(&editorAwake); //wake up the editor
			usleep(100000);
			pthread_mutex_lock(&editorAwake);
		} else {
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
			if(arrayEquals(rcv_message.message, "OKAY", 4)){
				
				char inputVal[20];
				char fundRequest[10];
				Fund_request_struct frs;			

				lockAndPrint("Login successful, requestFunds or withdraw-(quantity) > ");


				while(1){
					cin >> inputVal;
					if (arrayEquals(inputVal, "requestFunds", 13)){
						memcpy(frs.requestType, "FR", 2);					
						mq_send(PIN_MSG, (const char*) &frs, sizeof(frs), 1);
						mq_receive(PIN_MSG, (char *) &frs, sizeof(frs), NULL);
						pthread_mutex_lock(&printing_lock);
						cout << "Fund request made, funds available : " << frs.fundRequest << endl;
						pthread_mutex_unlock(&printing_lock);
						break;
					}
					if (arrayEquals(inputVal, "withdraw", 8)) {
						memcpy(frs.requestType, "WF", 2);
						memcpy(frs.fundRequest, &inputVal[9], 11);
						mq_send(PIN_MSG, (char*) &frs, sizeof(frs), 1);
						mq_receive(PIN_MSG, (char*) &frs, sizeof(frs), NULL);
						cout << "Withdrawal result: " << frs.fundRequest << endl;
						break;
					} 				
					lockAndPrint("Invalid input, please try again > ");
				}


			} else lockAndPrint("Username or password was incorrect\n");
		}
	}
}


void* run_DB(void* arg){
	FILE* database;
	char working_acct_number[5];
	int working_acct_number_attempts = 0;

	// bool dirty_file = 0; //file is modified and has to be written out to file, then this bit can be cleared. If dirty_file is set, we should have file_working_lock.

	char line[20];

	lockAndPrint("Database server running\n");
	int status;
	Pin_msg_struct rcv_message;
	DB_msg_struct db_message;
	Fund_request_struct fr_message;


	while(1){
		status = mq_receive(PIN_MSG, (char*) &rcv_message, sizeof(rcv_message), NULL); //we have the PIN and acctNum here, for safety, only use the first 5 and 3 chars respectively
		if (status < 0){
			perror("receiving PIN message failed ");
			exit(0);
		}
		lockAndPrint("Message received - checking for valid input\n");
		char* validReturn;
		pthread_mutex_lock(&file_working_lock);
		database = fopen(DATABASE_FILENAME, "r+");
		if(database == NULL){
			lockAndPrint("ERROR - FILE NOT FOUND");
			exit(0);
		}

		do{
			validReturn = fgets(line, 20, database);
			if (validReturn == NULL) break;
		} while(!arrayEquals(line, rcv_message.acctNum, 5)); //if the first 5 chars of the line match the 5 chars in accountNum)

		//if we get here, either we broke from the loop due to EOF, or we matched successfully
		fclose(database);
		pthread_mutex_unlock(&file_working_lock);
		if(validReturn == NULL) {
			memcpy(db_message.message, "NOT OK", strlen("NOT OK") + 1); //send a NOT OKAY message
		} else {
			
			if (!arrayEquals(working_acct_number, rcv_message.acctNum, 5)) memcpy(working_acct_number, rcv_message.acctNum, 5); //update the working account number
			char decryptedPin[3];
			memcpy(decryptedPin, decryptPIN(rcv_message.PIN), 3 + 1);
			if (arrayEquals(&line[6], decryptedPin, 3)){
				memcpy(db_message.message, "OKAY", strlen("OKAY") + 1); //send an OKAY message
			} else {
				memcpy(db_message.message, "NOT OK", strlen("NOT OK") + 1); //send a NOT OKAY message
				working_acct_number_attempts++;
			}

		}

		if (working_acct_number_attempts > 3){ //lock the account
			pthread_mutex_lock(&file_working_lock); //the file is not dirty, we don't have the lock. If we want to write to the file, we need the lock
				database = fopen(DATABASE_FILENAME, "r+");
				if(database == NULL){
					lockAndPrint("ERROR - FILE NOT FOUND");
					exit(0);
				}
				fseek(database, -strlen(line), SEEK_CUR);
				fprintf(database, "X");
				fclose(database);
			pthread_mutex_unlock(&file_working_lock);

			lockAndPrint("Too many bad password attempts - account locked\n");
		}

		status = mq_send(DB_MSG, (const char*) &db_message, sizeof(db_message), 1);
		if (status < 0) perror("error sending db message");

		status = mq_receive(PIN_MSG, (char*) &fr_message, sizeof(fr_message), NULL);
		if (status < 0) perror("error receiving fundRequest message");

		if (arrayEquals(fr_message.requestType, "FR", 2)){ //it's a fund request, send back the funds in this account
			memcpy(fr_message.requestType, "RR", 2);
			for (int i = 0; (i < 8) && (line[10+i] != '\n'); ++i)
			{
				fr_message.fundRequest[i] = line[10+i];
			}
		} else if (arrayEquals(fr_message.requestType, "WF", 2)){ //it's a withdraw funds request - try to withdraw the funds and send back "ENOUGH" or "LOW FUNDS"
			lockAndPrint("Withdrawing funds\n");
			pthread_mutex_lock(&file_working_lock); //lock the file
				database = fopen(DATABASE_FILENAME, "r+");
				if(database == NULL){
					lockAndPrint("ERROR - FILE NOT FOUND");
					exit(0);
				}

				FILE* newFile;
				char* vr;
				char mLine[20];
				
				rewind(database);
				newFile = fopen(TEMPFILE_FILENAME, "w+");
				if(newFile == NULL){
					lockAndPrint("ERROR OPENING TEMPORARY FILE");
					exit(0);
				}			

				vr = fgets(mLine, 20, database);

				while(1){ //exit this loop when it's broken by vr==NULL
					while(!arrayEquals(mLine, rcv_message.acctNum, 5)) { //if the first 5 chars of the line match the 5 chars in accountNum)
						// cout << mLine << endl; //DEBU
						fprintf(newFile, "%s", mLine);
						if (vr == NULL) break;
						vr = fgets(mLine, 20, database);
					} 
					if (vr == NULL) break; //we hit EOF, we're done and we can leave the outer loop
					//if we get here, we didn't hit the end of the file, that means we're at the line we want to modify.
					float money = atof(&line[10]);
					float withdrawal = atof(fr_message.fundRequest);
					if (withdrawal <= money) { //if there's enough money
						money -= withdrawal;
						for (int i = 0; i < 10; ++i)
						{	
							fprintf(newFile, "%c", mLine[i]); //the first half of this line is unchanged
						}
						fprintf(newFile, "%.2f\n", money); //print out the new money
						memcpy(fr_message.fundRequest, "ENOUGH", strlen("ENOUGH")); //this is what we're going to send
					} else {
						memcpy(fr_message.fundRequest, "LO FUNDS", strlen("LO FUNDS"));
						fprintf(newFile, "%s", mLine);
					}
					vr = fgets(mLine, 20, database); //fetch the next line for the processing loop
				}
				memcpy(fr_message.requestType, "RW", 2);
				fflush(newFile);
				remove(DATABASE_FILENAME);
				rename(TEMPFILE_FILENAME, DATABASE_FILENAME);

				fclose(database);
			pthread_mutex_unlock(&file_working_lock); //we're done with the file now
		}
		mq_send(PIN_MSG, (const char*) &fr_message, sizeof(fr_message), 1);

	}
	fclose(database);
}


void * run_DB_editor(void* arg){
	FILE* databasePointer;
	while(1){
		char inputArray[20];
		pthread_mutex_lock(&editorAwake);
		lockAndPrint("DB editor awake, please input a new entry > ");
		cin >> inputArray;
		pthread_mutex_lock(&file_working_lock); //going to do file work, we need the file lock.
			databasePointer = fopen(DATABASE_FILENAME, "a");
			if (databasePointer == NULL)lockAndPrint("ERROR - DATABASE FILE DOES NOT EXIST\n");
			fprintf(databasePointer, "%s\n", inputArray);
			fclose(databasePointer);
		pthread_mutex_unlock(&file_working_lock);
		pthread_mutex_unlock(&editorAwake);
		usleep(10000);
	}
}