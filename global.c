#include "global.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#define BUFFER_SIZE

//START REQUEST STRUCT------------------------------------------------------------------------------------------------

//Constructor function that creates a request
//First Parameter: Place recieving input from
//Second Parameter: Command
//Third Parameter: File path
//Fourth Parameter: Length of content
Request *request_create() {
    Request *request = (Request *) malloc(sizeof(Request));
    if (request != NULL) {
        request->in_fd = 0;
        request->command = NULL;
        request->target_path = NULL;
        request->header_key = NULL;
        request->header_value = 0;
        request->header_end = 0;
    }
    return request;
}

//Accessor function that returns in_fd of the current Request
////First parameter: Pointer to a Request struct
int get_in_fd(Request *r) {
    return (r->in_fd);
}

//Accessor function that returns command of the current Request
////First parameter: Pointer to a Request struct
char *get_request_command(Request *r) {
    return (r->command);
}

//Accessor function that returns target_path of the current Request
////First parameter: Pointer to a Request struct
char *get_target_path(Request *r) {
    return (r->target_path);
}

//Accessor function that returns header_key of the current Request
////First parameter: Pointer to a Request struct
char *get_header_key(Request *r) {
    return (r->header_key);
}

//Accessor function that returns header_value of the current Request
////First parameter: Pointer to a Request struct
int get_header_value(Request *r) {
    return (r->header_value);
}

//Accessor function that returns header_end of the current Request
//Returns the end of the header
int get_header_end(Request *r) {
    return (r->header_end);
}

//Destructor function that frees any memory allocated for the Request
//First parameter: Pointer to the Request struct
void request_delete(Request *r) {
    free(r);
    return;
}

//END REQUEST STRUCT------------------------------------------------------------------------------------------------

//START RESPONSE STRUCT------------------------------------------------------------------------------------------------

//Prints the Response struct
void print_response(Response response, int outfile) {
    dprintf(outfile, "Version: %s\n", response.version);
    dprintf(outfile, "Status Code: %d\n", response.status_code);
    dprintf(outfile, "Status Phrase: %s\n", response.status_phrase);
    dprintf(outfile, "Header: %s: %d\n", response.header_key, response.header_value);
}

//ENDS RESPONSE STRUCT------------------------------------------------------------------------------------------------

//START STATUS CODE STRUCT------------------------------------------------------------------------------------------------

//Creates the Status-Code struct
Status_Code status_code_create() {
    Status_Code sc;
    sc.status_code = 0;
    sc.status_phrase = "\0";
    sc.message = "\0";
    return sc;
}

//Applies the proper code/phrase to Status_Code struct when given a status code
void get_status(int code, Status_Code *stat) {
    switch (code) {

    case 200: //When a method is Successful
        stat->status_code = 200;
        stat->status_phrase = "OK";
        stat->message = "OK\n";
        break;

    case 201: //When a URI’s file is created
        stat->status_code = 201;
        stat->status_phrase = "Created";
        stat->message = "Created\n";
        break;

    case 400: //When a request is ill-formatted
        stat->status_code = 400;
        stat->status_phrase = "Bad Request";
        stat->message = "Bad Request\n";
        break;

    case 403: //When a URI’s file is not accessible
        stat->status_code = 403;
        stat->status_phrase = "Forbidden";
        stat->message = "Forbidden\n";
        break;

    case 404: //When the URI’s file does not exist
        stat->status_code = 404;
        stat->status_phrase = "Not Found";
        stat->message = "Not Found\n";
        break;

    case 500: //When an unexpected issue prevents processing
        stat->status_code = 500;
        stat->status_phrase = "Internal Server Error";
        stat->message = "Internal Server Error\n";
        break;

    case 501: //When a request includes an unimplemented Method
        stat->status_code = 501;
        stat->status_phrase = "Not Implemented";
        stat->message = "Not Implemented\n";
        break;

    case 505: //When a request includes an unsupported version
        stat->status_code = 505;
        stat->status_phrase = "Version Not Supported";
        stat->message = "Version Not Supported\n";
        break;

    default: return;
    }
    return;
}

//Prints the Status-code and Status-phrase
void print_status_code(Status_Code sc, int outfile, int bytes_written) {
    //If doing GET
    if (sc.status_code == 200
        && bytes_written == -10) { //If command is 'PUT' file has been overwritten
        dprintf(outfile, "HTTP/1.1 %d ", sc.status_code);
        dprintf(outfile, "%s\r\n", sc.status_phrase);
        dprintf(outfile, "Content-Length: %lu\r\n\r\n", strlen(sc.message));
        dprintf(outfile, "%s", sc.message);
    } else if (sc.status_code == 200) { //If request is succesful and does not change/create a file
        dprintf(outfile, "HTTP/1.1 %d ", sc.status_code);
        dprintf(outfile, "%s\r\n", sc.status_phrase);
        dprintf(outfile, "Content-Length: %d\r\n\r\n", bytes_written);
    } else if (sc.status_code == 201) { //If command is 'PUT' file has been created
        dprintf(outfile, "HTTP/1.1 %d ", sc.status_code);
        dprintf(outfile, "%s\r\n", sc.status_phrase);
        dprintf(outfile, "Content-Length: %lu\r\n\r\n", strlen(sc.message));
        dprintf(outfile, "%s", sc.message);
    } else { //Other cases
        dprintf(outfile, "HTTP/1.1 %d ", sc.status_code);
        dprintf(outfile, "%s\r\n", sc.status_phrase);
        dprintf(outfile, "Content-Length: %lu\r\n\r\n", strlen(sc.message));
        dprintf(outfile, "%s", sc.message);
    }
}

//END STATUS CODE STRUCT------------------------------------------------------------------------------------------------
