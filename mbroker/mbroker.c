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
#include"../fs/operations.h"
#include"../fs/state.h"
#include"../fs/config.h"


#define MAX_MESSAGE_SIZE 1025//289
#define MAX_PIPE_PATH_LEN 256
#define MAX_BOX_NAME_LEN 32
#define MAX_MESSAGE_LEN 1024
#define UINT8_T_SIZE 1
#define INT32_T_SIZE 4
#define UINT64_T_SIZE 8

//handle the publisher request
int handle_request_1(char *request){
    char pub_pipe_name[MAX_PIPE_PATH_LEN];
    char box_name[MAX_BOX_NAME_LEN];

    memcpy(pub_pipe_name, request + UINT8_T_SIZE, MAX_PIPE_PATH_LEN);
    memcpy(box_name, request + UINT8_T_SIZE + MAX_PIPE_PATH_LEN, MAX_BOX_NAME_LEN);

    int box_fd = tfs_open(box_name, TFS_O_APPEND);                                   //doubt - open modes
    if(box_fd == -1){
        WARN("pub_request : box do not exist")                                                       
        return -1;
    }

    int pub_fd = open(pub_pipe_name, O_RDONLY);
    if(pub_fd == -1)
        PANIC("mbroker : failed to open pub pipe"); //debug  

    char message[UINT8_T_SIZE + MAX_MESSAGE_LEN];

    while(1){

        ssize_t message_size = read(pub_fd, message, UINT8_T_SIZE + MAX_MESSAGE_LEN);
        if(message_size == -1) {
            WARN("error reading from pub_pipe");
            return -1;
        } else if (message_size == 0) {
            WARN("pub_pipe closed");
            break;
        } else {
            if(message[0] != 9){ //op_code = 9
                WARN("invalid message from pub");
                return -1;
            }
            printf("message to box = %s\n", message + 1);
            if(tfs_write(box_fd, message + UINT8_T_SIZE, MAX_MESSAGE_LEN) == -1){
                WARN("error writing in box");
                return -1;
            }
        }
    }
    
    if(tfs_close(box_fd) == -1){
        PANIC("error closing box");
    }

    return 0;
}

int handle_request_3(char *request){
    char manager_pipe_name[MAX_PIPE_PATH_LEN];
    char box_name[MAX_BOX_NAME_LEN];
    char answer[UINT8_T_SIZE + INT32_T_SIZE + MAX_MESSAGE_LEN] = {0}; //inicialize the buffer with '/0' 
    char error_message[MAX_MESSAGE_LEN] = {0};
    int32_t return_code;
    answer[0] = 4; //op_code

    int manager_fd = open(manager_pipe_name, O_WRONLY);
    if(manager_fd == -1)
        PANIC("failed to open manager pipe");//debug should be warn

    memcpy(manager_pipe_name, request + UINT8_T_SIZE, MAX_PIPE_PATH_LEN);
    memcpy(box_name, request + UINT8_T_SIZE + MAX_PIPE_PATH_LEN, MAX_BOX_NAME_LEN);

    int box_fd = tfs_open(box_name, TFS_O_CREAT);                                   //doubt - open modes
    if(box_fd == -1){
        WARN("failed to create box");                                                       
        
        return_code = -1;
        strncpy(answer + UINT8_T_SIZE, return_code, INT32_T_SIZE - 1);
        strncpy(answer + UINT8_T_SIZE + INT32_T_SIZE, error_message, MAX_BOX_NAME_LEN - 1);   // doubt - error message????

        if(write(manager_fd, answer, UINT8_T_SIZE + INT32_T_SIZE + MAX_BOX_NAME_LEN) < 0)
            PANIC("error writing answer to manager pipe");

        return -1;
    }

    if(tfs_close(box_fd) == -1){
        PANIC("error closing box")
    }
    return 0;
}

int handle_request_general(char *message){
    switch(message[0]) {
        case 1: // session request from publisher
            //printf("case 1\n"); //debug
            handle_request_1(message);
            break;
        case 2: // session request from subscriber
            printf("case 2\n"); //debug
            break;
        case 3: // request from manager to create a box
            printf("case 3\n"); //debug
            handle_request_3(message);
            break;
        case 5: // request from manager to remove a box
            printf("case 5\n"); //debug
            break;
        case 7: // request from manager to list boxes
            printf("case 7\n"); //debug
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

    // printf("argv[1] = %s\n", register_pipe_name); // debug
    // printf("argv[2] = %d\n", max_sessions); // debug


    int register_pipe_fd_r = open(register_pipe_name, O_RDONLY);
    if(register_pipe_fd_r == -1){
        PANIC("error opening register_pipe");
        return -1;
    }

    //printf("hear\n"); //debug

    /*this is a trick so the read never returns 0*/
    int register_pipe_fd_w = open(register_pipe_name, O_WRONLY);
    if(register_pipe_fd_w == -1) {
        PANIC("error opening register_pipe");
        return -1;
    }


    while(1){
        char message[MAX_MESSAGE_SIZE] = {0};
        ssize_t message_size = read(register_pipe_fd_r, message, MAX_MESSAGE_SIZE);
        if(message_size == -1) {
            WARN("error reading from register_pipe");
        } else if (message_size == 0) {
            PANIC("register_pipe closed"); //must not happen
        } else {
            if(write(1, message, MAX_MESSAGE_SIZE) < 0)//debug
                PANIC("mbroker: write debug"); 
            printf("\n");
            handle_request_general(message);
        }
    }

    return 0;
}
