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
#include "transferMethod.c"

#define MAXBUFSIZE 100
#define CHECKSUMCONST 23
//#define DEBUG

/*
        sends "ls" and waits for response
        keep receiveing the file lists
        end with "ls!"
        always returns 1
*/
int handleLsCommand(int sock, struct sockaddr_in *remote ) { // user type command "ls"
        char recBuf[MAXBUFSIZE];
        int size = sizeof(struct sockaddr_in);
        // Sends "ls" to server
        int t ;
        if ( t = reliableSendto(sock, "ls", 2, remote ) != 1 ) {
                #ifdef DEBUG
                printf("send \"ls\" with return value %d",t);
                #endif
                return 1;
        }

        printf(" --------------- \t Begin of server's response \t ------------- \n");
        while ( 1 ) {
                int recBytes = reliableRecvfrom(sock, recBuf, remote, 5 );
                
                if ( strcmp( recBuf, "ls!") == 0 )
                        break;
                if ( recBytes > 0 ) {
                        printf("%s",recBuf);
                }
        }
        printf(" --------------- \t End of server's response \t ------------- \n");
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

        if ( reliableSendto(sock, "put", 3, remote ) == -1 ) { //      send "put"
                printf("Sending fail\n");
                exit(1);
        }

        reliableSendto(sock, fileName, strlen(fileName), remote); //   send with "[filename]"
        int chunk = 0 ;
        int cnt = 0 ;
        while ( chunk < fileSize ) {        //     chunk the file and send them 
                printf("Sending data #%d\n",cnt++);
                memset( recBuf, 0, sizeof recBuf);

                int result = fread( recBuf, sizeof(char), MAXBUFSIZE-5, file ); 
                #ifdef DEBUG
                printf("%s with length %d\n",recBuf,result);
                #endif
                if ( result == 0 )
                        break;
                if ( reliableSendto(sock, recBuf, result, remote) == -1 ) {
                        printf("Sending fail\n");
                        exit(1);
                }
                chunk += result ;
        }
        strcat(fileName,"!");   //      send the end signal "[filename]!"
        reliableSendto(sock, fileName, strlen(fileName),remote );

        printf(" ----------------- \t Sending file %s finished! \t ----------------- \n",fileName);

        return 1; 
}

/*
        1. send "get" to server, create local file
        2. send "[fileName]" to server
        3. receive msg "OK" indicates file exist,  or "NO" not exist
        3. keep receiving strings from server
        4. end receiving by "[fileName]!"
*/

int handleGetCommand(int sock, struct sockaddr_in *remote, char *fileName) {

        if ( reliableSendto(sock, "get", 3, remote ) == -1 ) { //      send "get"
                printf("Sending fail\n");
                exit(1);
        }
        reliableSendto(sock, fileName, strlen(fileName), remote); //      send "[fileName]"

        char buf[MAXBUFSIZE];
        char endSignal[MAXBUFSIZE];
        FILE *file = fopen(fileName, "w");
        strcpy(endSignal, fileName);
        strcat(endSignal,"!");
        // TODO timer


        printf(" --------------- \t Begin of server's response \t ------------- \n");

        reliableRecvfrom(sock, buf, remote, 5); //      TODO handle the timeout
        if ( strcmp(buf, "NO") == 0 ) {
                printf("File does not exists!\n");
                return 1;
        }

        int cnt = 0 ;
        while ( 1 ) { 
                memset(buf,0,sizeof(buf));
                int r = reliableRecvfrom(sock, buf, remote, 5);
                if ( strcmp(buf,endSignal) == 0 ) { 
                        printf("File received!\n");     //      receive end singal
                        fclose(file);
                        return 1;
                }   
                if ( r > 0 )   {//       not timeout or duplicated packet
                        fwrite(buf, sizeof(char), r, file);
                        #ifdef DEBUG
                        printf("data #%d\n",cnt++);
                        printf("%s\n",buf);
                        #endif
                }
        }   
        fclose(file);

        printf(" --------------- \t End of server's response \t ------------- \n");

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
                return handleGetCommand(sock, remote, fileName);
        }
        else {
                printf("Usage:\n");
                printf("1.\texit\n");
                printf("2.\tls\n");
                printf("3.\tget [file_name]\n");
                printf("4.\tput [file_name]\n");
                printf("Anything else - help menu\n");

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
                printf("You are using default setting, ip = %s, port = %d\n",add,port);
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
        printf("Client online!\n");

                printf("Usage:\n");
                printf("1.\texit\n");
                printf("2.\tls\n");
                printf("3.\tget [file_name]\n");
                printf("4.\tput [file_name]\n");
                printf("Anything else - help menu\n");
                printf("\n\n\n");
 
        
        // Function command() will read the user's command and send to server
        // and return 0 when user command "quit" or "exit"
        while( command(sock, &remote) ) {
                printf("\n\n\n");
        }

	close(sock);
}

