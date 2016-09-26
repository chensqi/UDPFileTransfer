#ifndef __TRANSFERMETHOD__
#define __TRANSFERMETHOD__

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
//#define DEBUG

int sendACK = 0;
int recvACK = 0;

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
   recv a msg and check the last but 2 bit(checksum)
   return @lenth_buf if correct
   return 0 if incorrect
   return -1 if timeout or other error 
 */
int recvAndCheck(int sock, char *buf, struct sockaddr_in *remote, int timeOut ) {
        int size = sizeof( struct sockaddr_in );
        memset( buf, 0, MAXBUFSIZE );
        int r = recvfrom(sock, buf, MAXBUFSIZE, 0, remote, &size);

        struct timeval timeout;

        timeout.tv_sec = timeOut;
        timeout.tv_usec = 0;
        if( setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0 )
                printf("set socket timeout fail\n");    //      set timeout

        if ( r == -1 ) {        // timeout or other error
#ifdef DEBUG
                printf("recvfrom returns a negative value\n");
#endif
                return -1;
        }
        char s = checksum(buf, r-2);
        if ( s == buf[r-2] ) {  //      correct
#ifdef DEBUG
        printf("%s with length %d, checksum %c, buf[l-2] is %c\n",buf,r,s,buf[r-2]);
#endif
                buf[r-2] = 0;
                return r-2;
        }
        else {                  //      incorrect
                buf[0] = 0 ;
                return 0;
        }
}
/*
   append checksum to @buf, append ACKNUM to the @buf, and send it to remote port
   return -1 if error
   return 1 if sended
 */
int sendWithChecksum( int sock, char *buf, int l, struct sockaddr_in *remote, int ackNum ) {
        int size = sizeof( struct sockaddr_in ) ;
        char s = checksum(buf, l );
        char msg[MAXBUFSIZE] ;

        memset(msg, 0, MAXBUFSIZE);

        int i ;
        for ( i = 0 ; i < l ; ++i )
                msg[i] = buf[i] ;
        l += 1;
        msg[l-1] = s;   //      append checksum
        l += 1 ;
        msg[l-1] = (char)ackNum ;       //      append ackNUM
        msg[l] = 0;

        int result = sendto(sock, msg, l, 0, remote, size) ;
        if ( result < 0 ) {     //      somthing wrong
#ifdef DEBUG
                printf("sendto returns a negative value\n");
                printf("sending message: %s",buf);
#endif
                return -1;
        }
        else return 1;
}

/*
   1. waiting for message 
   2. if msg correct, give "ACK" response and return
   3. if not correct, give "NAK" response and waiting for message again

returns:
-1 for time expired
-2 if it is same as the last packet, r.g. duplicate
positive values for correct packet
 */
int reliableRecvfrom( int sock, char *buf, struct sockaddr_in *remote, int timeOut) {

        int res; // return value
        while ( 1 ) { // TODO set time expired and return -1
                int r = recvAndCheck( sock, buf, remote, timeOut ); 
                #ifdef DEBUG
                printf("recv: %s with length %d\n",buf,r);
                #endif
                if ( r > 0 ) {
                        sendWithChecksum(sock, "ACK", 3, remote, sendACK++);
                        #ifdef DEBUG
                        printf("responing... ACK\n");
                        #endif
                        if ( buf[r+1] == recvACK ) {     //      the same as last received ACK number
                                res = -2 ;
                                break;
                        }
                        else {
                                recvACK = buf[r+1];     //      save the last received ACM number
                                res = r ;
                                break;
                        }
                }
                else if ( r == 0 ) {    //      "NAK", waiting again
                        sendWithChecksum(sock, "NAK", 3, remote, sendACK++ );
                        #ifdef DEBUG
                        printf("responing... NAK\n");
                        #endif
                        
                }
                else if ( r == -1 ) {   //      timeout, waiting again
                        #ifdef DEBUG
                        printf("timeout, waiting again\n");
                        #endif
                
                }
        }
        return res ;
}
/*
   1. sending message
   2. if received "ACK", return 1
   3. if received "NAK" or waiting timeout, sending again
 */
int reliableSendto(int sock, char *buf, int length, struct sockaddr_in *remote) {
        int size = sizeof( struct sockaddr_in ) ;
        char response[MAXBUFSIZE] = {0};

        while ( 1 ) { // TODO set time expired
                int r = sendWithChecksum( sock, buf, length, remote, sendACK++ );
                #ifdef DEBUG
                printf("Sending %s\n",buf);
                #endif
                if ( r == -1 ) {
                        //      error when sending
                        // TODO
                        printf("unknow error!\n");
                }
                r = recvAndCheck( sock, response, remote, 5);
                #ifdef DEBUG
                printf("with recv value %d and %s\n",r, response);
                #endif
                if ( r > 0 ) {  //      check it is "ACK" or "NAK"
                        if ( strcmp(response, "ACK") == 0 ) {
                                return 1;
                        }
                        else if ( strcmp(response, "NAK") == 0 ) {
                                sendACK -- ;
                                continue;
                        }
                }
                else if ( r == 0 ) {    //      corrupt "ACK" or "NAK", resend
                        sendACK -- ;
                        continue;    
                }
                else if ( r == -1 ) {   //      timeout, resend 
                        sendACK -- ;
                        continue;
                }
                else if ( r == -2 ) {   //      error
                        printf("Received a -2 from \'ACK\' or \'NAK\', IMPOSSIBLE\n");
                        exit(1);
                }
        } 
}
#endif
