A local multithreaded server that communicates with clients using System IV message queues on linux.  Implemented using C++.

Execution:  start the server using ./server [a file of records] [number of threads to use]


partIRecords.dat is a records file that can be used if none is available.

Next, start the client using ./client

The client will rapidly start showing successfully retrieved records from the server
To quit use ctrl-c on the server; this should also kill the client.