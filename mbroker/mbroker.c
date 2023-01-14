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
#include"../fs/operations.c"
#include"../fs/state.h"
#include"../fs/config.h"

tfs_params param;

#define MAX_MESSAGE_SIZE 1025//289
#define MAX_PIPE_PATH_LEN 256
#define MAX_BOX_NAME_LEN 32
#define MAX_MESSAGE_LEN 1024
#define UINT8_T_SIZE 1
#define INT32_T_SIZE 4
#define UINT64_T_SIZE 8
#define MAX_NUMBER_BOXES (param.max_inode_count)

typedef struct{
    char box_name[MAX_BOX_NAME_LEN];
    int has_publisher; //0 1 or -1(empty)
} box;

box *box_table;

/*returns the index of the box, -1 if it do not exist*/
size_t get_box_index(char *box_name){
    for(size_t i = 0; i < MAX_NUMBER_BOXES; i++){
        if(strcmp(box_table[i].box_name, box_name) == 0){
            return i;
        }
    }
    return (size_t)-1;
}

/*returns the index of the box, -1 if it do not exist*/
int add_box(char *box_name){
    printf("MAX_NUMBER_BOXES = %lu\n", MAX_NUMBER_BOXES);
    for(size_t i = 0; i < MAX_NUMBER_BOXES; i++){ //searchs for the first empty index
        printf("sera\n");
        if(box_table[i].has_publisher == -1 ){
            printf("hear add\n");
            memcpy(box_table[i].box_name, box_name, MAX_BOX_NAME_LEN);
            box_table[i].has_publisher = 0;
            return 0;
        }
    }
    return -1;
}


//handle the publisher request
int handle_request_1(char *request){
    char pub_pipe_name[MAX_PIPE_PATH_LEN];
    char box_name[MAX_BOX_NAME_LEN];
	char path_box_name[MAX_BOX_NAME_LEN + 1];

    memcpy(pub_pipe_name, request + UINT8_T_SIZE, MAX_PIPE_PATH_LEN);
    memcpy(box_name, request + UINT8_T_SIZE + MAX_PIPE_PATH_LEN, MAX_BOX_NAME_LEN);
	snprintf(path_box_name, MAX_BOX_NAME_LEN + 1, "/%s", box_name);

    int box_fd = tfs_open(path_box_name, TFS_O_APPEND);                                   //doubt - open modes
    if(box_fd == -1){
        WARN("pub_request : box do not exist")
        return -1;
    }

    int pub_fd = open(pub_pipe_name, O_RDONLY);
    if(pub_fd == -1)
        PANIC("mbroker : failed to open pub pipe"); //debug

    char message[UINT8_T_SIZE + MAX_MESSAGE_LEN];
    printf("hear\n");

    box_table[get_box_index(box_name)].has_publisher++; //box now have a publisher

    while(1){

        ssize_t message_size = read(pub_fd, message, UINT8_T_SIZE + MAX_MESSAGE_LEN);
        if(message_size == -1) {
            WARN("error reading from pub_pipe");
            return -1;
        } else if (message_size == 0) {
			printf("pipe_closed\n");
            box_table[get_box_index(box_name)].has_publisher--; //box now have a publisher
            WARN("pub_pipe closed");
            break;
        } else {
            if(message[0] != 9){ //op_code = 9
                WARN("invalid message from pub");
                return -1;
            }
            printf("message to box = %s\n", message + 1);
			printf("message_size = %ld\n", sizeof(message));
			ssize_t size = tfs_write(box_fd, message + UINT8_T_SIZE, strlen(message + UINT8_T_SIZE) + 1); // +1 to write the last '/0'
			printf("written = %ld\n", size);
			if(size == -1){
                WARN("error writing in box");
                return -1;
            }
        }
    }

    if(tfs_close(box_fd) == -1){
        PANIC("error closing box"); //debug
    }

    
    return 0;
}

int handle_request_2(char *request){
    char sub_pipe_name[MAX_PIPE_PATH_LEN];
    char box_name[MAX_BOX_NAME_LEN];
	char path_box_name[MAX_BOX_NAME_LEN + 1];
	fprintf(stdout, "hear\n");

    memcpy(sub_pipe_name, request + UINT8_T_SIZE, MAX_PIPE_PATH_LEN);
    memcpy(box_name, request + UINT8_T_SIZE + MAX_PIPE_PATH_LEN, MAX_BOX_NAME_LEN);
	snprintf(path_box_name, MAX_BOX_NAME_LEN + 1, "/%s", box_name);
	fprintf(stdout, "hear2\n");

    int box_fd = tfs_open(path_box_name, 0);                                   //doubt - open modes
    if(box_fd == -1){
        WARN("sub_request : box do not exist")
        return -1;
    }
	fprintf(stdout, "hear3\n");
    int sub_fd = open(sub_pipe_name, O_WRONLY);
    if(sub_fd == -1)
        PANIC("mbroker : failed to open sub pipe"); //debug

	char message[UINT8_T_SIZE + MAX_MESSAGE_LEN] = {0};
	message[0] = 10; //op_code 10
    int i = 1; //counter that starts after the opcode

	while(1){
        char c;
        ssize_t message_size = tfs_read(box_fd, &c, sizeof(char));// read one character at a time
        if(message_size == -1) {
			printf("error reading from named box\n");
            WARN("error reading from named box");
            return -1;
        } else if (message_size == 0) {
			printf("no more messages to read\n");
            WARN("no more messages to read");         // when threads are working need to change this
            break;
        }else if (c != '\0') { // if the character is not a null character
            message[i] = c; // add the character to the buffer
            i++;
        } else {
            printf("Message: %s\n", message); // print the message
            if(write(sub_fd, message, UINT8_T_SIZE + MAX_MESSAGE_LEN) == -1){
                WARN("error writing in sub");
                return -1;
			}
            memset(message, 0, UINT8_T_SIZE + MAX_MESSAGE_LEN); //cleans the buffer
            i = 1; // reset the buffer index
        }
	}

	if(tfs_close(box_fd) == -1){
		PANIC("error closing box"); //debug
	}

    return 0;
}

int handle_request_3(char *request){
    char manager_pipe_name[MAX_PIPE_PATH_LEN];
    char box_name[MAX_BOX_NAME_LEN];
	char path_box_name[MAX_BOX_NAME_LEN + 1];
    char answer[UINT8_T_SIZE + INT32_T_SIZE + MAX_MESSAGE_LEN] = {0}; //inicialize the buffer with '/0'
    char error_message[MAX_MESSAGE_LEN] = {0};
    int32_t return_code;
    answer[0] = 4; //op_code

    memcpy(manager_pipe_name, request + UINT8_T_SIZE, MAX_PIPE_PATH_LEN);
    memcpy(box_name, request + UINT8_T_SIZE + MAX_PIPE_PATH_LEN, MAX_BOX_NAME_LEN);

    int manager_fd = open(manager_pipe_name, O_WRONLY);
    if(manager_fd == -1)
        PANIC("failed to open manager pipe");//debug should be warn
    
    //checks if the box already exists
    if(get_box_index(box_name) != -1){
        return_code = -1;
        memcpy(answer + UINT8_T_SIZE, &return_code, INT32_T_SIZE);
        strncpy(answer + UINT8_T_SIZE + INT32_T_SIZE, "box already exists", MAX_MESSAGE_LEN - 1); 

        if(write(manager_fd, answer, UINT8_T_SIZE + INT32_T_SIZE + MAX_MESSAGE_LEN) < 0)
            PANIC("error writing answer to manager pipe");

        return -1;
    }

    snprintf(path_box_name, MAX_BOX_NAME_LEN + 1, "/%s", box_name); //add '/' to box_name

    int box_fd = tfs_open(path_box_name, TFS_O_CREAT);
    if(box_fd == -1){
        WARN("failed to open box");
        return_code = -1;
        memcpy(answer + UINT8_T_SIZE, &return_code, INT32_T_SIZE);
        strncpy(answer + UINT8_T_SIZE + INT32_T_SIZE, "failed to open box", MAX_MESSAGE_LEN - 1);   // doubt - error message????

        if(write(manager_fd, answer, UINT8_T_SIZE + INT32_T_SIZE + MAX_MESSAGE_LEN) < 0)
            PANIC("error writing answer to manager pipe");

        return -1;
    }

    return_code = 0;
    memcpy(answer + UINT8_T_SIZE, &return_code, INT32_T_SIZE);
    strncpy(answer + UINT8_T_SIZE + INT32_T_SIZE, error_message, MAX_BOX_NAME_LEN - 1);   // doubt - error message????

    if(write(manager_fd, answer, UINT8_T_SIZE + INT32_T_SIZE + MAX_BOX_NAME_LEN) < 0)
        PANIC("error writing answer to manager pipe");

    

    if(tfs_close(box_fd) == -1){
        PANIC("error closing box")
    }

    if(add_box(box_name))
        PANIC("error adding box to box_table(not suposed)") // open already fails if there are no more space for creting files

    return 0;
}

int handle_request_general(char *message){
    switch(message[0]) {
        case 1: // session request from publisher
            printf("case 1\n"); //debug
            handle_request_1(message);
            break;
        case 2: // session request from subscriber
            printf("case 2\n"); //debug
			handle_request_2(message);
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

    if(tfs_init(NULL) == -1)
        PANIC("failed to init tfs")

    param = tfs_default_params();

    printf("MAX_NUMBER_BOXES = %lu\n", param.max_inode_count);
        
    box_table = (box*)malloc(MAX_NUMBER_BOXES * sizeof(box));
    if(box_table == NULL){
        PANIC("failed to alocate memory for the box_table")
    }
    /*inicializes the box table*/
    for (size_t i = 0; i < MAX_NUMBER_BOXES; i++) {
        memset(box_table[i].box_name, 0, MAX_BOX_NAME_LEN);
        box_table[i].has_publisher = -1; //simbolizes empty index
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

    free(box_table);

    return 0;
}
