#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
/* You will have to modify the program below */
#include <dirent.h>
#include "transferMethod.c"

#define MAXBUFSIZE 100
#define ACK "ACK"
#define NAK "NAK"
#define CHECKSUMCONST 23

/*
        After received msg "ls"
        1. Send the list of current folder to client
        2. Send the end signal "ls!"
        return 1
*/
int handleLsCommand(int sock, struct sockaddr_in *remote) {
        char response[MAXBUFSIZE] = {0};
        DIR *dp;
        struct dirent *ep;
        
        dp = opendir("./");
        if ( dp != NULL ) {
                while ( ep = readdir(dp) ) {
                        strcpy(response,ep->d_name);
                        strcat(response,"\n");
                        int t = reliableSendto(sock, response, strlen(response), remote);
                        #ifdef DEBUG
                                printf("ls command: sending %s",response);
                                printf("%s",t>0?"succ":"fail");
                        #endif
                }
                closedir(dp);
        }
        else {
                printf("Couldn't open the directory\n");
                // Send empty msg to client since it's waiting for response
        }
        reliableSendto(sock, "ls!", 3, remote);
        return 1;
}
/*
        After received "put" command
        Server began to create file and store it
        1. Start with receiving string "[filename]"
        ...
        2. Keep receiveing content
        ...
        3. End with a special string "[filename]!"
*/
int handlePutCommand(int sock, struct sockaddr_in *remote ) {
        char buf[MAXBUFSIZE];
        char endSignal[MAXBUFSIZE];
        char fileName[MAXBUFSIZE];

        reliableRecvfrom(sock, fileName, remote, 5); // receive "[filename]"
        FILE *file = fopen(fileName, "w");
        strcpy(endSignal, fileName);
        strcat(endSignal,"!");
        // TODO timer
        
        int cnt = 0 ;
        while ( 1 ) {
                memset(buf, 0, sizeof buf);
                int r = reliableRecvfrom(sock, buf, remote, 5);      //      receive file
                if ( strcmp(buf,endSignal) == 0 ) {
                        printf("File received!\n");     //      receive end singal
                        fclose(file);
                        return 1;
                }
                if ( r > 0 ) {
                        //fputs(buf, file);
                        fwrite( buf, sizeof(char), r, file);
                        printf("data #%d\n",cnt++);
                        #ifdef DEBUG
                        printf("%s\n",buf);
                        #endif
                }
        }
        fclose(file);
        return 0;
}

/*
   After received "get" command

   1. receive "[filename]"
   2. sending "OK" or "NO", for file exist or not
   3. sending [file] if "OK"
   4. end sending by "[filename]!"

 */
int handleGetCommand(int sock, struct sockaddr_in *remote) {
        char recBuf[MAXBUFSIZE]; 
        char fileName[MAXBUFSIZE];
        FILE *file ;

        if ( reliableRecvfrom(sock, fileName, remote, 5) < 0 ) ; // TODO handle timeout

        file = fopen( fileName, "r");
        if ( file == NULL )
        {
                printf("Open file fail!\n");    //      return 1 to wait for the next command
                reliableSendto(sock, "NO", 2, remote);
                return 1;
        }
        else {
                printf(" ---------------- \t Start sending file %s to client ! \t --------------- \n",fileName);
                reliableSendto(sock, "OK", 2, remote);
        }

        fseek(file, 0, SEEK_END);
        int fileSize = ftell(file);
        rewind(file);

        int chunk = 0 ;
        int cnt = 0 ;
        while ( chunk < fileSize ) {        //     chunk the file and send them 
                int result = fread( recBuf, 1, MAXBUFSIZE-5, file );
                if ( result == 0 )
                        break;
                recBuf[result] = 0;
                if ( reliableSendto(sock, recBuf, result, remote) == -1 ) {
                        printf("Sending fail\n");
                        exit(1);
                }
                chunk += result ;
                printf("Sending data #%d\n",cnt++);
        }
        strcat(fileName,"!");   //      send the end signal "[filename]!"
        reliableSendto(sock, fileName, strlen(fileName), remote );

        printf(" ----------------- \t Sending file %s finished! \t ----------------- \n",fileName);

        return 1;
}

int msgReceiver( int sock ) {        
        /*
                response for the following msg:
                1. "ls"
                2. "put [filename]"
                3. "get [filename]"

                always returns 1 for now
        */

        printf("Server online!\n");

        struct sockaddr_in remote;
        int remote_length;
        char buffer[MAXBUFSIZE];
        char response[MAXBUFSIZE];
        int nbytes;

	remote_length = sizeof(remote);

	//waits for an incoming message
	bzero(buffer,sizeof(buffer));
        bzero(response,sizeof(response)); // initialize the response buffer

        nbytes = reliableRecvfrom(sock, buffer, &remote, 5);    //      waiting with no timeout

	printf("The client says %s\n", buffer);

        // got the message from client
        // begin the prepare the response

        if ( strcmp(buffer, "ls" ) == 0 ) {     //      response for "ls"
                handleLsCommand( sock, &remote );
        }
        else if ( strcmp(buffer, "put") == 0 ) {  //      response for "put" 
                handlePutCommand( sock, &remote );
        }
        else {                                          //      response for "get"
                handleGetCommand( sock, &remote );
        }

        printf("\n\n\n");

        return 1;
}

int main (int argc, char * argv[] )
{
	int sock;                           //This will be our socket
	struct sockaddr_in sin, remote;     //"Internet socket address structure"
	unsigned int remote_length;         //length of the sockaddr_in structure
	int nbytes;                        //number of bytes we receive in our message
	char buffer[MAXBUFSIZE];             //a buffer to store our received message
        int port = 9999;
	if (argc != 2)
	{
		printf ("USAGE:  <port>\n");
                printf ("You are using default setting now, port = %d\n",port);
	}
        else
                port = atoi(argv[1]);
	/******************
	  This code populates the sockaddr_in struct with
	  the information about our socket
	 ******************/
	bzero(&sin,sizeof(sin));                    //zero the struct
	sin.sin_family = AF_INET;                   //address family
	sin.sin_port = htons(port);        //htons() sets the port # to network byte order
	sin.sin_addr.s_addr = INADDR_ANY;           //supplies the IP address of the local machine
	//Causes the system to create a generic socket of type UDP (datagram)
        if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
	{
		printf("unable to create socket");
                exit(1);
	}
	/******************
	  Once we've created a socket, we must bind that socket to the 
	  local address and port we've supplied in the sockaddr_in struct
	 ******************/
	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		printf("unable to bind socket\n");
                exit(1);
	}

        while( msgReceiver(sock) ) ;    // receive the msg from client and response

	close(sock);
}

