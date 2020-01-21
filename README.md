# DEUCHAT
DEUCHAT is a chat application written in C Language.

server.c handles requests coming from clients. It is multithreaded program.
Compile: gcc -pthread server.c -o server.o

client.c is client program. Sends requests to server.
Compile: gcc -pthread client.c -o client.o

Recommended gcc: 9.2.1

Commands:

● -list: Lists the currently available rooms with the name of the customers in it.
● -create room_name: Creates a new specified room. Not more than one room with the same name.
● -pcreate room_name: Creates a new specified private room. This type of room has been protected with password.
● -enter room_name: Enter to the specified room.
● -quit: Quit from the room that you are in. You come back to the common area.
● -msg message_body: Sends a message to room that you are in.
● -whoami: Shows your own nickname information.
● -exit: Exit the program.


<img src="/ss.png" width=1482 />
