/*
    DEUCHAT SERVER
    Written by Furkan Kayar

    -ASSUMPTIONS
        Total number of clients (alive + disconnected) can be 100 at maximum.
        Total number of rooms (active + inactive) can be 100 at maximum.
        Total number of clients entered into a room can be 100 at maximum.
        Total number of clients in a room at the same time can be 30 at maximum.
        If the last client in a room quits, room is closed.
        When clients create a room, they enter into room automatically.
        Clients can send message wihout using "-msg" command if they are in a room.
        Clients cannot create room, list rooms, enter room, if they are in a room.
        Clients cannot quit from room, send message, if they are not in a room.
        User nicknames are not unique.
        Room names are unique.
        Server listens on 3205 port. So, port 3205 has to be free on the system.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define SOCKET_CREATE_ERR   1
#define BINDING_ERR         2
#define ACCEPT_ERR          3
#define THREAD_CREATE_ERR   4
#define CONNECTION_ERR      5
#define SEND_ERR            6
#define RECV_ERR            7
#define PORT                3205
#define MAX_CLIENT_NUMBER   100
#define MAX_ROOM_NUMBER     100
#define ROOM_TYPE_PRIVATE   1
#define ROOM_TYPE_PUBLIC    0
#define ROOM_CAPACITY       30
#define ROOM_ACTIVE         0
#define ROOM_INACTIVE       1
#define LOCATION_LOBBY      0
#define LOCATION_ROOM       1
#define ALIVE               0
#define DISCONNECTED        1




typedef struct client{ // Information about a client is stored in struct.

    int id;
    int socket;
    char* nickname;
    int location;
    int room_id;
    int connection_flag;

} client;

typedef struct chat_room{ // Information about chat room is stored in struct.

    char* name;
    int type;
    char* password;
    int client_ids[100];
    int client_counter;
    int active_client_counter;
    int is_active;

} chat_room;


void* connection_handler(void*);
void init_room_client(void);
char** split(char*, char);
char* trim(char*);
int check_room_name_valid(char*);
void write_client(int, char*);
void console_log(char*, int, int, char*, char*);
int get_room_id_by_name(char*);
int check_socket_status(client*);
int is_room_contain_client(int, int);
int validate_password(char*, char*);


client clients[MAX_CLIENT_NUMBER];
chat_room rooms[MAX_ROOM_NUMBER];

int total_client_number = 0;
int total_room_number = 0;
char reserved_room_names[100][100] = {{'\0'}}; // Reserved room names is used in pcreate command.
char reserved_room_name_counter = 0;
sem_t mutex; // All client threads requires common data. Mutex is required to synchronize threads.

int main(){

    sem_init(&mutex, 0, 1);
    int socket_desc, new_socket, c, *new_sock;
    struct sockaddr_in server, client;

    // Create Socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    if(socket_desc == -1){

        puts("Coult not create socket!");
        return SOCKET_CREATE_ERR;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY; // IPv4 local host addr
    server.sin_port = htons(PORT);

    if(bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0){
        puts("Binding failed");
        return BINDING_ERR;
    }
    puts("Socket is binded");

    listen(socket_desc, 3); // Server is started to listen connections on 3205 port.
    puts("Waiting for incoming connections");

    c = sizeof(struct sockaddr_in);

    while((new_socket = accept(socket_desc,(struct sockaddr *)&client,(socklen_t*)&c))){ // This loop runs forever for new clients.

        puts("New connection");
        write_client(new_socket, "Welcome to the DEUCHAT\n");

        pthread_t client_thread;
        clients[total_client_number].id = total_client_number; // Giving thread an identity.
        clients[total_client_number++].socket = new_socket; // Socket number is used to send message to the client.

        if(pthread_create(&client_thread, NULL, connection_handler, (void*)&clients[total_client_number - 1]) < 0){ // Thread is assigned for the client.
            puts("Could not create thread");
            return THREAD_CREATE_ERR;
        }

        puts("Handler assigned\n");
    }

    close(socket_desc);
    close(new_socket);

    return 0;
}


/*
    This function is used by client threads.
    Communicates with client until client is disconnected.
*/
void *connection_handler(void* client_ptr){

    client* cl = ((client*)client_ptr);
    int sock = cl->socket;
    char client_message[100] = {'\0'};
    int bytes_read = 0;

    write_client(sock, "Enter your nickname: ");
    bytes_read = recv(sock, client_message, 2000, 0);
    client_message[bytes_read] = '\0';
    cl->nickname = (char*)malloc(sizeof(char) * bytes_read);
    strcpy(cl->nickname, client_message); // A nickname is assigned to client.
    cl->location = LOCATION_LOBBY;
    cl->room_id = -1; // Client is not in a room yet.
    char send[100];
    sprintf(send, "login_success;%d;%s", cl->id, cl->nickname);
    write_client(sock, send); // Informs client, client is in lobby now and server is ready to execute commands coming from client.


    while((bytes_read = recv(sock, client_message, 2000, 0)) > 0){ // Listens client commands

        client_message[bytes_read] = '\0';
        char** splitted = split(client_message, ' ');
        if(strcmp(splitted[0], "-list") == 0){
            if(cl->location == LOCATION_LOBBY){ // Client can list rooms, only if he/she in lobby
                int i = 0;
                char message[500] = {'\0'};
                strcat(message, "list;");
                sem_wait(&mutex); // Entering critical region.
                for(i = 0 ; i < total_room_number ; i++) {
                    if(rooms[i].is_active == ROOM_ACTIVE){ // Lists only active rooms, rooms turns to inactive forever when they are empty.
                        int t = 0;
                        char tmp[100];
                        sprintf(tmp, "\n Room Name: %s\n Room Type: %s\n", rooms[i].name, rooms[i].type == ROOM_TYPE_PRIVATE ? "Private" : "Public");
                        strcat(message, tmp);
                        if(rooms[i].type == ROOM_TYPE_PUBLIC){
                            strcat(message, " Customers: \n");
                            for (t = 0 ; t < rooms[i].client_counter ; t++){
                                if(check_socket_status(&clients[rooms[i].client_ids[t]]) || clients[rooms[i].client_ids[t]].room_id != i)
                                    continue;
                                sprintf(tmp, "\t%s\n", clients[rooms[i].client_ids[t]].nickname);
                                strcat(message, tmp);
                            }
                        }
                        else {
                            strcat(message, " No customer info given, room is private!\n");
                        }
                    }
                }
                sem_post(&mutex); // Exiting critical region.
                write_client(sock, message); // Sending room list to client.
            }
            else { // Client is not in lobby, so he/she can not list rooms.
                console_log(cl->nickname, cl->id, cl->socket, "Attempted to list rooms", "Rejected because of user is not in lobby");
                write_client(sock, "You have to be in lobby to list rooms!");
            }
        }
        else if(strcmp(splitted[0], "-create") == 0){
            if(cl->location == LOCATION_LOBBY){ // Client can create room, only if he/she is in lobby.
                int room_id = -1;
                int room_name_valid = 0;
                sem_wait(&mutex); // Entering critical region
                room_name_valid = check_room_name_valid(splitted[1]); // Checking room name's uniqueness.
                if(strcmp(splitted[1], "") == 0){ // Empty room name is not acceptable.
                    write_client(sock, "This room name is not valid!");
                    char result[100];
                    console_log(cl->nickname, cl->id, cl->socket, "Attempted to create a room", "Rejected because of user name is not valid");
                    sem_post(&mutex); // Exiting critical region because this turn of loop will not executed.
                    continue;
                }
                else if(!room_name_valid){ // Room name must be valid.
                    write_client(sock, "This room name is already in use!");
                    char result[100];
                    sprintf(result, "Rejected due to unique name constraint: %s", splitted[1]);
                    console_log(cl->nickname, cl->id, cl->socket, "Attempted to create a room", result);
                    sem_post(&mutex); // Exiting critical region because this turn of loop will not executed.
                    continue;
                }
                room_id = total_room_number; // Room id is assigned. Room id's are also unique.
                rooms[room_id].name = (char*)malloc(sizeof(char) * strlen(splitted[1]));
                strcpy(rooms[room_id].name, splitted[1]);
                rooms[room_id].type = ROOM_TYPE_PUBLIC;
                rooms[room_id].is_active = ROOM_ACTIVE;
                rooms[room_id].client_ids[rooms[room_id].client_counter++] = cl->id; // The client that creates room is added into room.
                rooms[room_id].active_client_counter = 1; // Counting client number in room.
                total_room_number += 1; // Counting total room number in system. (Active + inactive)
                cl->location = LOCATION_ROOM; // Updating client location
                cl->room_id = room_id; // Updating client's room.
                char message[100];
                sprintf(message, "room_created;%s;%d;%d", rooms[room_id].name, rooms[room_id].client_counter, ROOM_CAPACITY);
                write_client(sock, message); // Informing client
                sem_post(&mutex);
                char result[100];
                sprintf(result, "Successful, room \"%s\" has been created", rooms[room_id].name);
                console_log(cl->nickname, cl->id, cl->socket, "Attempted to create a room", result);

            }
            else{ // Client is not in lobby, so he/she cannot create room.
                console_log(cl->nickname, cl->id, cl->socket, "Attempted to create a room", "Rejected because of user is not in lobby");
                write_client(sock, "You have to be in lobby to create room!");
            }

        }
        else if(strcmp(splitted[0], "-pcreate") == 0){
            if(cl->location == LOCATION_LOBBY){ // Client can create private room, only if he/she is in lobby.
                int room_id = -1;
                int room_name_valid = 0;
                sem_wait(&mutex);
                room_name_valid = check_room_name_valid(splitted[1]); // Checking room name's uniqueness.
                if(strcmp(splitted[1], "") == 0){ // Empty room name is not acceptable.
                    write_client(sock, "This room name is not valid!");
                    char result[100];
                    console_log(cl->nickname, cl->id, cl->socket, "Attempted to create a room", "Rejected because of user name is not valid");
                    sem_post(&mutex); // Exiting critical region because this turn of loop will not executed.
                    continue;
                }
                else if(!room_name_valid){ // Room name must be valid.
                    write_client(sock, "This room name is already in use!");
                    char result[100];
                    sprintf(result, "Rejected due to unique name constraint: %s", splitted[1]);
                    console_log(cl->nickname, cl->id, cl->socket, "Attempted to create a room", result);
                    sem_post(&mutex); // Exiting critical region because this turn of loop will not executed.
                    continue;
                }
                int reserved_index = reserved_room_name_counter;
                strcpy(reserved_room_names[reserved_room_name_counter++], splitted[1]);
                /*
                    Room name is reserved until the client chooses a valid password for room.
                    This operation can take much time because of client.
                    So, we need to post mutex for another clients to continue their operations.
                    But another clients should not create room with same name. Therefore, room name is reserved.
                    reserved_room_names array is also used when checking uniqueness of room names.
                */
                sem_post(&mutex); //Exiting from critical region.

                // Getting valid password from client for private room.
                int password_valid = 0;
                char* password = (char*)malloc(sizeof(char) * 200);
                while(!password_valid){
                    write_client(sock, "set_password;Set a password for private room.");
                    int bytes = 0;
                    while((bytes = recv(sock, password, 200, 0)) > 0){
                        char result_buffer[100] = {'\0'};
                        password = trim(password);
                        password[bytes] = '\0';
                        if(validate_password(password, result_buffer)){
                            write_client(sock, result_buffer);
                            password_valid = 1;
                            sleep(1);
                            break;
                        }
                        else{
                            write_client(sock, result_buffer);
                            password_valid = 0;
                        }
                    }
                }

                // Password has been chosen.
                sem_wait(&mutex); // Enter critical region again because room will be created.
                strcpy(reserved_room_names[reserved_index], "\0");
                room_id = total_room_number; // Room id is assigned. Room id's are also unique.
                rooms[room_id].name = (char*)malloc(sizeof(char) * strlen(splitted[1]));
                rooms[room_id].password = (char*)malloc(sizeof(char) * strlen(password));
                strcpy(rooms[room_id].name, splitted[1]);
                strcpy(rooms[room_id].password, password);
                rooms[room_id].type = ROOM_TYPE_PRIVATE;
                rooms[room_id].is_active = ROOM_ACTIVE;
                rooms[room_id].client_ids[rooms[room_id].client_counter++] = cl->id; // The client that creates room is added into room.
                rooms[room_id].active_client_counter = 1; // Counting client number in room.
                total_room_number += 1; // Counting total room number in system. (Active + inactive)
                cl->location = LOCATION_ROOM; // Updating client location.
                cl->room_id = room_id; // Updating client's room.
                char message[100] = {'\0'};
                sprintf(message, "room_created;%s;%d;%d\0", rooms[room_id].name, rooms[room_id].client_counter, ROOM_CAPACITY);
                write_client(sock, message); // Informing client.
                sem_post(&mutex); // Exiting critical region.
                char result[100] = {'\0'};
                sprintf(result, "Successful, room \"%s\" has been created\0", rooms[room_id].name);
                console_log(cl->nickname, cl->id, cl->socket, "Attempted to create a room\0", result);

            }
            else{ // Client is not in lobby.
                console_log(cl->nickname, cl->id, cl->socket, "Attempted to create a room\0", "Rejected because of user is not in lobby\0");
                write_client(sock, "You have to be in lobby to create room!\0");
            }
        }
        else if(strcmp(splitted[0], "-enter") == 0){
            if(cl->location == LOCATION_LOBBY){ // Client can enter into room, only if he/she is in lobby.
                sem_wait(&mutex); // Entering critical region
                int room_id = get_room_id_by_name(splitted[1]);
                if(room_id == -1){ // There is no room that has given name in system.
                    write_client(sock, "Room could not found!");
                    console_log(cl->nickname, cl->id, cl->socket, "Attempted to enter a room", "Rejected because of room does not exists");
                    sem_post(&mutex); // Exiting critical region, client will not enter room.
                    continue;
                }
                if(rooms[room_id].client_counter == ROOM_CAPACITY){ // Room is full.
                    write_client(sock, "Room is full capacity!");
                    console_log(cl->nickname, cl->id, cl->socket, "Attempted to enter a room", "Rejected because of room is full capacity");
                    sem_post(&mutex); // Exiting critical region, client will not enter room.
                    continue;
                }
                if(rooms[room_id].type == ROOM_TYPE_PRIVATE){ // Room is private, client has to enter correct password.
                    char password[200] = {'\0'};
                    write_client(sock, "request_password;Enter password\0");
                    int bytes = 0;
                    if((bytes = recv(sock, password, 200, 0)) > 0){
                        password[bytes] = '\0';
                        printf("%s %s\n", password, rooms[room_id].password);
                        if(strcmp(password, rooms[room_id].password) != 0){
                            write_client(sock, "incorrect_password;Password is not accepted!\0");
                            sem_post(&mutex); // Exiting critical region, password is not true
                            continue;
                        }
                    }
                }
                // Password is true, client is entering into room.
                cl->location = LOCATION_ROOM;
                if(!is_room_contain_client(room_id, cl->id)){
                    rooms[room_id].client_ids[rooms[room_id].client_counter++] = cl->id; // Client is added to room.
                }
                rooms[room_id].active_client_counter += 1; // Updating client counter of room.
                cl->room_id = room_id;
                char message[100] = {'\0'};
                sprintf(message, "room_entered;%s;%d;%d\0", rooms[room_id].name, rooms[room_id].active_client_counter, ROOM_CAPACITY);
                write_client(sock, message); // Informing client
                sprintf(message, "update_counter;%d\0",rooms[room_id].active_client_counter);
                int t = 0;
                for(t = 0 ; t < rooms[room_id].client_counter-1 ; t++){ // Informing all clients in the same room to update their online counters.
                    // Checking socket status of client. If the socket connection is broken, we should not try to send data to avoid segmentation fault.
                    if(check_socket_status(&clients[rooms[cl->room_id].client_ids[t]]) || clients[rooms[cl->room_id].client_ids[t]].room_id != cl->room_id)
                        continue;
                    write_client(clients[rooms[room_id].client_ids[t]].socket, message); // Socket connection is stable, we can send message.
                }
                sem_post(&mutex); // Exiting critical region.
                char result[100];
                sprintf(result, "Successful, room \"%s\" has been entered", rooms[room_id].name);
                console_log(cl->nickname, cl->id, cl->socket, "Attempted to enter a room", result);

            }
            else{ // Client is not in lobby. So, he/she can enter a room.
                console_log(cl->nickname, cl->id, cl->socket, "Attempted to enter a room", "Rejected because of user is not in lobby");
                write_client(sock, "You have to be in lobby to enter a room!");
            }
        }
        else if(strcmp(splitted[0], "-quit") == 0){
            if(cl->location == LOCATION_ROOM){ // Client has to be in a room to quit.
                sem_wait(&mutex); // Entering critical region
                rooms[cl->room_id].active_client_counter -= 1; // Updating client counter of room.
                if(rooms[cl->room_id].active_client_counter == 0){ // Room is empty, room has to be closed.
                    rooms[cl->room_id].is_active = ROOM_INACTIVE;
                    strcpy(rooms[cl->room_id].name, ""); // The name of closed room is deleted to be able to create new room with this name.
                }
                else{ // Room is not empty, means there left another clients in room. So, their online counters should be updated.
                    char message[100] = {'\0'};
                    sprintf(message, "update_counter;%d\0",rooms[cl->room_id].active_client_counter);
                    int t = 0;
                    for(t = 0 ; t < rooms[cl->room_id].client_counter ; t++){
                        if(check_socket_status(&clients[rooms[cl->room_id].client_ids[t]])) // Checking broken sockets.
                            continue;
                        write_client(clients[rooms[cl->room_id].client_ids[t]].socket, message);
                    }
                }
                cl->location = LOCATION_LOBBY; // Client is in lobby now.
                cl->room_id = -1;
                sem_post(&mutex); // Exiting critical region.
                char message[100] = {'\0'};
                sprintf(message, "login_success;%d;%s\0", cl->id, cl->nickname);
                write_client(cl->socket, message); // Informing client, he/she entered to lobby.
            }
            else{
                console_log(cl->nickname, cl->id, cl->socket, "Attempted to quit from a room", "Rejected because of user is not in a room\0");
                write_client(sock, "You have to be in a room to quit from a room!\0");
            }
        }
        else if(strcmp(splitted[0], "-msg") == 0){
            if(cl->location == LOCATION_ROOM){ // Client has to be in room to send message.
                int z = 0;
                sem_wait(&mutex); // Entering critical region
                for(z = 0 ; z < rooms[cl->room_id].client_counter ; z++){
                    char message[250];
                    sprintf(message, "new_message;%s;%s\0", cl->nickname, splitted[1]);
                    if(check_socket_status(&clients[rooms[cl->room_id].client_ids[z]]) || clients[rooms[cl->room_id].client_ids[z]].room_id != cl->room_id)
                        continue;
                    write_client(clients[rooms[cl->room_id].client_ids[z]].socket, message);
                }
                sem_post(&mutex); // Exiting critical region.
            }
            else{
                console_log(cl->nickname, cl->id, cl->socket, "Attempted to send a message", "Rejected because of user is not in room\0");
                write_client(sock, "You have to be in room to send a message!\0");
            }
        }
        else if(strcmp(splitted[0], "-whoami") == 0){
            write_client(cl->socket, cl->nickname);
        }
        else if(strcmp(splitted[0], "-exit") == 0){

            sem_wait(&mutex); // Entering critical region.
            if(cl->room_id != -1){ // Exiting from a room. It is like quit command.
                rooms[cl->room_id].active_client_counter -= 1;
                if(rooms[cl->room_id].active_client_counter == 0){
                    rooms[cl->room_id].is_active = ROOM_INACTIVE;
                    strcpy(rooms[cl->room_id].name, "\0");
                }
                else{
                    char message[100] = {'\0'};
                    sprintf(message, "update_counter;%d\0",rooms[cl->room_id].active_client_counter);
                    int t = 0;
                    for(t = 0 ; t < rooms[cl->room_id].client_counter ; t++){
                        if(check_socket_status(&clients[rooms[cl->room_id].client_ids[t]]) || clients[rooms[cl->room_id].client_ids[t]].room_id != cl->room_id)
                            continue;
                        write_client(clients[rooms[cl->room_id].client_ids[t]].socket, message);
                    }
                }
            }
            // If client is not a room, exiting easy.
            cl->connection_flag = DISCONNECTED;
            console_log(cl->nickname, cl->id, cl->socket, "Attempted to exit", "Successful");
            sem_post(&mutex);
            break;
        }
        else{ // Unknown command
            if(cl->location == LOCATION_ROOM){ // Unknown commands are messages if the client in room. Sending message all clients in the same room.
                int z = 0;
                sem_wait(&mutex);
                for(z = 0 ; z < rooms[cl->room_id].client_counter ; z++){
                    char message[250];
                    sprintf(message, "new_message;%s;%s\0", cl->nickname, client_message);
                    if(check_socket_status(&clients[rooms[cl->room_id].client_ids[z]]) || clients[rooms[cl->room_id].client_ids[z]].room_id != cl->room_id)
                        continue;
                    write_client(clients[rooms[cl->room_id].client_ids[z]].socket, message);
                }
                sem_post(&mutex);
            }
            else{
                write_client(cl->socket, "Invalid command!");
            }
        }
    }


    return 0;
}

/*
    Special split is used to split client messages.
    Example:
        char* client_msg = "-msg This is a test message from client."
        char** splitted_client_msg = split(client_msg, ' ');
        splitted_client_msg[0] is "-msg"
        splitted_client_msg[0] is "This is a test message from client."

*/
char** split(char* string, char delimiter){

    int i;
    int delimiter_location = -1;
    int str_cnt = 0;
    char **str_arr = (char**)malloc(sizeof(char*) * 2);
    string = trim(string);

    for(i=0;i<strlen(string);i++){
        if(string[i] == delimiter){
            delimiter_location = i;
            break;
        }
    }

    if(delimiter_location != -1){
        str_arr[0] = (char*)malloc(sizeof(char) * delimiter_location == 0 ? 1 : delimiter_location);
        int k = 0;

        do{
            str_arr[0][k] = string[k];

        }while(string[k++ + 1] != delimiter);

        str_arr[0][k++] = '\0';
        str_arr[1] = (char*)malloc(sizeof(char) * (strlen(string) - k + 1));

        int t = 0;
        while(k < strlen(string)){
            str_arr[1][t++] = string[k++];
        }
        str_arr[1][t] = '\0';

        str_arr[0] = trim(str_arr[0]);
        str_arr[1] = trim(str_arr[1]);

    }
    else {
        str_arr[0] = string;
        str_arr[1] = "\0";
    }

    return str_arr;
}

/*
    Left and right trim for given string.
    Example:
        char* str = "    This is a string.      ";
        str = trim(str);
        str is "This is a string.";
*/
char* trim(char* string){

    if(!string) return string;

    while(*string == ' '){
        string = string + 1;
    }

    char* back = string + (strlen(string) - 1);
    while(*back == ' '){
        *(back) = '\0';
        back = back -1;
    }

    return string;
}

/*
    Checks given room name if it is unique.
    Searches given name in the rooms array and in the reserved names.
*/
int check_room_name_valid(char* room_name){

    int i = 0;

    for(i = 0 ; i < total_room_number ; i++){
        if(strcmp(room_name, rooms[i].name) == 0)
            return 0;
    }

    for(i = 0 ; i < reserved_room_name_counter ; i++){
        if(strcmp(room_name, reserved_room_names[i]) == 0)
            return 0;
    }

    return 1;
}

/*
    Sends given message to given socket.
*/
void write_client(int __fd, char* message){

    char* msg = (char*)malloc(sizeof(char) * strlen(message));
    strcpy(msg, message);
    const void* __buf = (const void*)msg;
    size_t __n = strlen(message);
    write(__fd, __buf, __n);

}

/*
    Prettify and log given data to console.
*/
void console_log(char* nickname, int client_id, int socket, char* action, char* result){

    printf("Nickname: %s\n"
            "Client ID: %d\n"
            "Socket: %d\n"
            "Action: %s\n"
            "Result: %s\n\n", nickname, client_id, socket, action, result);
}

/*
    Find room id with given name.
*/
int get_room_id_by_name(char* room_name){

    int i = 0;
    if(strcmp(room_name, "") == 0)
        return -1;

    for(i = 0 ; i < total_room_number ; i++){
        if(strcmp(room_name, rooms[i].name) == 0){
            return i;
        }
    }

    return -1;
}

/*
    Checking socket status for a client.
    This is really important operation.
    Program crashes(seg fault) if we try to send
    data on broken socket. So, we need to
    check first the status of socket.
*/
int check_socket_status(client* cl){

    int __fd = cl->socket;
    int error = 0;
    socklen_t len = sizeof(error);
    int retval = getsockopt(__fd, SOL_SOCKET, SO_ERROR, &error, &len);

    if(cl->connection_flag == DISCONNECTED){
        return 1;
    }

    if(retval != 0){
        printf("Error getting socket error code: %s\n", strerror(retval));
        cl->connection_flag = DISCONNECTED;
        return 1;
    }

    if (error != 0) {
        printf("Socket error: %s\n", strerror(error));
        cl->connection_flag = DISCONNECTED;
        return 1;
    }

    return 0;
}

/*
    Checking whether the given customer is in the given room.
*/
int is_room_contain_client(int room_id, int client_id){
    int i = 0;
    for(i = 0 ; i < rooms[room_id].client_counter ; i++){
        if(rooms[room_id].client_ids[i] == client_id){
            return 1;
        }
    }
    return 0;
}

/*
    Checking given password and creating proper message for client.
*/
int validate_password(char* password, char* result_buffer){

    if(strlen(password) < 4){
        strcpy(result_buffer, "unsuitable_password;Password cannot be shorter than 4 characters!\0");
        return 0;
    }
    else if(strstr(password, " ") != NULL){
        strcpy(result_buffer, "unsuitable_password;Password cannot contain space character!\0");
        return 0;
    }

    strcpy(result_buffer, "suitable_password;Password accepted!\0");
    return 1;
}
