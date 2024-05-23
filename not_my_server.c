#include "asgn2_helper_funcs.h"
#include "connection.h"
#include "response.h"
#include "request.h"
#include "queue.h"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/file.h>

#define INT2VOID(i) (void *) (uintptr_t) (i)
#define VOID2INT(p) (int) (uintptr_t) (p)

void handle_connection(int);

void handle_get(conn_t *);
void handle_put(conn_t *);
void handle_unsupported(conn_t *);

//Making the queue global
static queue_t *que;

//Mutex Locks
pthread_mutex_t auditting;
pthread_mutex_t file_create_mutex;
pthread_mutex_t file_lock_mutex;

//Worker Thread
void *worker() {
    void *connection;
    while (true) {
        queue_pop(que, &connection);
        int new_connection = VOID2INT(connection);
        handle_connection(new_connection);
    }
}

int main(int argc, char **argv) {
    int opt = 0;
    int num_threads = 4;

    //START ---------------------------------------- Command Line Parsing Stuff
    if (argc < 1) {
        warnx("wrong arguments: %s threads %s port_num", argv[0], argv[1]);
        fprintf(stderr, "usage: -t <# of threads>  <port>\n");
        return EXIT_FAILURE;
    }
    if (argc == 3) {
        warnx("wrong arguments: %s threads %s port_num", argv[0], argv[1]);
        fprintf(stderr, "usage: -t <# of threads>  <port>\n");
        return EXIT_FAILURE;
    }
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
        case 't':
            if (!isdigit(*argv[2])) {
                warnx("Invalid Number of threads: %s", argv[2]);
                fprintf(stderr, "usage: -t <# of threads>  <port>\n");
                return EXIT_FAILURE;
            }
            num_threads = atoi(argv[2]);
            break;
        } //END switch
    } //END getopt()

    char *endptr = NULL;
    size_t port;
    if (argc == 2) {
        port = (size_t) strtoull(argv[1], &endptr, 10);
    } else {
        port = (size_t) strtoull(argv[3], &endptr, 10);
    }
    if (endptr && *endptr != '\0') {
        warnx("invalid port number: %s", argv[3]);
        return EXIT_FAILURE;
    }
    //END ------------------------------------------- Command Line Parsing Stuff

    //Create MUTEX Lock
    pthread_mutex_init(&auditting, NULL);
    pthread_mutex_init(&file_create_mutex, NULL);
    pthread_mutex_init(&file_lock_mutex, NULL);

    //Create queue
    que = queue_new(num_threads + 1);

    //Create Worker threads
    pthread_t workers[num_threads];
    for (int i = 0; i < num_threads; i++) {
        int *thread_id = malloc(sizeof(int));
        memcpy(thread_id, &i, sizeof(int));
        pthread_create(&workers[i], NULL, worker, (void *) thread_id);
    }

    signal(SIGPIPE, SIG_IGN);
    Listener_Socket sock;
    listener_init(&sock, port);

    //Dispatcher Thread
    while (true) {
        uintptr_t connfd = listener_accept(&sock);
        queue_push(que, INT2VOID(connfd));
    }

    queue_delete(&que);
    pthread_mutex_destroy(&auditting);
    pthread_mutex_destroy(&file_create_mutex);
    pthread_mutex_destroy(&file_lock_mutex);
    return EXIT_SUCCESS;
}

void audit(const char *req, char *file_name, uint16_t response_num, char *Req_id) {
    if (Req_id == NULL) {
        Req_id = "0";
        fprintf(stderr, "%s,/%s,%d,%s\n", req, file_name, response_num, Req_id);
    }
    fprintf(stderr, "%s,/%s,%d,%s\n", req, file_name, response_num, Req_id);
}

void handle_connection(int connfd) {

    conn_t *conn = conn_new(connfd);
    const Response_t *res = conn_parse(conn);

    if (res != NULL) {
        conn_send_response(conn, res);
    } else {
        const Request_t *req = conn_get_request(conn);
        if (req == &REQUEST_GET) {
            handle_get(conn);
        } else if (req == &REQUEST_PUT) {
            handle_put(conn);
        } else {
            handle_unsupported(conn);
        }
    }

    close(connfd);
    conn_delete(&conn);
}

void handle_get(conn_t *conn) {
    char *uri = conn_get_uri(conn);
    const Response_t *res = NULL;
    const Request_t *request_inp = conn_get_request(conn);
    const char *request_type = request_get_str(request_inp);
    char *Req_id = conn_get_header(conn, "Request-Id");

    //LOCK
    pthread_mutex_lock(&file_lock_mutex);

    //Check if file DNE
    if (access(uri, F_OK) != 0) {
        res = &RESPONSE_NOT_FOUND;
        pthread_mutex_unlock(&file_lock_mutex);
        goto out;
    }

    //Open the file.
    int fd = open(uri, O_RDONLY, 0600);

    if (fd < 0) {
        if (errno == EACCES) {
            res = &RESPONSE_FORBIDDEN;
            goto out;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
            goto out;
        }
    }

    //UNLOCK
    pthread_mutex_unlock(&file_lock_mutex);

    //Lock the file
    flock(fd, LOCK_SH);

    //Get the size of the file.
    struct stat st;
    fstat(fd, &st);
    long file_size = st.st_size;

    //Check if the file is a directory
    if (S_ISDIR(st.st_mode)) {
        res = &RESPONSE_FORBIDDEN;
        goto out;
    }

    //Send the file
    res = &RESPONSE_OK;
    conn_send_file(conn, fd, file_size);

out:
    pthread_mutex_lock(&auditting);
    if (res != &RESPONSE_OK) {
        conn_send_response(conn, res);
    }
    uint16_t response_num = response_get_code(res);
    audit(request_type, uri, response_num, Req_id);
    if (res != &RESPONSE_NOT_FOUND) {
        close(fd);
    }
    pthread_mutex_unlock(&auditting);
}

void handle_unsupported(conn_t *conn) {
    // send responses
    const Response_t *res = &RESPONSE_NOT_IMPLEMENTED;
    char *uri = conn_get_uri(conn);
    const Request_t *request_inp = conn_get_request(conn);
    const char *request_type = request_get_str(request_inp);
    uint16_t response_num = response_get_code(res);
    char *Req_id = conn_get_header(conn, "Request-Id");

    pthread_mutex_lock(&auditting);
    conn_send_response(conn, res);
    audit(request_type, uri, response_num, Req_id);
    pthread_mutex_unlock(&auditting);
}

void handle_put(conn_t *conn) {
    char *uri = conn_get_uri(conn);
    const Response_t *res = NULL;
    const Request_t *request_inp = conn_get_request(conn);
    const char *request_type = request_get_str(request_inp);
    char *Req_id = conn_get_header(conn, "Request-Id");

    //LOCK
    pthread_mutex_lock(&file_create_mutex);
    int fd;

    // Check if file already exists before opening it.
    bool existed = access(uri, F_OK) == 0;

    //Create file if DNE
    if (!existed) {
        fd = open(uri, O_CREAT | O_WRONLY, 0600);
    } else {
        fd = open(uri, O_WRONLY, 0600);
    }

    if (fd < 0) {
        if (errno == EACCES || errno == EISDIR || errno == ENOENT) {
            res = &RESPONSE_FORBIDDEN;
            pthread_mutex_unlock(&file_create_mutex);
            goto out;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
            pthread_mutex_unlock(&file_create_mutex);
            goto out;
        }
    }

    //Lock the file
    flock(fd, LOCK_EX);

    //Truncate
    ftruncate(fd, 0);

    //UNLOCK
    pthread_mutex_unlock(&file_create_mutex);

    res = conn_recv_file(conn, fd);
    if (res == NULL && existed) {
        res = &RESPONSE_OK;
        goto out;
    } else if (res == NULL && !existed) {
        res = &RESPONSE_CREATED;
        goto out;
    }

out:
    pthread_mutex_lock(&auditting);
    conn_send_response(conn, res);
    uint16_t response_num = response_get_code(res);
    audit(request_type, uri, response_num, Req_id);
    close(fd);
    pthread_mutex_unlock(&auditting);
}
