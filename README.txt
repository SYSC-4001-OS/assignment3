*********************************************************
*	______ _____  ___ ______  ___  ___ _____ 			*
*	| ___ \  ___|/ _ \|  _  \ |  \/  ||  ___|			*
*	| |_/ / |__ / /_\ \ | | | | .  . || |__  			*
*	|    /|  __||  _  | | | | | |\/| ||  __| 			*
*	| |\ \| |___| | | | |/ /  | |  | || |___ 			*
*	\_| \_\____/\_| |_/___/   \_|  |_/\____/ 			*
*														*
*********************************************************

Assignment 3
SYSC 4001

Jason Van Kerkhoven			[100974276]
Brydon Gibson				[100975274]

09/12/2016


-----------------------------------------------------------
CONTENTS:

	* main.cpp
	* main
	* database.db
	* README.txt
	* SYSC4001-A3-VanKerkhoven-Gibson

	


-----------------------------------------------------------
SET UP & COMPELATION:

	1.	Set the working directory to the directory containg "main.cpp".
	2.	Type the following command into the terminal (not including quotations):
		"g++ -o main main.cpp -pthread -lrt"
	3.	To run the file, enter the following:
		"./main"
	4.	The terminal should then display an output indicating the
		database server thread is running, as well as prompting the
		user for input on the ATM thread.
	5.	Refer to the OPERATIONS MANUAL section for runtime information.


	
	
-----------------------------------------------------------
OPERATIONS MANUAL:

	LOGIN TO AN ACCOUNT
	=======================
	You will be prompted to enter an account number. The number
	must be entered as a five [5] digit number (ex 12345, 00117, etc.).
	After entering the account number, you will be prompted to enter a
	PIN. The PIN is entered as a three [3] digit number (ex 123, 108, etc.).
	
	It should be noted that the PIN that is saved in the database.db file
	is an encrypted PIN. So, the PIN you enter in the terminal != the PIN
	saved in the database. The encryption used is the PIN in the database
	is -1. For example, if the PIN is saved in the database as 009, the
	unencrypted PIN you enter in the terminal is 010.
	
	
	ACCOUNT OPTIONS
	=======================
	Once in an account, you have two options; checking the current balance,
	and making a withdrawl. The former is done by entering the command
	"requestFunds", and the later is done by entering the command "withdraw-A",
	where A is the number of dollars you wish to withdraw.
	
	
	LAUNCHING DBEDT
	=======================
	When the terminal prompts you to ender account number, enter "DBEDT" to
	launch the backdoor database editor. You will then be prompted to enter
	a new entry into the database. The entry must be entered in the following
	format:
		"aaaaa,ppp,D"
	Where 'aaaaa' is a five [5] digit account number, 'ppp' is a three [3]
	encrypted PIN, and D is the amount of dollars.
	
	
	READING THE DATABASE
	=======================
	The database, "database.db", is read with each account being saved
	as its own line. Each line contains details on one account, each account
	parameter is seperated by a comma. The first number must be five [5] digits
	and is read as the account number. The second number must be three [3]
	digits and is read as the encrypted PIN. The third number
	(undefined length) is read as the current account funds.
	
	If the account is locked due to too many incorrect PINs being inputed in
	a row, the account number will have an 'X' char preceding it, denoting the
	account cannot be accessed.
	
	
	
	
	
	