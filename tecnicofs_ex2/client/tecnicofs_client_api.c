#include "tecnicofs_client_api.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int fserv;
int id;
int fcli;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    unlink(client_pipe_path);
    if (mkfifo (client_pipe_path, 0644) == -1) {
        perror("Mkfifo error");
        return -1;
    }
    if ((fserv = open(server_pipe_path, O_WRONLY)) == -1) {
        perror("Open error");
        return -1;
    }
    
    char opcode = TFS_OP_CODE_MOUNT;
    char buffer[41];
    memcpy(buffer, &opcode, sizeof(char));
    memset(buffer + 1, '\0', sizeof(char) * 40);
    memcpy(buffer + 1, client_pipe_path, sizeof(char) * strlen(client_pipe_path));

    if (write (fserv, buffer, sizeof(buffer)) == -1) {
        perror("Write error");
        return -1;
    }
    fcli = open(client_pipe_path, O_RDONLY);
    if (fcli == -1) {
        perror("Open error");
        return -1;
    }
    
    ssize_t rd = read(fcli, &id, sizeof(int));
    if (rd == -1) {
        perror("Read error");
        return -1;
    }
    if (id == 0) {
        return -1;
    }
    return 0;
}

int tfs_unmount() {
    char buffer[5];
    char opcode = TFS_OP_CODE_UNMOUNT;
    memcpy(buffer, &opcode, sizeof(char));
    memcpy(buffer + 1, &id, sizeof(int));

    if (write (fserv, buffer, sizeof(buffer)) == -1) {
        perror("Write error");
        return -1;
    }
    if (close(fcli) == -1) {
        perror("Closing error");
        return -1;
    }
    if(close(fserv) == -1) {
        perror("Closing error");
        return -1;
    }

    return 0;
}

int tfs_open(char const *name, int flags) {
    char buffer[49];
    char opcode = TFS_OP_CODE_OPEN;

    memcpy(buffer, &opcode, sizeof(char));
    memcpy(buffer +  1, &id, sizeof(int));
    memset(buffer +  5, '\0', sizeof(char) * 40);

    memcpy(buffer +  5, name, sizeof(char) * strlen(name));
    memcpy(buffer + 45, &flags, sizeof(int));

    if (write (fserv, buffer, sizeof(buffer)) == -1) {
        perror("Write error");
        return -1;
    }

    int ret;
    ssize_t rd = read(fcli, &ret, sizeof(int));
    if (rd == -1) {
        perror("Read error");
        return -1;
    }

    return ret;
}

int tfs_close(int fhandle) {
    char buffer[9];
    char opcode = TFS_OP_CODE_CLOSE;

    memcpy(buffer, &opcode, sizeof(char));
    memcpy(buffer + 1, &id, sizeof(int));
    memcpy(buffer + 5, &fhandle, sizeof(int));

    if (write (fserv, buffer, sizeof(buffer)) == -1) {
        perror("Write error");
        return -1;
    }
    
    int ret;
    ssize_t rd = read(fcli, &ret, sizeof(int));
    if (rd == -1) {
        perror("Read error");
        return -1;
    }
    return ret;

}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    char res_buffer[len + 17]; 
    char opcode = TFS_OP_CODE_WRITE;
    memcpy(res_buffer, &opcode, sizeof(char));
    memcpy(res_buffer +  1, &id, sizeof(int));
    memcpy(res_buffer +  5, &fhandle, sizeof(int));
    memcpy(res_buffer +  9, &len, sizeof(size_t));
    memcpy(res_buffer + 17, buffer, sizeof(char) * len);

    if (write (fserv, res_buffer, sizeof(res_buffer)) == -1) {
        perror("Write error");
        return -1;
    }

    ssize_t ret;
    ssize_t rd = read(fcli, &ret, sizeof(ssize_t));
    if (rd == -1) {
        perror("Read error");
        return -1;
    }
    return ret;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    char res_buffer[17];
    char opcode = TFS_OP_CODE_READ;

    memcpy(res_buffer, &opcode, sizeof(char));
    memcpy(res_buffer + 1, &id, sizeof(int));
    memcpy(res_buffer + 5, &fhandle, sizeof(int));
    memcpy(res_buffer + 9, &len, sizeof(size_t));

    if (write (fserv, res_buffer, sizeof(res_buffer)) == -1) {
        perror("Write error");
        return -1;
    }
    ssize_t num;
    ssize_t rd = read(fcli, &num, sizeof(ssize_t));
    if (rd == -1) {
        perror("Read error");
        return -1;
    }
    if (num == -1) {
        return -1;
    }
        
    ssize_t rd2 = read(fcli, buffer, sizeof(char) * (size_t) num);
    if (rd2 == -1) {
        perror("Read error");
        return -1;
    }
    return num;
}

int tfs_shutdown_after_all_closed() {
    char buffer[5];
    char opcode = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;

    memcpy(buffer, &opcode, sizeof(char));
    memcpy(buffer + 1, &id, sizeof(int));
    if (write (fserv, buffer, sizeof(buffer)) == -1) {
        perror("Write error");
        return -1;
    }

    int ret;
    if (read(fcli, &ret, sizeof(int)) == -1) {
        perror("Read error");
        return -1;
    }
    return ret;
}
