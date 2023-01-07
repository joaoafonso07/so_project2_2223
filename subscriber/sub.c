#include "logging.h"
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

#define BUFFER_SUB_MAXSIZE 1024

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
	char buffer[BUFFER_SUB_MAXSIZE];
	int mCounter = 0;

    if(argc != 4){
        PANIC("invalid comand creating subscriber")
    }
    char* register_pipe_name = argv[1];
    char* pipe_name = argv[2];
	//char* box_name = argv[3];


	//Create client pipe
    if(mkfifo(register_pipe_name, 0660) == -1) {
        PANIC("error creating register_pipe");
        return -1;
    }

	//Open client pipe
	int pclient = open(register_pipe_name, O_WRONLY);
    if(pclient == -1){
        WARN("failed to open named pipe: %s", strerror(errno));
        return -1;
    }
	//Open server pipe
	int pserver = open(pipe_name, O_RDONLY);
    if(pserver == -1){
        WARN("failed to open named pipe: %s", strerror(errno));
        return -1;
    }

	//TODO: Buscar mensagens todas do box_name para ele poder ler (acho que e preciso o pub estar feito)

	//SIGNALS
  	if (signal(SIGINT, sig_handler) == SIG_ERR) {
    	exit(EXIT_FAILURE);
 	 }

	//Wait for new messages
	for(;;){
		if(read(pclient, buffer, BUFFER_SUB_MAXSIZE) > 0){
			mCounter++;
			printf("%s\n", buffer);
		}
		if(write(pserver, buffer, BUFFER_SUB_MAXSIZE) == -1){
			WARN("failed to write: %s", strerror(errno));
		}
	}

	printf("Number of messages: %d\n", mCounter);
	close(pserver);
	close(pclient);

    return -1;
}
