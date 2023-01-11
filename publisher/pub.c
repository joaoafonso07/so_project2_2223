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
    //printf("%d\n", op_code1);//debug

    int pid = getpid();

    printf("pid: %d\n", pid);
    printf("pid size: %ld\n", sizeof(pid)/sizeof(int));

    char *register_pipe_name = argv[1];
    char *original_pipe_name = argv[2];
    char *box_name = argv[3];

    char *new_pipe_name = (char*)malloc(strlen(original_pipe_name) + 8); // 8 is the maximum length of a pid (e.g. "4294967295")


    //printf("original_pipe_name size: %ld\n", strlen(original_pipe_name)); //debug
	// O new_pipe = originnal_pipe_name + pid (8)
    snprintf(new_pipe_name, strlen(original_pipe_name) + 8, "%s%d", original_pipe_name, pid);
    //printf("new_pipe_name = %s\n", new_pipe_name); //debug

    //printf("argv[1] = %s\n", register_pipe_name); //debug
    //printf("argv[2] = %s\n", original_pipe_name); //debug
    //printf("argv[3] = %s\n", box_name); //debug

    int register_fd = open(register_pipe_name, O_WRONLY);
    if(register_fd == -1){
        PANIC("failed to open named pipe");
    }

    /*buffer where we are going to put togheter a request to the mbroker*/
    char request[1 + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN]/* = {0}*/;

    //printf("request 1: %s\n", request); //debug

    memset(request, 0, 1 + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN); //puts all the buffer to '\0'

    //printf("request 2: %s\n", request); //debug

    memcpy(request, &op_code1, sizeof(uint8_t)); //copy the code of the operation (1) to the start of the buffer

    //printf("request 3: %s\n", request); //debug

    strncpy(request + 1, new_pipe_name, MAX_PIPE_PATH_LEN - 1);

    //printf("request 4: %s\n", request); //debug

    strncpy(request + 1 + MAX_PIPE_PATH_LEN, box_name, MAX_BOX_NAME_LEN - 1);

    //printf("request 5: %s\n", request); //debug
    /*
    if(write(1, request, 1 + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN) < 0)
        PANIC("debug");
    */

    if(write(register_fd, request, 1 + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN) < 0)
        PANIC("error writing request to register pipe")

    close(register_fd);

    unlink(new_pipe_name);

    if(mkfifo(new_pipe_name, 0660) == -1) {
        PANIC("error creating pub_pipe");
    }

    int pub_fd = open(new_pipe_name, O_WRONLY);
    if(pub_fd == -1){
        PANIC("failed to open named pipe");
    }

    char message[1 + MAX_MESSAGE_LEN];
    memcpy(message, &op_code9, sizeof(uint8_t));

    for (;;){
        if(fgets(message + 1, MAX_MESSAGE_LEN, stdin) == NULL) //when receives EOF it returns NULL
            break;


        if(write(pub_fd, message, 1 + MAX_MESSAGE_LEN) < 0)
            PANIC("error writing request to  pub_pipe")

    }

    close(pub_fd);
    return 0;
}
