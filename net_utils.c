#include "net_utils.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

const Response_t RESPONSE_OK = {200, "OK"};
const Response_t RESPONSE_CREATED = {201, "Created"};
const Response_t RESPONSE_BAD_REQUEST = {400, "Bad Request"};
const Response_t RESPONSE_FORBIDDEN = {403, "Forbidden"};
const Response_t RESPONSE_NOT_FOUND = {404, "Not Found"};
const Response_t RESPONSE_INTERNAL_SERVER_ERROR = {500, "Internal Server Error"};
const Response_t RESPONSE_NOT_IMPLEMENTED = {501, "Not Implemented"};

const Request_t REQUEST_GET = {"GET"};
const Request_t REQUEST_PUT = {"PUT"};

int listener_init(Listener_Socket *sock, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) return -1;
    if (listen(fd, 128) < 0) return -1;

    sock->fd = fd;
    return 0;
}

int listener_accept(Listener_Socket *sock) {
    return accept(sock->fd, NULL, NULL);
}

conn_t *conn_new(int fd) {
    conn_t *conn = calloc(1, sizeof(conn_t));
    conn->connfd = fd;
    return conn;
}

void conn_delete(conn_t **conn) {
    if (*conn) {
        free(*conn);
        *conn = NULL;
    }
}

const Response_t *conn_parse(conn_t *conn) {
    char buffer[4096] = {0};
    ssize_t bytes = recv(conn->connfd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) return &RESPONSE_BAD_REQUEST;

    char method[16], uri[256], version[16];
    if (sscanf(buffer, "%s %s %s", method, uri, version) != 3) return &RESPONSE_BAD_REQUEST;
    
    strncpy(conn->method, method, 15);
    strncpy(conn->uri, uri, 255);
    
    char *id_header = strstr(buffer, "Request-Id: ");
    if (id_header) sscanf(id_header + 12, "%s", conn->request_id);

    if (conn->uri[0] == '/') memmove(conn->uri, conn->uri + 1, strlen(conn->uri));
    return NULL; 
}

char *conn_get_uri(conn_t *conn) { return conn->uri; }

char *conn_get_header(conn_t *conn, char *header) {
    if (strcmp(header, "Request-Id") == 0 && strlen(conn->request_id) > 0) return conn->request_id;
    return NULL;
}

const Request_t *conn_get_request(conn_t *conn) {
    if (strcmp(conn->method, "GET") == 0) return &REQUEST_GET;
    if (strcmp(conn->method, "PUT") == 0) return &REQUEST_PUT;
    return NULL;
}

void conn_send_response(conn_t *conn, const Response_t *res) {
    char out[512];
    int len = sprintf(out, "HTTP/1.1 %d %s\r\nContent-Length: %d\r\n\r\n%s\n", 
                      res->code, res->message, (int)strlen(res->message) + 1, res->message);
    send(conn->connfd, out, len, 0);
}

void conn_send_file(conn_t *conn, int fd, uint64_t size) {
    char header[256];
    int len = sprintf(header, "HTTP/1.1 200 OK\r\nContent-Length: %lu\r\n\r\n", size);
    send(conn->connfd, header, len, 0);

    char buffer[8192];
    ssize_t read_bytes;
    while ((read_bytes = read(fd, buffer, sizeof(buffer))) > 0) {
        send(conn->connfd, buffer, read_bytes, 0);
    }
}

const Response_t *conn_recv_file(conn_t *conn, int fd) {
    char buffer[8192];
    ssize_t bytes;
    while ((bytes = recv(conn->connfd, buffer, sizeof(buffer), 0)) > 0) {
        write(fd, buffer, bytes);
    }
    return NULL;
}

const char *request_get_str(const Request_t *req) { return req->method; }
uint16_t response_get_code(const Response_t *res) { return res->code; }
