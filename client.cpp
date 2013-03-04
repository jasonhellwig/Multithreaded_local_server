#include <iostream>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <cstdlib>
#include <ctime>
#include <cstring>

using namespace std;

//constants------------------------------------------------------------------------
const long MSG_RECEIVE=3;
const long MSG_SEND=2;
const int BUFFER_SIZE=256;

//record struct---------------------------------------------------------------------
struct Record
{
	int id;
	char first[BUFFER_SIZE];
	char last[BUFFER_SIZE];
};

//message struct---------------------------------------------------------------------
struct msg_buff
{
	long messageType;
	Record message_info;
};

bool run_signal = true;



int main()
{
	srand(time(NULL));
	
	msg_buff from_server;
	msg_buff to_server;
	to_server.messageType = MSG_SEND;

	
	//get message queue information
	key_t key = ftok("/bin/ls",'J');
	if (key < 0)
	{
		cerr<<"ftok\n";
		exit(-1);
	}
	int msgqid = msgget(key,0666);	
	if(msgqid < 0)
	{
		cerr<<"msgget\n";
		exit(-1);
	}	
	
	//run until signal is sent
	while(run_signal)
	{
		//generate a random id
		to_server.message_info.id = rand()%10000;		
		
		
		//send id to server
		if(msgsnd(msgqid,&to_server,sizeof(to_server)-sizeof(long),0)<0)
		{
			cerr<<"msgsnd\n";
			exit(-1);
		}	
			
		//receive message from server
		if(msgrcv(msgqid,&from_server,sizeof(from_server)-sizeof(long),MSG_RECEIVE,0)<0)
		{
			cerr<<"msgrcv\n";
			exit(-1);
		}	
				
		//display records successfully retreived
		if(from_server.message_info.id != -1)
		{
			cout<<"ID:"<<from_server.message_info.id<<"  first name:"<<from_server.message_info.first
			<<"  last name:"<<from_server.message_info.last<<endl;
		}
		
		//uncomment to display failed lookups on the server
		/*
		else 
		{
			cout<<"Record not found.\n";
		}
		*/
		
	}
	return 0;
}
