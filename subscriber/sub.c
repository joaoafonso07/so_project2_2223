#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "logging.h"

#define MAX_PIPE_PATH_LEN 256
#define MAX_BOX_NAME_LEN 32
#define MAX_MESSAGE_LEN 1024

int m_counter = 0;

static void sig_handler(int sig) {
    if (sig == SIGINT) {
        if (signal(SIGINT, sig_handler) == SIG_ERR) {
            exit(EXIT_FAILURE);
        }
		char counter_str[64];
		snprintf(counter_str, 64, "%d\n", m_counter);
		if (write(1, counter_str, strlen(counter_str)) < 0){
			PANIC("error write counter");
		}
        printf("Execution stopped by SIGINT signal\n");
        return;
    }
}

int main(int argc, char** argv) {
    /*
(void)argc;
(void)argv;
fprintf(stderr, "usage: sub <register_pipe_name> <box_name>\n");
WARN("unimplemented"); // TODO: implement
    */

    if (argc != 4) {
        PANIC("invalid comand creating subscriber")
    }
    // SIGNALS
    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        exit(EXIT_FAILURE);
    }

    uint8_t op_code2 = 2;

    char* register_pipe_name = argv[1];
    char* pipe_name = argv[2];
    char* box_name = argv[3];

    unlink(pipe_name);
    // Create client pipe
    if (mkfifo(pipe_name, 0660) == -1)
        PANIC("error creating pipe_name")

    // Open client pipe
    int register_fd = open(register_pipe_name, O_WRONLY);
    if (register_fd == -1)
        PANIC("failed to open register pipe");

    // afonso: TODO: enviar o pedido de inicio de sessão ao mbroker

    /*buffer where we are going to put togheter a request to the mbroker*/
    char request[1 + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN] = {0};

    memcpy(request, &op_code2, sizeof(uint8_t));  // copy the code of the operation (1) to the start of the buffer

    strncpy(request + 1, pipe_name, MAX_PIPE_PATH_LEN - 1);

    strncpy(request + 1 + MAX_PIPE_PATH_LEN, box_name, MAX_BOX_NAME_LEN - 1);

    if (write(register_fd, request, 1 + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN) < 0)
        PANIC("error writing request to register pipe")

    close(register_fd);

    // Open subscriber pipe
    int sub_fd = open(pipe_name, O_RDONLY);
    if (sub_fd == -1) {
        WARN("failed to open sub pipe: %s", strerror(errno));
        return -1;
    }

    char message[1 + MAX_MESSAGE_LEN];

    //memcpy(message, &op_code10, sizeof(uint8_t));

    // Wait for new messages
    for (;;) {
        ssize_t message_size = read(sub_fd, message, 1 + MAX_MESSAGE_LEN);
        if(message_size == -1) {
            PANIC("error reading from sub_pipe");
        } else if (message_size == 0) {
            PANIC("sub_pipe closed");
            break;
		} else{
        	m_counter++;
        	fprintf(stdout, "%s\n", message+1);
		}
    }

    fprintf(stdout, "%d\n", m_counter);
    close(sub_fd);

    return 0;
}
