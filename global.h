#pragma once

#include <stdio.h>

typedef struct Request Request;

//START REQUEST STRUCT-------------------------------------------------------------------------------------------------

struct Request {
    int in_fd;
    char *command;
    char *target_path;
    char *header_key;
    long long unsigned header_value;
    int header_end;
};

//Creates the Request struct
Request *request_create();

//Returns the infile
int get_in_fd(Request *r);

//Returns the request command
char *get_request_command(Request *r);

//Returns the target path
char *get_target_path(Request *r);

//Returns the key of the header
char *get_header_key(Request *r);

//Returns the value of the header
int get_header_value(Request *r);

//Returns the end of the header
int get_header_end(Request *r);

//Deletes the Request struct
void request_delete(Request *r);

//Prints out the Request struct
void request_print(Request *r, FILE *outfile);

//END REQUEST STRUCT-----------------------------------------------------------------------------------------------------

//START RESPONSE STRUCT--------------------------------------------------------------------------------------------------

typedef struct {
    char *version;
    int status_code;
    char *status_phrase;
    char *header_key;
    int header_value;
} Response;

//Prints the Response Struct
void print_response(Response response, int outfile);

//END RESPONSE STRUCT-----------------------------------------------------------------------------------------------------

//START STATUS CODE STRUCT------------------------------------------------------------------------------------------------

typedef struct {
    int status_code;
    char *status_phrase;
    char *message;
} Status_Code;

//Creates the Status-Code struct
Status_Code status_code_create();

//Applies the proper code/phrase to Status_Code struct when given a status code
void get_status(int code, Status_Code *stat);

//Prints the Status-Code struct
void print_status_code(Status_Code sc, int outfile, int bytes_written);

//END STATUS CODE STRUCT-------------------------------------------------------------------------------------------------
