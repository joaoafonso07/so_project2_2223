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

int main(int argc, char **argv) {
    /*
    (void)argc;
    (void)argv;
    fprintf(stderr, "usage: pub <register_pipe_name> <box_name>\n");
    WARN("unimplemented"); // TODO: implement
    */
    if(argc != 4){
        PANIC("invalid comand to launch a publisher");
        return -1;
    }

    uint8_t op_code = 1;
    printf("%d\n", op_code);

    char *register_pipe_name = argv[1];
    char *pipe_name = argv[2];
    char *box_name = argv[3];

    printf("argv[1] = %s\n", register_pipe_name);
    printf("argv[2] = %s\n", pipe_name);
    printf("argv[3] = %s\n", box_name);

    int tx = open(register_pipe_name, O_WRONLY);
    if(tx == -1){
        PANIC("failed to open named pipe");
        return -1;
    }

    /*buffer where we are going to put togheter a request to the mbroker*/
    char request[1 + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN];

    printf("request 1: %s\n", request); //debug

    memset(request, 0, 1 + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN); //puts all the buffer to '\0'

    printf("request 2: %s\n", request); //debug

    memcpy(request, &op_code, sizeof(uint8_t)); //copy the code of the operation (1) to the start of the buffer

    printf("request 3: %s\n", request); //debug

    strncpy(request + 1, pipe_name, MAX_PIPE_PATH_LEN - 1);

    printf("request 4: %s\n", request); //debug

    strncpy(request + 1 + MAX_PIPE_PATH_LEN, box_name, MAX_BOX_NAME_LEN - 1);

    printf("request 5: %s\n", request); //debug

    if(write(tx, request, 1 + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN) < 0){
        PANIC("error writing request to register pipe")
        return -1;
    }


    printf("yaaa\n");

    close(tx);



    return 0;
}
