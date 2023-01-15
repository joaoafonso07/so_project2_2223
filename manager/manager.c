#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../fs/config.h"
#include "../fs/operations.h"
#include "../fs/state.h"
#include "logging.h"

#define MAX_PIPE_PATH_LEN 256
#define MAX_BOX_NAME_LEN 32
#define MAX_MESSAGE_LEN 1024
#define UINT8_T_SIZE 1
#define INT32_T_SIZE 4
#define UINT64_T_SIZE 8

/*
static void print_usage() {
    fprintf(stderr, "usage: \n"
                    "   manager <register_pipe_name> create <box_name>\n"
                    "   manager <register_pipe_name> remove <box_name>\n"
                    "   manager <register_pipe_name> list\n");
}
*/

static size_t max_number_boxes = 64;

typedef struct {
    char box_name[MAX_BOX_NAME_LEN];
    uint64_t box_size;
    uint64_t n_publisheres;
    uint64_t n_subscribers;
} box;

int compare_buffer(const void* a, const void* b) {
    box* box1 = (box*)a;
    box* box2 = (box*)b;
    return strcmp(box1->box_name, box2->box_name);
}

int main(int argc, char** argv) {
    /*
(void)argc;
(void)argv;
print_usage();
WARN("unimplemented"); // TODO: implement
    */

    if (argc != 4 && argc != 5) {
        PANIC("invalid comand to launch a manager")
    }

    char* register_pipe_name = argv[1];
    char* original_pipe_name = argv[2];
    char* mode = argv[3];

    int pid = getpid();

    int answer_type = 0;

    char* new_pipe_name = (char*)malloc(strlen(original_pipe_name) + 8);  // 8 is the maximum length of a pid (e.g. "4294967295")

    snprintf(new_pipe_name, strlen(original_pipe_name) + 8, "%s%d", original_pipe_name, pid);

    unlink(new_pipe_name);

    if (mkfifo(new_pipe_name, 0660) == -1)
        PANIC("error creating manager_pipe");

    int register_fd = open(register_pipe_name, O_WRONLY);
    if (register_fd == -1)
        PANIC("failed to open register pipe");

    // send request depending on the argue
    if (strcmp(mode, "create") == 0) {
        char* box_name = argv[4];
        char request[UINT8_T_SIZE + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN] = {0};

        request[0] = 3;  // op_code
        strncpy(request + UINT8_T_SIZE, new_pipe_name, MAX_PIPE_PATH_LEN - 1);
        strncpy(request + UINT8_T_SIZE + MAX_PIPE_PATH_LEN, box_name, MAX_BOX_NAME_LEN - 1);

        if (write(register_fd, request, UINT8_T_SIZE + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN) < 0)
            PANIC("error writing request to register pipe")
    } else if (strcmp(mode, "remove") == 0) {
        char* box_name = argv[4];
        char request[UINT8_T_SIZE + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN] = {0};

        request[0] = 5;  // op_code
        strncpy(request + UINT8_T_SIZE, new_pipe_name, MAX_PIPE_PATH_LEN - 1);
        strncpy(request + UINT8_T_SIZE + MAX_PIPE_PATH_LEN, box_name, MAX_BOX_NAME_LEN - 1);

        if (write(register_fd, request, UINT8_T_SIZE + MAX_PIPE_PATH_LEN + MAX_BOX_NAME_LEN) < 0)
            PANIC("error writing request to register pipe")
    } else if (strcmp(mode, "list") == 0) {
        char request[UINT8_T_SIZE + MAX_PIPE_PATH_LEN] = {0};

        request[0] = 7;  // op_code
        strncpy(request + UINT8_T_SIZE, new_pipe_name, MAX_PIPE_PATH_LEN - 1);

        if (write(register_fd, request, UINT8_T_SIZE + MAX_PIPE_PATH_LEN) < 0)
            PANIC("error writing request to register pipe")

        answer_type = 1;
    } else {
        PANIC("invalid comand to inicialize manager: mode")
    }

    close(register_fd);

    int manager_fd = open(new_pipe_name, O_RDONLY);
    if (manager_fd == -1)
        PANIC("failed to open manager pipe");

    uint8_t answer_op_code;

    if (answer_type == 0) {  // request was for remove/create box
        char answer[UINT8_T_SIZE + INT32_T_SIZE + MAX_MESSAGE_LEN];
        int32_t return_code;

        if (read(manager_fd, answer, UINT8_T_SIZE + INT32_T_SIZE + MAX_MESSAGE_LEN) < 0)
            PANIC("error reading from manager_pipe")

        memcpy(&answer_op_code, answer, UINT8_T_SIZE);

        if (answer_op_code != 4 && answer_op_code != 6)
            PANIC("manager : invalid answer")

        memcpy(&return_code, answer + UINT8_T_SIZE, INT32_T_SIZE);

        if (return_code == 0)
            fprintf(stdout, "OK\n");  // success
        else if (return_code == -1) {
            char error_message[MAX_MESSAGE_LEN];
            memcpy(error_message, answer + UINT8_T_SIZE + INT32_T_SIZE, MAX_MESSAGE_LEN);
            fprintf(stdout, "ERROR %s\n", error_message);
        } else {
            PANIC("manager : invalid answer")
        }
    } else if (answer_type == 1) {  // request was for list boxes
        char answer[UINT8_T_SIZE + UINT8_T_SIZE + MAX_BOX_NAME_LEN + UINT64_T_SIZE + UINT64_T_SIZE + UINT64_T_SIZE];
        uint8_t last;
        char box_name[MAX_BOX_NAME_LEN];
        uint64_t box_size, n_publishers, n_subscribers;
        // for sorting global
        box boxes[max_number_boxes];
        box b;
        int actual_box_number = 0;
        // for sorting (aux functions)
        int i, j;
        char box_min[MAX_BOX_NAME_LEN];
        int j_min;
        box aux;
        box boxes_copy[max_number_boxes];

        while (1) {
            ssize_t message_size = read(manager_fd, answer, sizeof(answer));
            if (message_size == -1) {
                PANIC("error reading from manager_pipe");
            } else if (message_size == 0) {
                WARN("manager_pipe closed");
                break;
            } else {
                memcpy(&answer_op_code, answer, UINT8_T_SIZE);

                if (answer_op_code != 8)
                    PANIC("manager : invalid answer")

                memcpy(&last, answer + UINT8_T_SIZE, UINT8_T_SIZE);
                memcpy(box_name, answer + UINT8_T_SIZE + UINT8_T_SIZE, MAX_BOX_NAME_LEN);
                memcpy(&box_size, answer + UINT8_T_SIZE + UINT8_T_SIZE + MAX_BOX_NAME_LEN, UINT64_T_SIZE);
                memcpy(&n_publishers, answer + UINT8_T_SIZE + UINT8_T_SIZE + MAX_BOX_NAME_LEN + UINT64_T_SIZE, UINT64_T_SIZE);
                memcpy(&n_subscribers, answer + UINT8_T_SIZE + UINT8_T_SIZE + MAX_BOX_NAME_LEN + UINT64_T_SIZE + UINT64_T_SIZE, UINT64_T_SIZE);

                strcpy(b.box_name, box_name);
                b.box_size = box_size;
                b.n_publisheres = n_publishers;
                b.n_subscribers = n_subscribers;
                boxes[actual_box_number] = b;
                actual_box_number++;

                if (last == 1) {
                    break;
                }
                memset(answer, 0, UINT8_T_SIZE + UINT8_T_SIZE + MAX_BOX_NAME_LEN + UINT64_T_SIZE + UINT64_T_SIZE + UINT64_T_SIZE);
            }
        }

        // Sort boxes

        for (i = 0; i < actual_box_number; i++) {
            boxes_copy[i] = boxes[i];
        }
        // print boxes
        for (i = 0; i < actual_box_number; i++) {
            strcpy(box_min, boxes_copy[i].box_name);
            j_min = i;

            for (j = i; j < actual_box_number; j++) {
                if (strcmp(boxes_copy[j].box_name, box_min) < 0) {
                    strcpy(box_min, boxes_copy[j].box_name);
                    j_min = j;
                }
            }

            aux = boxes_copy[i];
            boxes_copy[i] = boxes_copy[j_min];
            boxes_copy[j_min] = aux;

            fprintf(stdout, "%s %zu %zu %zu\n", boxes_copy[i].box_name, boxes_copy[i].box_size, boxes_copy[i].n_publisheres, boxes_copy[i].n_subscribers);
        }
    }

    close(manager_fd);
    free(new_pipe_name);

    return 0;
}
