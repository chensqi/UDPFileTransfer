Hi, this is programming assignment 1 from siqi chen.
In this readme, you will learn how to compiler and make the client/server, and how to run it. For the implement details, I will show you in the [MyTransferProtocol.pdf](https://github.com/chensqi/UDPFileTransfer/blob/master/MyTransferProtocol.pdf)

Make 
--------- 
The Makefile in the top-level folder, just recursively call the Makefile in the _./client_ folder and _./server_ folder. You can use Makefile in the subdirectory if you want to compile them individually like:
```bash
$ cd ./client
$ make
```

Run
--------
Run the client program and server program simultaneously. Client will interact with server when you input one of following command:
```
ls
put [fileName]
get [fileName]
exit
```
Client will list, send or receive file in the server folder for above command.

Run client
--------- 
Run in command line:
```bash
$ cd ./client
$ make
$ ./client
```
program will begin with assumption with server_ip "127.0.0.1" and port "9999". You can run with parameters:
```bash
$ ./client <server_ip> <server_port>
```
For example
```bash
$ ./client 192.168.0.1 9999
```
After you run client, help menu will be shown on the screen.


Run server:
-----------
```bash
$ cd ./server
$ make
$ ./server
```
Program will assume the port is "9999". You can also run with parameters:
```bash
$ ./server <port>
```
For example
```bash
$ ./server 9999
```
After you run server, it will keep waiting the message from client. The only way to stopping is CTRL+C.

