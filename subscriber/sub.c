#include "logging.h"
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>

#define MAX_PIPE_PATH_LEN 256
#define MAX_BOX_NAME_LEN 32
#define MAX_MESSAGE_LEN 1024

static void sig_handler(int sig){
	if (sig == SIGINT) {
		if (signal(SIGINT, sig_handler) == SIG_ERR) {
			exit(EXIT_FAILURE);
		}
		printf("Execution stopped by SIGINT signal\n");
		return;
	}
}

int main(int argc, char **argv) {
	/*
    (void)argc;
    (void)argv;
    fprintf(stderr, "usage: sub <register_pipe_name> <box_name>\n");
    WARN("unimplemented"); // TODO: implement
	*/


    if(argc != 4){
        PANIC("invalid comand creating subscriber")
    }

	uint8_t op_code2 = 2;
	uint8_t op_code10 = 10;


    char* register_pipe_name = argv[1];
    char* pipe_name = argv[2];
	char* box_name = argv[3];


	unlink(pipe_name);
	//Create client pipe
    if(mkfifo(pipe_name, 0660) == -1) {
        PANIC("error creating pipe_name");
        return -1;
    }

	//Open client pipe
	int register_fd = open(register_pipe_name, O_WRONLY);
    if(register_fd == -1){
        WARN("failed to open named pipe: %s", strerror(errno));
        return -1;
    }


	//afonso: TODO: enviar o pedido de inicio de sessão ao mbroker

    /*buffer where we are going to put togheter a request to the mbroker*/
    char request[1 + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN] = {0};

	memcpy(request, &op_code2, sizeof(uint8_t)); //copy the code of the operation (1) to the start of the buffer

    strncpy(request + 1, pipe_name, MAX_PIPE_PATH_LEN - 1);

    strncpy(request + 1 + MAX_PIPE_PATH_LEN, box_name, MAX_BOX_NAME_LEN - 1);

    if(write(register_fd, request, 1 + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN) < 0)
        PANIC("error writing request to register pipe")

	close(register_fd);


	//Open server pipe
	int sub_fd = open(pipe_name, O_RDONLY);
    if(sub_fd == -1){
        WARN("failed to open named pipe: %s", strerror(errno));
        return -1;
    }

	char message[1 + MAX_MESSAGE_LEN];
	int mCounter = 0;
	memcpy(message, &op_code10, sizeof(uint8_t));

	//SIGNALS
  	if (signal(SIGINT, sig_handler) == SIG_ERR) {
    	exit(EXIT_FAILURE);
 	 }

	//Wait for new messages
	for(;;){
		if(read(register_fd, message, 1+ MAX_MESSAGE_LEN) > 0){
			mCounter++;
			printf("%s\n", message);
		}
		if(write(sub_fd, message, 1+ MAX_MESSAGE_LEN) < 0){
			WARN("failed to write: %s", strerror(errno));
		}
	}

	printf("Number of messages: %d\n", mCounter);
	close(sub_fd);


    return -1;
}
