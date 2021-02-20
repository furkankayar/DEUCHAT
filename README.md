# DEUCHAT
[![GitHub license](https://img.shields.io/github/license/Naereen/StrapDown.js.svg)](https://github.com/furkankayar/DEUCHAT/blob/master/LICENSE)<br><br>
DEUCHAT is a chat application written in C Language.

server.c handles requests coming from clients. It is multithreaded program.
Compile: gcc -pthread server.c -o server.o

client.c is client program. Sends requests to server.
Compile: gcc -pthread client.c -o client.o

Recommended gcc: 9.2.1

Commands:

<ul>
  <li>-list: Lists the currently available rooms with the name of the customers in it.</li>
  <li>-create room_name: Creates a new specified room. Not more than one room with the same name.</li>
  <li>-pcreate room_name: Creates a new specified private room. This type of room has been protected with password.</li>
  <li>-enter room_name: Enter to the specified room.</li>
  <li>-quit: Quit from the room that you are in. You come back to the common area.</li>
  <li>-msg message_body: Sends a message to room that you are in.</li>
  <li>-whoami: Shows your own nickname information.</li>
  <li>-exit: Exit the program.</li>
</ul>

<img src="/ss.png" width=1482 />
