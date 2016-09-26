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
#include <errno.h>
#include <string.h>

#define MAXBUFSIZE 100
#define CHECKSUMCONST 23
#define DEBUG

/*
        Calc the checksum
*/
char checksum( char *buf, int len ) {
        char s = 0 ;
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
   1. receive the msg from Server, ckeck the checksum and give response to Server 
   2. give response "ACK" if checksum is correct. o.w. "NAK"
   return
   -1 if corruptioin
   otherwise return the length of buf
 */
int receiveAndGiveResponse(int sock, char *buf, struct sockaddr_in *remote) {
        int len = sizeof( struct sockaddr_in );
        int r = recvfrom(sock, buf, MAXBUFSIZE, 0, remote, &len);

        char s = checksum(buf, r-1);

#ifdef debug
        printf("Client received: ");
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
   encode and append the checksum in the end of @buf
   send new @buf to server
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
        printf("Client is sending.....: ");
        printf("%s\n",send);
#endif

        return sendto(sock, send, length+1, 0, remote, size) ;
}


/*
        sends "ls" and waits for response
        always returns 1
*/
int handleLsCommand(int sock, struct sockaddr_in *remote ) { // user type command "ls"
        char recBuf[MAXBUFSIZE];
        int size = sizeof(struct sockaddr_in);
        // Sends "ls" to server
        if ( reliableSendto(sock, "ls", remote ) == -1 )
                return 1;
        // Prints the list of file's name
        int recBytes = reliableRecvfrom(sock, recBuf, remote);
        if ( recBytes > 0 ) {
                printf(" --------------- \t Begin of server's response \t ------------- \n");
                printf("%s\n",recBuf);
                printf(" --------------- \t End of server's response \t ------------- \n");
        }
        else if ( recBytes == 0 ) {
                printf(" --------------- \t packet is corrupt \t ----------\n");
        }
        else if ( recBytes == -1 ) {
                printf(" --------------- \t listener timeout \t ------------- \n");
        }
        return 1;
}

/*
        put a file onto server
        1. send "put" msg to server
        2. begin with "[filename]" 
        3. send file by chunk
        4. end with "[filename]!"

*/
int handlePutCommand(int sock, struct sockaddr_in *remote, char *fileName ) {
        int size = sizeof(struct sockaddr_in);
        char recBuf[MAXBUFSIZE];
        FILE *file ;
        
        file = fopen( fileName, "r");
        if ( file == NULL )
        {
                printf("Open file fail!\n");    //      return 1 to wait for the next command
                return 1;
        }
        else {
                printf(" ---------------- \t Start sending file %s to server! \t --------------- \n",fileName);
        }

        fseek(file, 0, SEEK_END);
        int fileSize = ftell(file);
        rewind(file);

        if ( reliableSendto(sock, "put", remote ) == -1 ) { //      send "put"
                printf("Sending fail\n");
                exit(1);
        }

        reliableSendto(sock, fileName, remote); //   send with "[filename]"
        int chunk = 0 ;
        while ( chunk < fileSize ) {        //     chunk the file and send them 
                int result = fread( recBuf, 1, MAXBUFSIZE-5, file ); 
                if ( result == 0 )
                        break;
                recBuf[result] = 0;
                if ( reliableSendto(sock, recBuf, remote) == -1 ) {
                        printf("Sending fail\n");
                        exit(1);
                }
                chunk += result ;
        }
        strcat(fileName,"!");   //      send the end signal "[filename]!"
        reliableSendto(sock, fileName, remote );

        printf(" ----------------- \t Sending file %s finished! \t ----------------- \n",fileName);

        return 1; 
}

/*
        1. send "get" to server, create local file
        2. send "[fileName]" to server
        3. receive msg indicates file exist or not exist
        3. keep receiving strings from server
        4. end receiving by "[fileName]!"
*/
int handleGetCommand(int sock, struct sockaddr_in *remote, char *fileName) {

        if ( reliableSendto(sock, "get", remote ) == -1 ) { //      send "get"
                printf("Sending fail\n");
                exit(1);
        }
        reliableSendto(sock, fileName, remote); //      send "[fileName]"

        char buf[MAXBUFSIZE];
        char endSignal[MAXBUFSIZE];
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

        return 1;

}


/*
reads user's command and forward to server
return: 1 if everything is well
*/
int command(int sock, struct sockaddr_in *remote) {     //      read user's command and speak to server
        char cmd[MAXBUFSIZE];
        char fileName[MAXBUFSIZE];
        scanf("%s",cmd);

        if ( strcmp(cmd,"quit") == 0 || strcmp(cmd,"exit") == 0 ) {
                printf("Bye!\n");
                return 0;
        }
        else if ( strcmp(cmd,"ls") == 0 ) {
                return handleLsCommand(sock, remote);
        }
        else if ( strcmp(cmd,"put") == 0 ) {
                scanf("%s",fileName);
                printf("putting %s\n",fileName);

                return handlePutCommand(sock, remote, fileName);
        }
        else if ( strcmp(cmd,"get") == 0 ) {
                scanf("%s",fileName);
                printf("getting %s\n",fileName);

                return handleGetCommand(sock, remote, fileName);
        }
        else {
                printf("Usage:\n");
                printf("1.\texit\n");
                printf("2.\tls\n");
                printf("3.\tget [file_name]\n");
                printf("4.\tput [file_name]\n");
                return 1;
        }
}

int main (int argc, char * argv[])
{
	int sock;                               //this will be our socket
        int port = 9999;
        char add[20] = "127.0.0.1";

	struct sockaddr_in remote;              //"Internet socket address structure"

	if (argc < 3)
	{
		printf("USAGE:  <server_ip> <server_port>\n");
	}
        else
        {
                port = atoi(argv[2]);
                strcpy(add, argv[1]);
        }

	/******************
	  Here we populate a sockaddr_in struct with
	  information regarding where we'd like to send our packet 
	  i.e the Server.
	 ******************/
	bzero(&remote,sizeof(remote));               //zero the struct
	remote.sin_family = AF_INET;                 //address family
	remote.sin_port = htons(port);//sets port to network byte order
	remote.sin_addr.s_addr = inet_addr(add); //sets remote IP address

	//Causes the system to create a generic socket of type UDP (datagram)
        if ( (sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1 )
	{
		printf("unable to create socket\n");
                exit(1);
	}
        if (inet_aton(add,&remote.sin_addr)==0) {
                printf("inet_aton() error\n");
                exit(1);
        }
        printf("Socket created\n");

        //sendto(sock, "hello", 5, 0, &remote, sizeof(remote));
        
        // Function command() will read the user's command and send to server
        // and return 0 when user command "quit" or "exit"
        while( command(sock, &remote) ) ;

	close(sock);
}

