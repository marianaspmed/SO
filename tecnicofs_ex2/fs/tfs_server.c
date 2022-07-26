#include "operations.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int fserv, fcli;
char buffer[BLOCK_SIZE];
int id;
//int sessions[S];

int s_mount(char tfs_op_code);
int s_unmount();
int s_open(char tfs_op_code);
int s_close(char tfs_op_code);
int s_write(char tfs_op_code);
int s_read(char tfs_op_code);
int s_shutdown();

int main(int argc, char **argv) {
    char opcode;

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    tfs_init();

    unlink(pipename);

    if (mkfifo(pipename, 0644) == -1) {
        perror("Mkfifo error");
        return 1;
    }
    if ((fserv = open(pipename, O_RDONLY)) == -1) {
        perror("Open error");
        return -1;
    }

    while (true) {
        memcpy(&opcode, buffer, sizeof(char));
        ssize_t rd = read(fserv, &opcode, sizeof(opcode));
        if (rd == -1) {
            perror("Read error");
        }
        if (rd == 0) {
            fserv = open(pipename, O_RDONLY);
            continue;
        }

        if (opcode == TFS_OP_CODE_MOUNT) {
            s_mount(opcode);
        } else if (opcode == TFS_OP_CODE_UNMOUNT) {
            s_unmount();
        } else if (opcode == TFS_OP_CODE_OPEN) {
            s_open(opcode);
        } else if (opcode == TFS_OP_CODE_CLOSE) {
            s_close(opcode);
        } else if (opcode == TFS_OP_CODE_WRITE) {
            s_write(opcode);
        } else if (opcode == TFS_OP_CODE_READ) {
            s_read(opcode);
        } else if (opcode == TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED) {
            s_shutdown();
        }
    }

    close(fserv);
    unlink(pipename);
    return 0;
}

int s_mount(char opcode) {
    char client_pipe_path[40];
    id = 1;
    memcpy(buffer, &opcode, sizeof(char));

    if (read(fserv, buffer + 1, sizeof(char) * 40) == -1) {
        perror("Read error");
        return -1;
    }
    memcpy(client_pipe_path, buffer + 1, sizeof(client_pipe_path));

    if ((fcli = open(client_pipe_path, O_WRONLY)) == -1) {
        perror("Open error");
        return -1;
    }
    if (write(fcli, &id, sizeof(int)) == -1) {
        perror("Write error");
        return -1;
    }

    return 0;
}

int s_unmount() {
    int session_id;
    if (read(fserv, &session_id, sizeof(int)) == -1) {
        perror("Read error");
        return -1;
    }
    if (id == 0) {
        return -1;
    }

    if (close(fcli) == -1) {
        perror("Closing error");
        return -1;
    }

    return 0;
}

int s_open(char opcode) {
    int session_id, flags;
    char name[40];
    if (read(fserv, &session_id, sizeof(int)) == -1) {
        perror("Read error");
        return -1;
    }
    if (id == 0) {
        return -1;
    }
    memcpy(buffer, &opcode, sizeof(char));
    memcpy(buffer + 1, &session_id, sizeof(int));

    if (read(fserv, buffer + 5, sizeof(char) * 44) == -1) {
        perror("Read error");
        return -1;
    }
    memcpy(name, buffer + 5, sizeof(name));
    memcpy(&flags, buffer + 45, sizeof(int));

    int ret = tfs_open(name, flags);
    if (write(fcli, &ret, sizeof(int)) == -1) {
        perror("Write error");
        return -1;
    }

    return 0;
}

int s_close(char opcode) {
    int session_id;
    if (read(fserv, &session_id, sizeof(int)) == -1) {
        perror("Read error");
        return -1;
    }
    if (id == 0) {
        return -1;
    }
    memcpy(buffer, &opcode, sizeof(char));
    memcpy(buffer + 1, &session_id, sizeof(int));

    if (read(fserv, buffer + 5, sizeof(char) * 4) == -1) {
        perror("Read error");
        return -1;
    }
    int fh, ret;
    memcpy(&fh, buffer + 5, sizeof(int));
    ret = tfs_close(fh);

    if (write(fcli, &ret, sizeof(int)) == -1) {
        perror("Write error");
        return -1;
    }
    printf("Closed \n");
    return 0;
}

int s_write(char opcode) {
    int session_id, fh;

    if (read(fserv, &session_id, sizeof(int)) == -1) {
        perror("Read error");
        return -1;
    }
    if (id == 0) {
        return -1;
    }
    memcpy(buffer, &opcode, sizeof(char));
    memcpy(buffer + 1, &session_id, sizeof(int));

    if (read(fserv, buffer + 5, sizeof(int)) == -1) {
        perror("Read error");
        return -1;
    }
    memcpy(&fh, buffer + 5, sizeof(int));
    
    size_t len;
    if (read(fserv, &len, sizeof(size_t)) == -1) {
        perror("Read error");
        return -1;
    }
    memcpy(buffer + 9, &len, sizeof(size_t));

    if (read(fserv, buffer + 17, sizeof(char) * len) == -1) {
        perror("Read error");
        return -1;
    }
    char to_write[len];
    memcpy(to_write, buffer + 17, sizeof(to_write));

    ssize_t wt = tfs_write(fh, to_write, len);
    if (write(fcli, &wt, sizeof(ssize_t)) == -1) {
        perror("Write error");
        return -1;
    }

    return 0;
}

int s_read(char opcode) {
    int session_id, fh;

    if (read(fserv, &session_id, sizeof(int)) == -1) {
        perror("Read error");
        return -1;
    }
    if (id == 0) {
        return -1;
    }
    memcpy(buffer, &opcode, sizeof(char));
    memcpy(buffer + 1, &session_id, sizeof(int));
    
    if (read(fserv, buffer + 5, sizeof(char) * 12) == -1) {
        perror("Read error");
        return -1;
    }
    size_t len;
    memcpy(&fh, buffer + 5, sizeof(int));
    memcpy(&len, buffer + 9, sizeof(size_t));

    char to_read[len];
    ssize_t rd = tfs_read(fh, to_read, len);
    if (write(fcli, &rd, sizeof(ssize_t)) == -1) {
        perror("Write error");
        return -1;
    }
    if (rd != -1) {
        if (write(fcli, to_read, sizeof(char) * (size_t)rd) == -1) {
            perror("Write error");
            return -1;
        }
    }

    return 0;
}

int s_shutdown() {
    return 0;
}
