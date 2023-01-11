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

static void print_usage() {
    fprintf(stderr, "usage: \n"
                    "   manager <register_pipe_name> create <box_name>\n"
                    "   manager <register_pipe_name> remove <box_name>\n"
                    "   manager <register_pipe_name> list\n");
}

void send_request(int register_fd, uint8_t code){
    //TODO: implement
}

int main(int argc, char **argv) {
	/*
    (void)argc;
    (void)argv;
    print_usage();
    WARN("unimplemented"); // TODO: implement
	*/

   if(argc != 4 || argc != 5)
    PANIC("invalid comand to launch a manager")

    char *register_pipe_name = argv[1];
    char *original_pipe_name = argv[2];
    char *mode = argv[3];

    int pid = getpid();

    char *new_pipe_name = (char*)malloc(strlen(original_pipe_name) + 8); // 8 is the maximum length of a pid (e.g. "4294967295")

    snprintf(new_pipe_name, strlen(original_pipe_name) + 8, "%s%d", original_pipe_name, pid);

    unlink(new_pipe_name);

    if(mkfifo(new_pipe_name, 0660) == -1)
        PANIC("error creating manager_pipe");

    int register_fd = open(register_pipe_name, O_WRONLY);
    if(register_fd == -1)
        PANIC("failed to open register pipe");


    if(strcmp(mode, "create") == 0){
        char* box_name = argv[4];
        uint8_t opcode3 = 3;
        send_request(register_fd, opcode3);
    }
    else if(strcmp(mode, "remove") == 0){
        char* box_name = argv[4];
        //TODO: implement
    }
    else if(strcmp(mode, "list") == 0){
        //TODO: implement
    }
    else{
        PANIC("invalid comand to inicialize manager: mode")
    }



    return 0;
}

