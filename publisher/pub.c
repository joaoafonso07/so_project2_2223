#include "logging.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_PIPE_PATH_LEN 256
#define MAX_BOX_NAME_LEN 32
#define MAX_MESSAGE_LEN 1024


int main(int argc, char **argv) {
    /*
    (void)argc;
    (void)argv;
    fprintf(stderr, "usage: pub <register_pipe_name> <box_name>\n");
    WARN("unimplemented"); // TODO: implement
    */
    if(argc != 4){
        PANIC("invalid comand to launch a publisher");
    }

    uint8_t op_code1 = 1;
    uint8_t op_code9 = 9;
    int pid = getpid();


    char *register_pipe_name = argv[1];
    char *original_pipe_name = argv[2];
    char *box_name = argv[3];

    char *new_pipe_name = (char*)malloc(strlen(original_pipe_name) + 8); // 8 is the maximum length of a pid (e.g. "4294967295")


	// O new_pipe = originnal_pipe_name + pid (8)
    snprintf(new_pipe_name, strlen(original_pipe_name) + 8, "%s%d", original_pipe_name, pid);
  

    unlink(new_pipe_name);

    if(mkfifo(new_pipe_name, 0660) == -1) {
        PANIC("error creating pub_pipe");
    }

    int register_fd = open(register_pipe_name, O_WRONLY);
    if(register_fd == -1){
        PANIC("failed to open register pipe");
    }

    /*buffer where we are going to put togheter a request to the mbroker*/
    char request[1 + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN] = {0};



    memcpy(request, &op_code1, sizeof(uint8_t)); //copy the code of the operation (1) to the start of the buffer



    strncpy(request + 1, new_pipe_name, MAX_PIPE_PATH_LEN - 1);



    strncpy(request + 1 + MAX_PIPE_PATH_LEN, box_name, MAX_BOX_NAME_LEN - 1);



    /*
    if(write(1, request, 1 + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN) < 0)
        PANIC("debug");
    */

    if(write(register_fd, request, 1 + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN) < 0)
        PANIC("error writing request to register pipe")

    close(register_fd);


    int pub_fd = open(new_pipe_name, O_WRONLY);
    if(pub_fd == -1){
        PANIC("failed to open named pipe");
    }

    char message[1 + MAX_MESSAGE_LEN] = {0}; //inicializes the bufer to '\0'
    memcpy(message, &op_code9, sizeof(uint8_t));

    for (;;){
        if(fgets(message + 1, MAX_MESSAGE_LEN, stdin) == NULL) //when receives EOF it returns NULL
            break;

		message[strlen(message) - 1] = 0;
        if(write(pub_fd, message, 1 + MAX_MESSAGE_LEN) < 0)
            PANIC("error writing request to  pub_pipe")

    }

    close(pub_fd);
    free(new_pipe_name);

    return 0;
}
