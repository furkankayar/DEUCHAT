# DEUCHAT
DEUCHAT is a chat application written in C Language.

server.c handles requests coming from clients. It is multithreaded program.
Compile: gcc -pthread server.c -o server.o

client.c is client program. Sends requests to server.
Compile: gcc -pthread client.c -o client.o

Recommended gcc: 9.2.1


<img src="/ss.png" width=1482 />
