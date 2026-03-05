#pragma once

#include <stdint.h>
#include <sys/types.h>


typedef struct {
    int fd;
} Listener_Socket;

int listener_init(Listener_Socket *sock, int port);

int listener_accept(Listener_Socket *sock);

ssize_t read_until(int in, char buf[], size_t nbytes, char *string);

ssize_t write_all(int out, char buf[], size_t nbytes);

ssize_t pass_bytes(int src, int dst, size_t nbytes);
