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

#define MAXBUFSIZE 100
#define ACK "ACK"
#define NAK "NAK"
#define CHECKSUMCONST 23
#define debug


/*
   Calc the checksum
 */
char checksum( char *buf, int len ) { 
        char s = 0;
        int i ; 
        for ( i = 0 ; i < len ; ++i )
                s = s * CHECKSUMCONST + buf[i];
        return s;
}

/*
sets timeout and listen to the socket

returns:
        -1 for time expired
        0 for corruption
        positive values for success
 */
int reliableRecvfrom( int sock, char *buf, struct sockaddr_in *remote) {

        struct timeval timeout;

        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        if( setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0 ) 
                printf("set socket timeout fail\n");    //      set timeout

        int size = sizeof( struct sockaddr_in);
        int length = strlen(buf);
        int r = recvfrom( sock, buf, MAXBUFSIZE, 0, remote, &size);
        if ( r == -1 ) return -1;       //      time limit exceeded

        char s = checksum( buf, r-1 );
        if ( s == buf[r-1] ) {
                buf[r-1] = 0;
                return r-1;     //      the checksum matches original msg, return 
        }
        else {
                printf("%s ",buf);
                printf("%d-%d\n",s,buf[r-1]);
                return 0;       //      corruption, reutnr 0
        }
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;

        if( setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0 ) 
                printf("set socket timeout fail\n");    //      set timeout
}

/*
   encode and append the checksum in the end of buf
   send buf to server
   wait for response, if incorrect, retransmit
 */
int reliableSendto(int sock, char *buf, struct sockaddr_in *remote ) {
        int size = sizeof( struct sockaddr_in ) ;
        int length = strlen( buf );
        char s = checksum(buf, length);
        char send[MAXBUFSIZE] = {0};
        char response[MAXBUFSIZE] = {0};

        strcpy(send, buf);

        send[length] = s ;
        send[length+1] = 0 ;

        int cnt = 0 ;
        while( cnt++ < 10 ) {
                if ( sendto( sock, send, length+1, 0, remote, size ) < 0 ) {
                        printf("Fail to send bcz socket didnt set up correctly\n");
                        return -1;
                }
                // listen to feedback
                // if given correct response, it is a successfull send
                // otherwise, send it again
                int r = reliableRecvfrom(sock, response, remote );

                if ( r == -1 ) continue;        //      time limit exceeded, retransmit
                else if ( r == 0 ) continue;         //      checksum fail, packet corrupt
                else if ( r > 0 ) return 1;        //      success!
                else printf("Unknow response from reliablereceiver!\n");
        }
        return -1;
}

/*
        receive the msg from client, ckeck the checksum and give response to client
        give response "ACK" if checksum is correct
        o.w. "NAK"
        return
                -1 if corruptioin
                otherwise return the length of buf
*/
int receiveAndGiveResponse(int sock, char *buf, struct sockaddr_in *remote) {
        int len = sizeof( struct sockaddr_in );
        int r = recvfrom(sock, buf, MAXBUFSIZE, 0, remote, &len);

        char s = checksum(buf, r-1);

        #ifdef debug
        printf("Server received: ");
        printf("%s with checksum:%c, length(%d)\n",buf,s,r);
        #endif 

        if ( s == buf[r-1] ) {  //      checksum correct, send response
                buf[r-1] = 0 ;
                encodeAndSend(sock, ACK, remote);
                return r-1;
        }
        else {
                buf[0] = 0 ;
                encodeAndSend(sock, NAK, remote);
                return -1;
        }
}

/*
        encode and append the checksum in the end of buf
        send new buf to client
*/
int encodeAndSend(int sock, char *buf, struct sockaddr_in *remote) {
        int size = sizeof( struct sockaddr_in );
        int length = strlen(buf);
        char s = checksum(buf, length );
        char send[MAXBUFSIZE] = {0};

        strcpy(send,buf);
        send[length] = s ;
        send[length+1] = 0 ;
        
        #ifdef debug
        printf("Server is sending.....: ");
        printf("%s\n",send);
        #endif

        return sendto(sock, send, length+1, 0, remote, size) ;
}
/*
        After received msg "ls"
        Send the list of current folder to client
*/
int handleLsCommand(int sock, struct sockaddr_in *remote) {
        char response[MAXBUFSIZE] = {0};
        DIR *dp;
        struct dirent *ep;
        
        dp = opendir("./");
        if ( dp != NULL ) {
                while ( ep = readdir(dp) ) {
                        strcat(response,ep->d_name);
                        strcat(response,"\t");
                }
                closedir(dp);
        }
        else {
                printf("Couldn't open the directory\n");
                // Send empty msg to client since it's waiting for response
        }
        int t = encodeAndSend(sock, response, remote ); 
        if ( t == -1 )
                printf("Response fail\n");
        return t;

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

        receiveAndGiveResponse(sock, fileName , remote); // receive "[filename]"
        FILE *file = fopen(fileName, "w");
        strcpy(endSignal, fileName);
        strcat(endSignal,"!");
        // TODO timer
        
        while ( 1 ) {
                receiveAndGiveResponse(sock, buf, remote);      //      receive file
                if ( strcmp(buf,endSignal) == 0 ) {
                        printf("File received!\n");     //      receive end singal
                        fclose(file);
                        return 1;
                }
                fputs(buf, file);
        }
        fclose(file);

        return 0;
}

/*
        After received "get" command

        1. receive "[filename]"
        2. sending "yes" or "fail", for file exist or not
        3. sending file if "yes"
        4. end sending by "[filename]!"

*/
int handleGetCommand(int sock, struct sockaddr_in *remote) {
        
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
        nbytes = receiveAndGiveResponse(sock, buffer, &remote);

        if ( nbytes < 0 ) {
                printf("incorrect packet received!\n");
                return 1;
        }
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

