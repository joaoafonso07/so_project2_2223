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

static size_t  max_number_boxes;

int actual_number_boxes = 0;

#define MAX_MESSAGE_SIZE 1025//289
#define MAX_PIPE_PATH_LEN 256
#define MAX_BOX_NAME_LEN 32
#define MAX_MESSAGE_LEN 1024
#define UINT8_T_SIZE 1
#define INT32_T_SIZE 4
#define UINT64_T_SIZE 8

typedef struct{
    char box_name[MAX_BOX_NAME_LEN];
    uint64_t has_publisher; //0 1
    uint64_t n_subscribers;
    uint64_t size;
    int empty; //0(empty) 1(full)

} box;

box *box_table;

pc_queue_t * pc_queue;

/*returns the index of the box, -1 if it do not exist*/
size_t get_box_index(char *box_name){
    for(size_t i = 0; i < max_number_boxes; i++){
        if(strcmp(box_table[i].box_name, box_name) == 0){
            return i;
        }
    }
    return (size_t)-1;
}

/*returns 0 if successful, -1 otherwise*/
int add_box(char *box_name){
    for(size_t i = 0; i < max_number_boxes; i++){ //searchs for the first empty index
        if(box_table[i].empty == 0 ){
            memcpy(box_table[i].box_name, box_name, MAX_BOX_NAME_LEN);
            box_table[i].empty = 1;
            return 0;
        }
    }
    return -1;
}


void remove_box(char *box_name){
    size_t i = get_box_index(box_name);

    memset(box_table[i].box_name, 0, MAX_BOX_NAME_LEN);
    box_table[i].empty = 0; //simbolizes empty index
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

    box_table[get_box_index(box_name)].has_publisher++; //box now have a publisher

    while(1){

        ssize_t message_size = read(pub_fd, message, UINT8_T_SIZE + MAX_MESSAGE_LEN);
        if(message_size == -1) {
            WARN("error reading from pub_pipe");
            return -1;
        } else if (message_size == 0) {
            box_table[get_box_index(box_name)].has_publisher--; //box don't have a publisher now
            WARN("pub_pipe closed");
            break;
        } else {
            if(message[0] != 9){ //op_code = 9
                WARN("invalid message from pub");
                return -1;
            }
			ssize_t size = tfs_write(box_fd, message + UINT8_T_SIZE, strlen(message + UINT8_T_SIZE) + 1); // +1 to write the last '/0'
            box_table[get_box_index(box_name)].size += (uint64_t)size;
			if(size == -1){
                WARN("error writing in box");
                return -1;
            }
        }
    }

    if(tfs_close(box_fd) == -1){
        WARN("error closing box"); //debug
    }

    
    return 0;
}

int handle_request_2(char *request){
    char sub_pipe_name[MAX_PIPE_PATH_LEN];
    char box_name[MAX_BOX_NAME_LEN];
	char path_box_name[MAX_BOX_NAME_LEN + 1];

    memcpy(sub_pipe_name, request + UINT8_T_SIZE, MAX_PIPE_PATH_LEN);
    memcpy(box_name, request + UINT8_T_SIZE + MAX_PIPE_PATH_LEN, MAX_BOX_NAME_LEN);
	snprintf(path_box_name, MAX_BOX_NAME_LEN + 1, "/%s", box_name);

    int box_fd = tfs_open(path_box_name, 0);                                   //doubt - open modes
    if(box_fd == -1){
        WARN("sub_request : box do not exist")
        return -1;
    }

    box_table[get_box_index(box_name)].n_subscribers ++;

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
            WARN("error reading from named box");
            return -1;
        } else if (message_size == 0) {
            WARN("no more messages to read: %s", strerror(errno));        // when threads are working need to change this
            break;
        }else if (c != '\0') { // if the character is not a null character
            message[i] = c; // add the character to the buffer
            i++;
        } else {
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

    box_table[get_box_index(box_name)].n_subscribers --;

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
    
    actual_number_boxes ++;

    return 0;
}

int handle_request_5(char *request){
    char manager_pipe_name[MAX_PIPE_PATH_LEN];
    char box_name[MAX_BOX_NAME_LEN];
	char path_box_name[MAX_BOX_NAME_LEN + 1];
    char answer[UINT8_T_SIZE + INT32_T_SIZE + MAX_MESSAGE_LEN] = {0}; //inicialize the buffer with '/0'
    char error_message[MAX_MESSAGE_LEN] = {0};
    int32_t return_code;
    answer[0] = 6; //op_code

    memcpy(manager_pipe_name, request + UINT8_T_SIZE, MAX_PIPE_PATH_LEN);
    memcpy(box_name, request + UINT8_T_SIZE + MAX_PIPE_PATH_LEN, MAX_BOX_NAME_LEN);

    int manager_fd = open(manager_pipe_name, O_WRONLY);
    if(manager_fd == -1)
        PANIC("failed to open manager pipe");//debug should be warn
    
    //checks if the box already exists
    if(get_box_index(box_name) == -1){
        return_code = -1;
        memcpy(answer + UINT8_T_SIZE, &return_code, INT32_T_SIZE);
        strncpy(answer + UINT8_T_SIZE + INT32_T_SIZE, "box does not exist", MAX_MESSAGE_LEN - 1); 

        if(write(manager_fd, answer, UINT8_T_SIZE + INT32_T_SIZE + MAX_MESSAGE_LEN) < 0)
            PANIC("error writing answer to manager pipe");

        return -1;
    }

    snprintf(path_box_name, MAX_BOX_NAME_LEN + 1, "/%s", box_name); //add '/' to box_name

    if(tfs_unlink(path_box_name) == -1){
        WARN("failed to unlink box");
        return_code = -1;
        memcpy(answer + UINT8_T_SIZE, &return_code, INT32_T_SIZE);
        strncpy(answer + UINT8_T_SIZE + INT32_T_SIZE, "failed to remove box", MAX_MESSAGE_LEN - 1);   // doubt - error message????

        if(write(manager_fd, answer, UINT8_T_SIZE + INT32_T_SIZE + MAX_MESSAGE_LEN) < 0)
            PANIC("error writing answer to manager pipe");

        return -1;
    }

    return_code = 0;
    memcpy(answer + UINT8_T_SIZE, &return_code, INT32_T_SIZE);
    strncpy(answer + UINT8_T_SIZE + INT32_T_SIZE, error_message, MAX_BOX_NAME_LEN - 1);   // doubt - error message????

    if(write(manager_fd, answer, UINT8_T_SIZE + INT32_T_SIZE + MAX_BOX_NAME_LEN) < 0)
        PANIC("error writing answer to manager pipe");

    remove_box(box_name);
    actual_number_boxes --;

    return 0;
}


int handle_request_7(char* request){
    char manager_pipe_name[MAX_PIPE_PATH_LEN];
    char answer[UINT8_T_SIZE + UINT8_T_SIZE + MAX_BOX_NAME_LEN + UINT64_T_SIZE + UINT64_T_SIZE + UINT64_T_SIZE] = {0}; //inicialize the buffer with '/0'
    answer[0] = 8; //op_code
    uint8_t last = 0;

    memcpy(manager_pipe_name, request + UINT8_T_SIZE, MAX_PIPE_PATH_LEN);

    int manager_fd = open(manager_pipe_name, O_WRONLY);
        if(manager_fd == -1)
            PANIC("failed to open manager pipe");//debug should be warn


    int boxes_counter = 0;
    for(size_t i = 0; i < max_number_boxes; i++) {
        if(box_table[i].empty == 1){
            boxes_counter ++;

            uint64_t box_size = box_table[i].size;
            uint64_t n_publishers = box_table[i].has_publisher;
            uint64_t n_subscribers = box_table[i].n_subscribers;

            if(boxes_counter == actual_number_boxes){
                last = 1;
            }

            memcpy(answer + UINT8_T_SIZE, &last, UINT8_T_SIZE);
            strncpy(answer + UINT8_T_SIZE + UINT8_T_SIZE, box_table[i].box_name, MAX_BOX_NAME_LEN);
            memcpy(answer + UINT8_T_SIZE + UINT8_T_SIZE + MAX_BOX_NAME_LEN, &box_size, UINT64_T_SIZE);
            memcpy(answer + UINT8_T_SIZE + UINT8_T_SIZE + MAX_BOX_NAME_LEN + UINT64_T_SIZE, &n_publishers, UINT64_T_SIZE);
            memcpy(answer + UINT8_T_SIZE + UINT8_T_SIZE + MAX_BOX_NAME_LEN + UINT64_T_SIZE + UINT64_T_SIZE, &n_subscribers, UINT64_T_SIZE);

            if(write(manager_fd, answer, UINT8_T_SIZE + UINT8_T_SIZE + MAX_BOX_NAME_LEN + UINT64_T_SIZE + UINT64_T_SIZE + UINT64_T_SIZE) == -1){
                WARN("error writing in manager pipe");
                return -1;
			}
            
        }
    }

    return 0;
}


int handle_request_general(char *message){
    switch(message[0]) {
        case 1: // session request from publisher
            if(handle_request_1(message))
                WARN("failed to handle request 1 (publisher)")
            break;
        case 2: // session request from subscriber
			if(handle_request_2(message))
                WARN("failed to handle request 2 (subscriber)")
            break;
        case 3: // request from manager to create a box
            if(handle_request_3(message))
                WARN("failed to handle request 3 (manager-create)")
            break;
        case 5: // request from manager to remove a box
            if(handle_request_5(message))
                WARN("failed to handle request 5 (manager-remove)")
            break;
        case 7: // request from manager to list boxes
            if(handle_request_7(message))
                WARN("failed to handle request 7 (manager-list)")
            break;
        default:
            PANIC("invalid message code");
    }
    return 0;
}

void *worker_thread(){
    
    while(1){
        void *request = pcq_dequeue(pc_queue);
        handle_request_general((char*)request); //not receiving what suposed
    }
    
   return NULL;
}



int main(int argc, char **argv) {

   /*-----Store Input-----*/

    if(argc != 3){
        PANIC("invalid comand to inicialize mbroker")
    }

    char* register_pipe_name = argv[1];
    size_t max_sessions;

    if(sscanf(argv[2], "%lu", &max_sessions) != 1) {
            PANIC("invalid comand to inicialize mbroker");
            return -1;
    }

    /*-----Init TFS-----*/

    if(tfs_init(NULL) == -1)
        PANIC("failed to init tfs")

    tfs_params param = tfs_default_params();
    max_number_boxes = param.max_inode_count;


    /*-----Init the box table-----*/

    box_table = (box*)malloc(max_number_boxes * sizeof(box));
    if(box_table == NULL){
        PANIC("failed to alocate memory for the box_table")
    }

    
    for (size_t i = 0; i < max_number_boxes; i++) {
        memset(box_table[i].box_name, 0, MAX_BOX_NAME_LEN);
        box_table[i].has_publisher = 0; //simbolizes empty index
        box_table[i].n_subscribers = 0;
        box_table[i].size = 0;
        box_table[i].empty = 0;
    }

    /*-----Init the producer-consumer queue-----*/
    
    pc_queue = (pc_queue_t*)malloc(sizeof(pc_queue_t));
    if(pc_queue == NULL)
        PANIC("failed to allocate memory for the pc_queue")

    pcq_create(pc_queue, 2 * max_sessions); //inicializes the pcq with twice the max_sessions

    /*-----Init max_sessions of threads-----*/

    pthread_t tid[max_sessions];

    for(size_t i = 0; i < max_sessions; i++){
        pthread_create(&tid[i], NULL, worker_thread, NULL);
    }
    
    /*-----Create FIFO-----*/

    unlink(register_pipe_name);

    if(mkfifo(register_pipe_name, 0660) == -1) {
        PANIC("error creating register_pipe");
        return -1;
    }

    /*-----Read from FIFO-----*/

    int register_pipe_fd_r = open(register_pipe_name, O_RDONLY);
    if(register_pipe_fd_r == -1){
        PANIC("error opening register_pipe");
    }

    

    /*this is a trick so the read never returns 0*/
    int register_pipe_fd_w = open(register_pipe_name, O_WRONLY);
    if(register_pipe_fd_w == -1) {
        PANIC("error opening register_pipe");
        return -1;
    }


    while(1){
        char request[MAX_MESSAGE_SIZE] = {0};
        ssize_t request_size = read(register_pipe_fd_r, request, MAX_MESSAGE_SIZE);
        if(request_size == -1) {
            WARN("error reading from register_pipe");
        } else if (request_size == 0) {
            PANIC("register_pipe closed"); //must not happen
        } else {
            //handle_request_general(request);
            pcq_enqueue(pc_queue, request); // inserts the request in the pcqueue
        }
    }

    /*-----Destroy-----*/

    free(box_table);

    pcq_destroy(pc_queue);
    free(pc_queue);

    return 0;
}
