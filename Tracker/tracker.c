#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#define BUFFSIZE 256

void syserr(char *msg) { perror(msg); exit(-1); }
void readandsend(int tempfd, int newsockfd, char* buffer);
void * trccomm(void * s);
void recvandwrite(int tempfd, int newsockfd, int size, char* buffer);
//Shared List
typedef struct l {

  char * filename; //Tallies which slots are being used
  uint32_t clientIP;
  int portnum;
  struct l * fl_next;
} fileList;

fileList * head = NULL;
fileList * curr = NULL;
fileList * tail = NULL;
int listLen = 0;

pthread_mutex_t llock; // List Lock
pthread_t pthread; //peer thread

//Thread struct
typedef struct s {
   int nsock;
   struct sockaddr_in* clientInfo;
} sockStruct;
   

int main(int argc, char *argv[])
{
  int sockfd, newsockfd, portno;
  struct sockaddr_in serv_addr, clt_addr;
  socklen_t addrlen;

  

  if(argc != 2) 
  { 
    portno = 5000;
    printf("Default port is: %d\n", portno);
  }
  else
  { 
  	portno = atoi(argv[1]);
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0); 
  if(sockfd < 0) syserr("can't open socket"); 
  	printf("create socket...\n");

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  if(bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) 
    syserr("can't bind");
  printf("bind socket to port %d...\n", portno);

  listen(sockfd, 5); 

for(;;) {
  printf("wait on port %d...\n", portno);
  addrlen = sizeof(clt_addr); 
  newsockfd = accept(sockfd, (struct sockaddr*)&clt_addr, &addrlen);
  if(newsockfd < 0) syserr("can't accept");
  
  sockStruct * s;
  s = (sockStruct *)malloc(sizeof(sockStruct));
  s -> nsock = newsockfd;
  s -> clientInfo = &clt_addr;
  
  pthread_mutex_init(&llock, NULL); 
  if(pthread_create(&pthread, NULL, trccomm, (void *) s))
  	syserr("Thread was not created.\n");
  close(sockfd);
}
  close(sockfd); 
  return 0;
}

void * trccomm(void * s)
{
	int n, size, tempfd, newsockfd, clientPort;
	uint32_t clientIP;
    struct stat filestats;
	char * filename;
	char command[20];
    char buffer[256];
	filename = malloc(sizeof(char)*BUFFSIZE);
	
	//Thread struct operations
	sockStruct* sCInfo = (sockStruct *) s;
	newsockfd = sCInfo -> nsock;
	clientIP = sCInfo -> clientInfo -> sin_addr.s_addr;
	clientPort = sCInfo -> clientInfo -> sin_port;
	
	//Populate list with client files, IP and port
	pthread_mutex_lock(&llock);
	for(;;)
	{
	    memset(buffer, 0, BUFFSIZE);
		n = recv(newsockfd, buffer, BUFFSIZE, 0);
		if(n < 0) syserr("can't receive from client");
		if(strcmp(buffer, "EndOfList") != 0) break;
		curr = (fileList *)malloc(sizeof(fileList));
		curr -> clientIP = clientIP;
		curr -> portnum = clientPort;
		strcpy(curr -> filename, buffer);
		
		if(tail == NULL)
		{
		  tail = curr;
		  head = curr;
		  listLen++;
		}
		else
		{
		  tail -> fl_next = curr;
		  tail = curr;
		  listLen++;
		}			
	}
	pthread_mutex_unlock(&llock);
	
	for(;;)
	{
	    memset(buffer, 0, BUFFSIZE);
		n = recv(newsockfd, buffer, BUFFSIZE, 0);
		//printf("amount of data recieved: %d\n", n);
		if(n < 0) syserr("can't receive from client");
		sscanf(buffer, "%s", command);
		//printf("message from client is: %s\n", buffer);
		
		if(strcmp(command, "list") == 0)
		{
			pthread_mutex_lock(&llock);
            size = htonl(listLen);      
			n = send(newsockfd, &size, sizeof(int), 0);
		    if(n < 0) syserr("couldn't send listLen to client");
		    curr = head;
			while(curr)
			{
				memset(buffer, 0, BUFFSIZE);
				strcpy(buffer, curr -> filename);
				n = send(newsockfd, &buffer, BUFFSIZE, 0);
		    	if(n < 0) syserr("couldn't send filename to client");
				int cIP = htonl(clientIP); 
				n = send(newsockfd, &cIP, sizeof(uint32_t), 0);
		    	if(n < 0) syserr("couldn't send clientIP to client");
				int cP = htonl(clientPort); 
				n = send(newsockfd, &cP, sizeof(int), 0);
		    	if(n < 0) syserr("couldn't send clientIP to client");
		    	
		    	curr = curr -> fl_next;
			}
			//readandsend(tempfd, newsockfd, buffer);
			//close(tempfd);			
		}
		
		if(strcmp(command, "exit") == 0)
		{
			printf("Connection to client shutting down\n");
			int i = 1;
			i = htonl(i);
			n = send(newsockfd, &i, sizeof(int), 0);
		    if(n < 0) syserr("didn't send exit signal to client");
			break;  // check to make sure it doesn't need to be exit
		}
		
		if(strcmp(command, "get") == 0)
		{
			sscanf(buffer, "%*s%s", filename);
			//printf("filename from client is: %s\n", filename);
			//printf("size of filename is: %lu\n", sizeof(filename));
			stat(filename, &filestats);
			size = filestats.st_size;
			//printf("Size of file to send: %d\n", size);
            size = htonl(size);      
			n = send(newsockfd, &size, sizeof(int), 0);
		    if(n < 0) syserr("couldn't send size to client");
			//printf("The amount of bytes sent for filesize is: %d\n", n);
			tempfd = open(filename, O_RDONLY);
			if(tempfd < 0) syserr("failed to open file");
			readandsend(tempfd, newsockfd, buffer);
			close(tempfd);			
		}
		if(strcmp(command, "put") == 0)
		{
			//Parse filename
			sscanf(buffer, "%*s%s", filename);
			//printf("filename from client is: %s\n", filename);
			//printf("size of filename is: %lu\n", sizeof(filename));
			//Receieve size of file
			n = recv(newsockfd, &size, sizeof(int), 0); 
        	if(n < 0) syserr("can't receive from server");
        	size = ntohl(size);        
			if(size ==0) // check if file exists
			{
				printf("File not found\n");
				break;
			}
			//printf("The size of the file to recieve is: %d\n", size);
			//Receieve the file
			tempfd = open(filename, O_CREAT | O_WRONLY, 0666);
			if(tempfd < 0) syserr("failed to open the file");
			recvandwrite(tempfd, newsockfd, size, buffer);
			close(tempfd);
		}
	}
	return 0;
}

void readandsend(int tempfd, int newsockfd, char* buffer)
{
	while (1)
	{
		memset(buffer, 0, BUFFSIZE);
		int bytes_read = read(tempfd, buffer, BUFFSIZE); //is buffer cleared here?
		buffer[bytes_read] = '\0';
		if (bytes_read == 0) // We're done reading from the file
			break;

		if (bytes_read < 0) syserr("error reading file");
		//printf("The amount of bytes read is: %d\n", bytes_read); 
		
		int total = 0;
		int n;
		int bytesleft = bytes_read;
		//printf("The buffer is: \n%s", buffer);
		while(total < bytes_read)
		{
			n = send(newsockfd, buffer+total, bytesleft, 0);
			if (n == -1) 
			{ 
			   syserr("error sending file"); 
			   break;
			}
			//printf("The amount of bytes sent is: %d\n", n);
			total += n;
			bytesleft -= n;
		}
	}
}

void recvandwrite(int tempfd, int newsockfd, int size, char* buffer)
{
	int totalWritten = 0;
	int useSize = 0;
	while(1)
	{
		if(size - totalWritten < BUFFSIZE) 
		{
			useSize = size - totalWritten;
		}
		else
		{
			useSize = BUFFSIZE;
		}
			memset(buffer, 0, BUFFSIZE);
			int total = 0;
			int bytesleft = useSize; //bytes left to recieve
			int n;
			while(total < useSize)
			{
				n = recv(newsockfd, buffer+total, bytesleft, 0);
				if (n == -1) 
				{ 
					syserr("error receiving file"); 
					break;
				}
				total += n;
				bytesleft -= n;
			}
			//printf("The buffer is: \n%s", buffer);
			//printf("Amount of bytes received is for one send: %d\n", total);
		
			int bytes_written = write(tempfd, buffer, useSize);
			//printf("Amount of bytes written to file is: %d\n", bytes_written);
			totalWritten += bytes_written;
			//printf("Total amount of bytes written is: %d\n", totalWritten);
			if (bytes_written == 0 || totalWritten == size) //Done writing into the file
				break;

			if (bytes_written < 0) syserr("error writing file");
		
    }	
}
