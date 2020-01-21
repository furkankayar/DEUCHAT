/*
    DEUCHAT CLIENT
    Written by Furkan Kayar

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>


#define LOCALHOST           "127.0.0.1"
#define PORT                3205
#define SOCKET_CREATE_ERR   1
#define BINDING_ERR         2
#define ACCEPT_ERR          3
#define THREAD_CREATE_ERR   4
#define CONNECTION_ERR      5
#define SEND_ERR            6
#define RECV_ERR            7

#define GRAVE               -1
#define LOBBY               0
#define ROOM                1

#define clear()             printf("\033[H\033[J")
#define gotoxy(x,y)         printf("\033[%d;%dH", (y), (x))
#define COLOR_RED           "\x1b[31m"
#define COLOR_GREEN         "\x1b[32m"
#define COLOR_YELLOW        "\x1b[33m"
#define COLOR_BLUE          "\x1b[34m"
#define COLOR_MAGENTA       "\x1b[35m"
#define COLOR_CYAN          "\x1b[36m"
#define COLOR_RESET         "\x1b[0m"

void* server_handler(void*);
char** split(char*, char, int*);
void draw(void);
int kbhit(void);


char buffer[250] = {'\0'}; // Keeps all characters inputted by keyboard.
int buf_loc = 0;
char nickname[100] = {'\0'}; // Client's nickname.
char client_location = GRAVE; // Client's location.

int msg_ptr_loc = 0;
int msg_ptr_col = 0;
char all_console[10000] = {'\0'}; // Keeps all data that is printed to console.

int main(){

    int socket_desc;
    int bytes_read = 0;
    struct sockaddr_in server;
    char* message;
    char server_reply[3000];
    pthread_t server_listener;

    clear();

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_desc == -1){

        puts("Coult not create socket");
        return SOCKET_CREATE_ERR;
    }

    server.sin_addr.s_addr = inet_addr(LOCALHOST);
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    if(connect(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0){

        puts("Connection error");
        return CONNECTION_ERR;
    }


    if((bytes_read = recv(socket_desc, server_reply, 2000, 0)) < 0){
        puts("Recv failed");
        return RECV_ERR;
    }

    // Connection established.
    server_reply[bytes_read] = '\0';
    puts(server_reply);


    if((bytes_read = recv(socket_desc, server_reply, 2000, 0)) < 0){
        puts("Recv failed");
        return RECV_ERR;
    }

    server_reply[bytes_read] = '\0';
    puts(server_reply);

    pthread_create(&server_listener, NULL, server_handler, (void*)&socket_desc); // This thread is used to communicate with server while user entering commands.

    scanf("%s", message); // Get nickname from user.
    send(socket_desc, message, strlen(message), 0); // Send nickname to server.


   int run = 1;

    while(run){

        while(kbhit()){ // kbhit() returns true when a key is pushed.
            int ch = getchar(); // Read pushed key from buffer.

            if(ch == 10){ // Enter key
                buffer[buf_loc] = '\0';
                buf_loc = 0;

                if(strcmp(buffer, "") != 0){

                    int* length = (int*)malloc(sizeof(int));
                    char** splitted = split(buffer, ' ', length);
                    if(!((strcmp(splitted[0], "-msg") == 0 || splitted[0][0] != '-') && client_location == ROOM)){

                        strcat(all_console, buffer);
                        strcat(all_console, "\n ");
                        msg_ptr_loc += 1;
                    }

                    if(send(socket_desc, buffer, strlen(buffer), 0) < 0){ // Send entered command to the server.

                        puts("Send failed");
                        return SEND_ERR;
                    }

                    if(strcmp(buffer, "-exit") == 0){ // Terminate program.
                        run = 0;
                        break;
                    }

                    memset(buffer, 0, sizeof(buffer));
                    draw();
                }
            }
            else if(ch == 127){ // Backspace key
                if(buf_loc != 0){
                    buf_loc -= 1;
                    buffer[buf_loc] = '\0';
                    draw();
                }
            }
            else if(ch == 32){ // Space key
                if(strcmp(buffer, "") != 0){
                    buffer[buf_loc] = ch;
                    buffer[buf_loc+1] = '\0';
                    buf_loc += 1;
                    draw();
                }
            }
            else{ // Any key
              buffer[buf_loc] = ch;
              buffer[buf_loc+1] = '\0';
              buf_loc += 1;
              draw();
            }

        }
    }


    close(socket_desc);

    return 0;
}

/*
    Analyzes server responses for sent messages.
*/
void* server_handler(void* socket){

    char server_reply[3000] = {'\0'};
    int socket_desc = *((int*)socket);
    int bytes_read = 0;
    int client_id = -1;



    while(1){

        if((bytes_read = recv(socket_desc, server_reply, 2000, 0)) < 0){
            puts("Recv failed");
        }
        server_reply[bytes_read] = '\0';
        int* length = (int*)malloc(sizeof(int));
        char** splitted = split(server_reply, ';', length); // Server responses with special format (ex. $1;$2;$3;$4)

        if(strcmp(splitted[0], "login_success") == 0){
            client_id = atoi(splitted[1]); //String to int
            strcpy(nickname, splitted[2]);
            memset(all_console,0,sizeof(all_console));
            char msg[250] = {'\0'};
            sprintf(msg, " Welcome %s enter your command!\n\n ", nickname);
            strcat(all_console, msg);
            msg_ptr_loc = 3;
            msg_ptr_col = 2;
            client_location = LOBBY;
            draw();
        }
        else if(strcmp(splitted[0], "room_created") == 0){
            memset(all_console, 0, sizeof(all_console));
            char msg[250] = {'\0'};
            sprintf(msg, " Welcome to the chat room %s!\n Online: %s\n Capacity: %s\n\n ", splitted[1], splitted[2], splitted[3]);
            strcat(all_console, msg);
            memset(buffer, 0, sizeof(buffer));
            msg_ptr_loc = 5;
            msg_ptr_col = 2;
            client_location = ROOM;
            draw();
        }
        else if(strcmp(splitted[0], "room_entered") == 0){
            memset(all_console, 0, sizeof(all_console));
            char msg[250] = {'\0'};
            sprintf(msg, " Welcome to the chat room %s!\n Online: %s  \n Capacity: %s\n\n ", splitted[1], splitted[2], splitted[3]);
            strcat(all_console, msg);
            memset(buffer, 0, sizeof(buffer));
            msg_ptr_loc = 5;
            msg_ptr_col = 2;
            client_location = ROOM;
            draw();
        }
        else if(strcmp(splitted[0], "update_counter") == 0){
            int first_line_end = 0;
            while(all_console[first_line_end++] != '\n');
            int capacity = atoi(splitted[1]);
            all_console[first_line_end + 9] = splitted[1][0];
            if(capacity >= 10){
                all_console[first_line_end + 10] = splitted[1][1];
            }
            draw();

        }
        else if(strcmp(splitted[0], "new_message") == 0){
            char msg[250] = {'\0'};
            if(strcmp(splitted[1], nickname) == 0){
                sprintf(msg, COLOR_GREEN " %s:" COLOR_RESET " %s\n ", splitted[1], splitted[2]);
            }
            else{
                sprintf(msg, COLOR_CYAN " %s:" COLOR_RESET " %s\n ", splitted[1], splitted[2]);
            }

            strcat(all_console, msg);
            msg_ptr_loc += 1;
            draw();
        }
        else if(strcmp(splitted[0], "set_password") == 0){
            strcat(all_console, "Choose a password: ");
            memset(buffer, 0, sizeof(buffer));
            msg_ptr_col = 21;
            draw();
        }
        else if(strcmp(splitted[0], "unsuitable_password") == 0){
            strcat(all_console, splitted[1]);
            strcat(all_console, "\n Choose new password: ");
            memset(buffer, 0, sizeof(buffer));
            msg_ptr_col = 23;
            msg_ptr_loc += 1;
            draw();
        }
        else if(strcmp(splitted[0], "suitable_password") == 0){
            strcat(all_console, splitted[1]);
            memset(buffer, 0, sizeof(buffer));
            msg_ptr_col = 22;
            draw();
        }
        else if(strcmp(splitted[0], "request_password") == 0){
            strcat(all_console, "Enter password: ");
            memset(buffer, 0, sizeof(buffer));
            msg_ptr_col = 18;
            draw();
        }
        else if(strcmp(splitted[0], "incorrect_password") == 0){
            strcat(all_console, splitted[1]);
            strcat(all_console, "\n ");
            memset(buffer, 0, sizeof(buffer));
            msg_ptr_col = 2;
            msg_ptr_loc += 1;
            draw();
        }
        else if(strcmp(splitted[0], "list") == 0){

            if(strcmp(splitted[1], "") == 0){
                strcat(all_console,"There is no active room!\n ");
                msg_ptr_loc += 1;
                msg_ptr_col = 2;
                memset(buffer, 0, sizeof(buffer));
                draw();
            }
            else{
                strcat(all_console, splitted[1]);
                strcat(all_console, "\n ");
                int line_num = 0;
                int z = 0;
                for(z = 0 ; z < strlen(splitted[1]) ; z++) if(splitted[1][z] == '\n') line_num += 1;
                msg_ptr_loc += line_num + 1;
                msg_ptr_col = 2;
                memset(buffer, 0, sizeof(buffer));
                draw();
            }

        }
        else{

            strcat(all_console, server_reply);
            strcat(all_console, "\n ");
            int line_num = 0;
            int z = 0;
            for(z = 0 ; z < strlen(server_reply) ; z++) if(server_reply[z] == '\n') line_num += 1;
            msg_ptr_loc += line_num + 1;
            msg_ptr_col = 2;
            memset(buffer, 0, sizeof(buffer));
            draw();
        }

    }
}

/*
    Spliting given string with given delimiter and inserts the array size in length pointer.
*/
char** split(char* string, char delimiter, int* length){

    int i;
    int delimiter_cnt = 0;
    int str_cnt = 0;
    char **str_arr;

    for(i=0;i<strlen(string);i++)
        if(string[i] == delimiter)
            delimiter_cnt++;

    if(string[strlen(string) - 1] == '\n') string[strlen(string) - 1] = '\0';

    str_arr = (char**)malloc(sizeof(char*) * (delimiter_cnt + 1));

    *length = delimiter_cnt + 1;


    for(i=0;i<strlen(string);){

        if(i==0 || string[i] == delimiter){
            char *tmp = (char*)malloc(sizeof(char) * 500);
            int k=0;
            if(i != 0) i++;
            while(string[i] != delimiter && i < strlen(string)){
                tmp[k++] = string[i++];
            }
            tmp[k] = '\0';
            str_arr[str_cnt++] = tmp;

        }
    }

    return str_arr;
}

/*
    Prints data to console.
*/
void draw(void){

    clear();
    printf("%s", all_console);

    gotoxy(msg_ptr_col, msg_ptr_loc);
    if(buffer[0] == '-'){
        int* length = (int*)malloc(sizeof(int));
        char** splitted_buf = split(buffer, ' ', length);
        printf(COLOR_YELLOW "%s" COLOR_RESET, splitted_buf[0]);
        int i = 1;
        for(i = 1 ; i < *length ; i++){
            printf(" %s", splitted_buf[i]);
        }
    }
    else{
        printf("%s", buffer);
    }


}

/*
    This function exists in conio.h library that is not available in linux.
    It is used to get input character by character from user without blocking the program flow.
    Reference of this function: https://cboard.cprogramming.com/c-programming/63166-kbhit-linux.html
*/
int kbhit(void){

  struct termios oldt, newt;
  int ch;
  int oldf;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  if(ch != EOF){
    ungetc(ch, stdin);
    return 1;
  }

  return 0;
}


