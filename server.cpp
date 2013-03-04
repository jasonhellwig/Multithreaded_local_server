#include <iostream>
#include <fstream>
#include <list>
#include <cstdlib>
#include <ctime>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <cstring>
#include <signal.h>

using namespace std;

//constants-------------------------------------------------------------------------
const int TABLE_SIZE = 100;
const int BUFFER_SIZE = 256;
const int RECORD_MAKER_NUM = 5;
const long MSG_RECEIVE = 2;
const long MSG_SEND = 3;
const int SLEEP_TIME = 5;
const int MAX_RETRY = 1000;


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

//Hash Table-----------------------------------------------------------------------
class HashTable
{
private:
	//cell definition
	struct Cell
	{
		pthread_mutex_t lock;
		list<Record> linkedList;
	};
	//the table
	Cell table[TABLE_SIZE];

	//hashing function
	int hash(int key)
	{	return key%TABLE_SIZE;}

public:
	HashTable();
	void insert(Record data);
	Record search(int id);
	void deallocate_mutex();
};

HashTable::HashTable()
{
	//initialize the mutexes
	for (int i=0; i<TABLE_SIZE; ++i)
	{
		pthread_mutex_init(&table[i].lock,NULL);
	}
}

void HashTable::insert(Record new_record)
{
	//get cell value
	int value = hash(new_record.id);
	
	//lock the mutex at the value
	if (pthread_mutex_lock(&table[value].lock) < 0)
	{
		cerr<<"Table lock error\n";
		exit(-1);
	}
		
	//insert a value into the linked list at the cell
	table[value].linkedList.push_front(new_record);
	
	//unlock value
	if (pthread_mutex_unlock(&table[value].lock) < 0)
	{
		cerr<<"Table unlock error\n";
		exit(-1);
	}
}

Record HashTable::search(int id)
{
	//get cell value
	int value = hash(id);
	
	Record temp;
	
	//lock
	if (pthread_mutex_lock(&table[value].lock) < 0)
	{
		cerr<<"Table lock error\n";
		exit(-1);
	}
	
	//search for a value in the Linked List
	list<Record>::iterator i;
	for (i = table[value].linkedList.begin(); i != table[value].linkedList.end(); ++i)
	{
		if (i->id == id)
		break;		
	}
	
	//if not found create a record with a negative id
	 if (i == table[value].linkedList.end())
		temp.id	= -1;
	else
	{
		temp.id = i->id;
		strncpy(temp.first,i->first,BUFFER_SIZE);
		strncpy(temp.last,i->last,BUFFER_SIZE);
	}	
	//unlock value
	if (pthread_mutex_unlock(&table[value].lock) < 0)
	{
		cerr<<"Table unlock error\n";
		exit(-1);
	}
	
	return temp;	
}

void HashTable::deallocate_mutex()
{
	for(int i=0;i<TABLE_SIZE;++i)
	{
		if(pthread_mutex_destroy(&table[i].lock)<0)
		{
			cerr<<"Mutex destroy\n";
			exit(-1);
		}
	}
}

//Global variables-------------------------------------------------------------

//hashtable
HashTable data;

//work queue
list<int> work_queue;
pthread_mutex_t work_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t work_queue_cond = PTHREAD_COND_INITIALIZER;


//signal (will use later to terminate program)
bool run_signal = true;



//Functions--------------------------------------------------------------------

void readFile(char*,HashTable&);
void* do_work(void*);
void make_work(key_t, int);
void* make_records(void*);
void signalHandler(int);

//main-------------------------------------------------------------------------

int main(int argc, char* argv[])  //main takes 2 arguments
{
	//check number of arguments
	if (argc != 3)
	{
		cerr<<"Invalid number of arguments\n"
		<<"USAGE: ./server [filename of records] [# of threads]\n";
		exit(-1);
	}	
	//read records from file into table
	readFile(argv[1],data);
	
	//create message queue key
	key_t key = ftok("/bin/ls",'J');
	if (key < 0)
	{
		cerr<<"Message queue key error\n";
		exit(-1);
	}
	
	//create a message queue
	int msqid = msgget(key, 0666 | IPC_CREAT);
	if (msqid < 0)
	{
		cerr<<"Message queue creation error\n";
		exit(-1);
	}
	
	
	//create worker threads
	int threads_to_make = atoi(argv[2]);
	pthread_t * workers = new pthread_t[threads_to_make];
	for (int i = 0; i < threads_to_make; ++i)
	{
		if (pthread_create(&workers[i],NULL,&do_work,NULL)<0)
		{
			cerr<<"failed to creaked worker thread\n";
			exit(-1);
		}
	}
	
	//create record making threads
	pthread_t record_makers[RECORD_MAKER_NUM];
	for (int i = 0; i<RECORD_MAKER_NUM;i++)
	{
		if(pthread_create(&record_makers[i],NULL,&make_records,NULL)<0)
		{
			cerr<<"failed to create record maker thread\n";
			exit(-1);
		}
	}
	
	//overide default signal handler for ctrl-c
	signal(SIGINT,signalHandler);
	
	//get messages for workers
	make_work(key, msqid);
	
	//deallocate stuff
	
	if(msgctl(msqid,IPC_RMID,NULL)<0)
	{
		cerr<<"msgctl\n";
	}
	for (int i=0;i<threads_to_make;++i)
	{
		if(pthread_kill(workers[i],SIGINT)<0)
		{
			cerr<<"pthread kill\n";
		}
	}
	for (int i=0;i<RECORD_MAKER_NUM;++i)
	{
		if(pthread_kill(record_makers[i],SIGINT)<0)
		{
			cerr<<"pthread kill\n";
		}
	}
	if(pthread_mutex_destroy(&work_queue_mutex)<0)
	{
		cerr<<"mutex destroy\n";
	}
	if(pthread_cond_destroy(&work_queue_cond)<0)
	{
		cerr<<"cond destroy\n";
	}
	data.deallocate_mutex();

	

	
		
	return 0;

}

//readFile------------------------------------------------------------------------------
void readFile(char* filename, HashTable& table)
{
	ifstream fin;
	fin.open(filename);

	//check if file is open
	if (!fin)
	{
		cerr<<"Could not open file\n";
		exit(-1);
	}

	Record temp;

	//read entries into hash table
	while (!fin.eof())
	{
		fin>>temp.id;
		fin>>temp.first;
		fin>>temp.last;

		table.insert(temp);		
	}
	fin.close();
}

//do_work----------------------------------------------------------------------------
//worker threads will go here to serve requests
void* do_work(void* args)
{	
	//set up message queue information for sending a message back to the client
	Record result;
	msg_buff to_client;
	to_client.messageType = MSG_SEND;
	key_t key = ftok("/bin/ls",'J');
	if(key < 0)
	{
		cerr<<"ftok\n";
		pthread_exit(NULL);
	}
	int msgqid = msgget(key,0666);
	if(msgqid < 0)
	{
		cerr<<"ftok\n";
		pthread_exit(NULL);
	}
		
	
	//run forever unless signaled to stop	
	while(run_signal)
	{
		//lock the work queue mutex so no other threads can access
		if(pthread_mutex_lock(&work_queue_mutex)<0)
		{
			cerr<<"work queue mutex failed\n";
			pthread_exit(NULL);
		}
		
		//if there is nothing to work on, then sleep
		while(work_queue.empty())
		{
			//wait for signal that work has been added
			if(pthread_cond_wait(&work_queue_cond, &work_queue_mutex) < 0)
			{
				cerr<<"pthread_cond_wait\n";
				pthread_exit(NULL);
			}
		}
		
		//do work		
		int id_to_fetch = work_queue.back();
		work_queue.pop_back();
		
		//access to work_queue is done, so unlock the mutex
		if (pthread_mutex_unlock(&work_queue_mutex)<0)
		{
			cerr<<"pthread_mutex_unlock";
			pthread_exit(NULL);
		}		
		
		//copy data into the message buffer
		result = data.search(id_to_fetch);
		to_client.message_info.id = result.id;
		strncpy(to_client.message_info.first,result.first,BUFFER_SIZE);
		strncpy(to_client.message_info.last,result.last,BUFFER_SIZE);
		
		//pass the result in the message queue to the client		
		if(msgsnd(msgqid,&to_client,sizeof(to_client)-sizeof(long),0)<0)
		{
			cerr<<"msgsnd";
			pthread_exit(NULL);
		}			
	}
	return NULL;
}

//make work-------------------------------------------------------------------------
//gets messages from the message queue and puts them into the work queue for workers
void make_work(key_t key, int msgqid)
{
	msg_buff message;
	int id;
	
	//run until program is signaled to end
	while(run_signal)
	{
		//get data from message queue
		if (msgrcv(msgqid, &message, sizeof(message)-sizeof(long),MSG_RECEIVE,0)<0)
		{
			cerr<<"message queue error\n";
			return;
		}
		
		id = message.message_info.id;	
		
		//lock the work queue mutex before adding value
		if(pthread_mutex_lock(&work_queue_mutex)<0)
		{
			cerr<<"mutex lock error\n";
			return;
		}
		
		//add work item to work queue
		work_queue.push_front(id);
		
		//wake up a worker
		if(pthread_cond_signal(&work_queue_cond)<0)
		{
			cerr<<"work queue signal error\n";
			return;
		}
		
		//unlock the work queue
		if(pthread_mutex_unlock(&work_queue_mutex)<0)
		{
			cerr<<"work queue unlock error\n";
			return;
		}
	}
}

//make records-------------------------------------------------------------------------
//makes random records that will be placed into the hash table
void* make_records(void* arg)
{	
	Record new_record;
	Record search_result;
	strncpy(new_record.first,"Bob",3);
	strncpy(new_record.last,"Generated",9);
	srand(time(NULL));
	
	while(run_signal)
	{
		//generate a random record id		
		new_record.id = rand()%10000;
		search_result = data.search(new_record.id);
		
		//if id is already used, retry 1000 more times and then give up
		//prevents infinite looping when most of the id's have been picked
		if (search_result.id != -1)
		{
			for (int i=0;i<MAX_RETRY;++i)
			{
				new_record.id = rand()%10000;
				search_result = data.search(new_record.id);
				if (search_result.id == -1)
					break;
			}
			if (search_result.id != -1)
			{
				pthread_exit(NULL);
			}
		}
				
		//add record to hash table
		data.insert(new_record);
		
		//wait five seconds
		sleep(SLEEP_TIME);
				
	}
	return NULL;
}

//signalHandler-----------------------------------------------------------------
//overides the ctrl-c functionality
void signalHandler(int arg)
{
	run_signal = false;
}









