Hi, this is programming assignment 1 from siqi chen.
In this readme, you will learn how to compiler and make the client/server, and how to run it. For the implement details, I will show you in the MyTransferProtocol.pdf


Make:
        The Makefile in this folder, just recursively call the Makefile in the ./client and ./server folder. You can use Makefile in the subdirectory if you want to compile them individually.

Run client:
        $ cd ./client
        $ make
        $ ./client

        If you run like that, program will assume the server_ip is "127.0.0.1" and port is "9999". You can run with parameters:
        $ ./client <server_ip> <server_port>

        For example
        $ ./client 192.168.0.1 9999

        After you run client, help menu will be shown in the screen.


Run server:
        $ cd ./server
        $ make
        $ ./server

        If you run like that, program will assume the port is "9999". You can run with parameters:
        $ ./server <port>

        For example
        $ ./server 9999

        After you run server, it will keep waiting the message from client. The only way to stopping is CTRL+C.

