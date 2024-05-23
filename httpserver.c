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
#include <regex.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#include "asgn2_helper_funcs.h"
#include "connection.h"
#include "response.h"
#include "request.h"
#include "queue.h"
#include "global.h"
#include "parse.h"


#define BUFFERSIZE 4096

int main(int argc, char *argv[]) {
    int port;
    int localhost;
    int client;
    char grade = '\0';
    Listener_Socket sock;

    //Ensure only 2 arguments are provided
    if (argc > 2 || argc <= 1) {
        fprintf(stderr, "Invalid Port\n");
        exit(1);
    }

    //Checking to make sure the port is a valid integer
    char *port_char = argv[1];
    for (unsigned long i = 0; i < strlen(port_char); i++) {
        if (isdigit(port_char[i]) == 0) {
            fprintf(stderr, "Invalid Port\n");
            exit(1);
        }
    }

    //Checking to see if port is in valid range
    port = atoi(port_char);
    if ((port < 1) || (port > 65535)) { //Check to see if port is valid [1, 65535]
        fprintf(stderr, "Invalid Port\n");
        exit(1);
    }

    //Bind to port
    localhost = listener_init(&sock, port);
    if (localhost != 0) { //Check to see if
        fprintf(stderr, "Invalid Port\n");
        exit(1);
    }

    while (1) {
        //Accept the connection
        client = listener_accept(&sock);

        //Create buff
        char buffer[BUFFERSIZE];

        //Create a request struct
        Request *req = request_create();

        //Create struct for the Status_code
        Status_Code status_code = status_code_create();

        //Read in info from the client
        int bytes_read = read_until(client, buffer, BUFFERSIZE, "\r\n\r\n");
        buffer[bytes_read] = '\0';
        if (bytes_read == -1) {
            get_status(400, &status_code);
            print_status_code(status_code, client, 0);
            request_delete(req);
            close(client);
            continue;
        }

        //CHECKING FOR CASE WHERE THERE IS CONTENT AFTER "\r\n\r\n"
        int count = 0;
        int request_line_end = 0;
        while (count < bytes_read) {
            if (buffer[count] == '\r' && buffer[count + 1] == '\n') {
                if (buffer[count - 2] == '\r' && buffer[count - 1] == '\n') {
                    if (count < bytes_read) {
                        request_line_end = count + 2;
                        break;
                    }
                }
            }
            count++;
        }

        //Making seperate buffer for Message body for PUT request
        char message_body[4096];
        int message_len = 0;
        if (request_line_end != 0) {
            for (int i = 0; i < (bytes_read - request_line_end); i++) {
                message_body[i] = buffer[request_line_end + i];
                message_len++;
            }
        }

        int check_parse = parse_request(buffer, req, request_line_end);

        //Ensuring that the request was valid
        if (check_parse != 0 && check_parse != 201) {
            get_status(check_parse, &status_code);
            print_status_code(status_code, client, 0);
            request_delete(req);
            close(client);
            continue;
        }

        //Checking for valid command
        while (true) {
            if (strcmp(get_request_command(req), "GET") == 0) {
                grade = 'G';
                break;
            } else {
                grade = 'P';
                break;
            }
        }

        switch (grade) {

        case 'G': //COMMAND: "GET"--------------------------------------------------------------------------------
        {
            int infile = open(get_target_path(req), O_RDONLY);

            //If file DNE
            if (infile == -1) {
                get_status(404, &status_code);
                print_status_code(status_code, client, 0);
                request_delete(req);
                close(client);
                continue;
            }

            //Getting the size of the file
            struct stat stbuf;
            stat(get_target_path(req), &stbuf);
            long long file_size = stbuf.st_size;

            get_status(200, &status_code);
            print_status_code(status_code, client, file_size);

            int out_count = 1;
            //Read from infile and print to stdout
            while (out_count != 0) {
                out_count = pass_bytes(infile, client, file_size);

                //Error reading from infile
                if (out_count == -1) {
                    fprintf(stderr, "READING FROM INFILE FAILED");
                    get_status(500, &status_code);
                    print_status_code(status_code, client, 0);
                    request_delete(req);
                    close(infile);
                    close(client);
                    break;
                }

                //Error writing to client
                if (out_count == -2) {
                    fprintf(stderr, "WRITING TO CLIENT FAILED");
                    get_status(500, &status_code);
                    print_status_code(status_code, client, 0);
                    request_delete(req);
                    close(infile);
                    close(client);
                    break;
                }
            }
            if (out_count == -1 || out_count == -2) {
                continue;
            }

            request_delete(req);
            close(infile);
            close(client);
            break;
        }

        case 'P': //COMMAND: "PUT"--------------------------------------------------------------------------------
        {
            //Checking if file EXISTS and has WRITE ACESS
            char *out_file = get_target_path(req);
            int outfile;

            //If file Exists
            if (access(out_file, F_OK | W_OK) == 0) {
                get_status(200, &status_code);
                print_status_code(status_code, client, -10);

                outfile = open(out_file, O_RDWR | O_CREAT | O_TRUNC, 0666);
            } else {
                outfile = open(out_file, O_RDWR | O_CREAT | O_TRUNC, 0666);

                //If outfile could not be created
                if (outfile == -1) {
                    get_status(404, &status_code);
                    print_status_code(status_code, client, 0);
                    request_delete(req);
                    close(client);
                    continue;
                }

                //If outfile was created successfully
                get_status(201, &status_code);
                print_status_code(status_code, client, 0);
            }

            //Write any remaining bytes from message body already read
            int check_write = 0;
            if (request_line_end < bytes_read && request_line_end != 0) {
                if (get_header_value(req) < bytes_read - request_line_end) {
                    check_write = write_all(outfile, message_body, get_header_value(req));
                } else {
                    check_write = write_all(outfile, message_body, bytes_read - request_line_end);
                }
            }
            if (check_write == -1) {
                fprintf(stderr, "WRITING TO OUTFILE FAILED");
                get_status(500, &status_code);
                print_status_code(status_code, client, 0);
                request_delete(req);
                close(outfile);
                close(client);
                break;
            }

            check_write = pass_bytes(client, outfile, get_header_value(req));

            if (check_write == -1 || check_write == -2) {
                fprintf(stderr, "WRITING TO OUTFILE FAILED");
                get_status(500, &status_code);
                print_status_code(status_code, client, 0);
                request_delete(req);
                close(outfile);
                close(client);
                break;
            }

            request_delete(req);
            close(outfile);
            close(client);
            break;
        }
        }
    }
    return 0;
}
