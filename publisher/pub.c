#include "logging.h"
#include <errno.h>
#include <fcntl.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    fprintf(stderr, "usage: pub <register_pipe_name> <box_name>\n");
    WARN("unimplemented"); // TODO: implement

    int tx = open(argv[1], O_WRONLY);
    if(tx == -1){
        WARN("failed to open named pipe: %s", strerror(errno));
        return -1;
    }

    return -1;
}
