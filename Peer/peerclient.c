#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#define BUFFSIZE 256

void syserr(char* msg) { perror(msg); exit(-1); }
void recvandwrite(int tempfd, int sockfd, int size, char* buffer);
void readandsend(int tempfd, int sockfd, char* buffer);

int main(int argc, char* argv[])
{
  //Sockfd: file descriptor; portno: port number; n: return values for read and write
  int sockfd, portno, n, size, tempfd;
  char input[70];
  char *filename;
  char *command;
  struct hostent* server; // server info
  struct sockaddr_in serv_addr; //server address info
  struct stat filestats;
  char buffer[256];
  command = malloc(sizeof(char)*sizeof(buffer));
  filename = malloc(sizeof(char)*sizeof(buffer));

  if(argc != 3) {
    fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
    return 1;
  }
  server = gethostbyname(argv[1]);
  if(!server) {
    fprintf(stderr, "ERROR: no such host: %s\n", argv[1]);
    return 2;
  }
  portno = atoi(argv[2]);

  sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // check man page
  if(sockfd < 0) syserr("can't open socket");
  printf("create socket...\n");

 // set all to zero, then update the sturct with info
  memset(&serv_addr, 0, sizeof(serv_addr)); 
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr = *((struct in_addr*)server->h_addr);
  serv_addr.sin_port = htons(portno); 	

 // connect with filde descriptor, server address and size of addr
  if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    syserr("can't connect to server");
  printf("connect...\n");
  printf("Connection established. Waiting for commands...\n");
  
  for(;;){
  	printf("%s:%d> ", argv[1], portno);
  	fgets(buffer, sizeof(input), stdin);
  	int m = strlen(buffer);
  	if (m>0 && buffer[m-1] == '\n')
  		buffer[m-1] = '\0';
  	
  	strcpy(command, strtok(buffer, " "));
  	//printf("The size of command is: %lu\n", sizeof(command));
  	//printf("The command is: %s\n", command);
  	if(strcmp(command, "ls-local") == 0)
  	{	
  		printf("Files at the Client\n");
		system("ls -a | cat");
		printf("\n");
  	}
  	else if(strcmp(command, "ls-remote") == 0)
  	{
  		memset(buffer, 0, sizeof(buffer));
		strcpy(buffer, "ls");
		//printf("sent in buffer: %s\n", buffer);
		n = send(sockfd, buffer, BUFFSIZE, 0);
		//printf("amount of data sent: %d\n", n);
		if(n < 0) syserr("can't send to server");
		n = recv(sockfd, &size, sizeof(int), 0); 
        if(n < 0) syserr("can't receive from server");
        size = ntohl(size);        
		if(size ==0) // check if file exists
		{
			printf("File not found\n");
			break;
		}
		//printf("The size of the file to recieve is: %d\n", size);
		tempfd = open("remotelist.txt", O_CREAT | O_WRONLY, 0666);
		recvandwrite(tempfd, sockfd, size, buffer);
		printf("%s\n", buffer);
		remove("remotelist.txt");
		close(tempfd);	
  	}
  	else if(strcmp(command, "exit") == 0)
  	{
  		memset(buffer, 0, sizeof(buffer));
		strcpy(buffer, "exit");
		//printf("sending command to server: %s\n", buffer);
		n = send(sockfd, buffer, strlen(buffer), 0);
		//printf("amount of data sent: %d\n", n);
		if(n < 0) syserr("can't send to server");
		n = recv(sockfd, &size, sizeof(int), 0);
        size = ntohl(size);  
        if(n < 0) syserr("can't receive from server");
        
		//printf("size was %d from server\n", size);
		if(size)
		{
			printf("Connection to server terminated\n");
			exit(0);
		}
		else
		{
			printf("Server didn't exit");
		}
		
  	} 	
  	else if(strcmp(command, "get") == 0)
  	{
  		strcpy(filename, strtok(NULL, " "));
  		//printf("The filename is: %s\n", filename);
  		memset(buffer, 0, sizeof(buffer));
  		strcpy(buffer, "get ");
  		strcat(buffer, filename);
		//printf("sent in buffer: %s\n", buffer);
		n = send(sockfd, buffer, BUFFSIZE, 0);
		//printf("amount of data sent: %d\n", n);
		if(n < 0) syserr("can't send to server");
		n = recv(sockfd, &size, sizeof(int), 0); 
        if(n < 0) syserr("can't receive from server");
        size = ntohl(size);        
		if(size ==0) // check if file exists
		{
			printf("File not found\n");
			break;
		}
		//printf("The size of the file to recieve is: %d\n", size);
		tempfd = open(filename, O_CREAT | O_WRONLY, 0666);
		if(tempfd < 0) syserr("failed to get file");
		recvandwrite(tempfd, sockfd, size, buffer);
		printf("Download of '%s' was successful\n", filename);  
		close(tempfd);
		
  	}
  	else if(strcmp(command, "put") == 0)
  	{
  		//Send command & filename
  		strcpy(filename, strtok(NULL, " "));
  		memset(buffer, 0, BUFFSIZE);
  		strcpy(buffer, "put ");
  		strcat(buffer, filename);
  		//printf("The filename to send to server is: %s\n", filename);
		n = send(sockfd, buffer, BUFFSIZE, 0);
		//printf("amount of data sent: %d\n", n);
		if(n < 0) syserr("can't send to server");
		//printf("sent in buffer: %s\n", buffer);
		//Send file size
		stat(filename, &filestats);
		size = filestats.st_size;
		//printf("Size of file to send: %d\n", size);
        size = htonl(size);      
		n = send(sockfd, &size, sizeof(int), 0);
		if(n < 0) syserr("couldn't send size to server");
		//Send file
		tempfd = open(filename, O_RDONLY);
		if(tempfd < 0) syserr("failed to get file, server side");
		readandsend(tempfd, sockfd, buffer);
		printf("Upload of '%s' was successful\n", filename); 
		close(tempfd);	
  	}
  	else
  	{
  		printf("Correct commmands are: 'ls-local', 'ls-remote', 'get <filename>' and 				'put <filename>, 'exit''\n");
  	}  	
  }
  close(sockfd);
  return 0;
}

void recvandwrite(int tempfd, int sockfd, int size, char* buffer)
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
				n = recv(sockfd, buffer+total, bytesleft, 0);
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

void readandsend(int tempfd, int sockfd, char* buffer)
{
	int totalSent = 0;
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
			n = send(sockfd, buffer+total, bytesleft, 0);
			if (n == -1) 
			{ 
			   syserr("error sending file"); 
			   break;
			}
			//printf("The amount of bytes sent is: %d\n", n);
			total += n;
			bytesleft -= n;
		}
		totalSent += total;
		//printf("The total bytes sent is: %d\n", totalSent);
	}
}
