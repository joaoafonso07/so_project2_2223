#include "logging.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../producer-consumer/producer-consumer.h"
#include <unistd.h>     

#define MAX_MESSAGE_SIZE 289

int handle_request(uint8_t *message){
    switch(message[1]) {
        case 1: // send stuf to the pcq
            break;
        default:
            PANIC("invalid message code");
    }
    return 0;
}

int main(int argc, char **argv) {
    /*
    (void)argc;
    (void)argv;
    fprintf(stderr, "usage: mbroker <pipename>\n");
    WARN("unimplemented"); // TODO: implement
    */
    if(argc != 3){
        PANIC("invalid comand to inicialize mbroker")
    }

    char* register_pipe_name = argv[1];
    int max_sessions;

    if(sscanf(argv[2], "%d", &max_sessions) != 1) {
            PANIC("invalid comand to inicialize mbroker");
            return -1;        
        }

    unlink(register_pipe_name);

    if(mkfifo(register_pipe_name, 0660) == -1) {
        PANIC("error creating register_pipe");
        return -1;
    }
    printf("argv[1] = %s\n", register_pipe_name);
    printf("argv[2] = %d\n", max_sessions);


    int register_pipe_fd_r = open(register_pipe_name, O_RDONLY);
    if(register_pipe_fd_r == -1){
        PANIC("error opening register_pipe");
        return -1;
    }

    printf("hear\n");

    /*this is a trick so the read never returns 0*/
    int register_pipe_fd_w = open(register_pipe_name, O_WRONLY);
    if(register_pipe_fd_w == -1) {
        PANIC("error opening register_pipe");
        return -1;
    }


    while(1){
        uint8_t message[MAX_MESSAGE_SIZE];
        ssize_t message_size = read(register_pipe_fd_r, message, MAX_MESSAGE_SIZE);
        if(message_size == -1) {
            PANIC("error reading from register_pipe");
        } else if (message_size == 0) {
            WARN("register_pipe closed");
            break;
        } else {
            printf("received message: %s\n", message);
            handle_request(message);
        } 
    }

    return -1;
}
