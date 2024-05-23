#include <stdio.h>
#include <regex.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#include "parse.h"
#include "global.h"

#define REG_COMMAND "([a-zA-Z]{1,8})"
#define REG_FILE    "([a-zA-Z0-9._]{1,63})"
#define REG_VERSION "([A-Z]{1,4}/[0-9].[0-9])(\r\n\r\n|\r\n)"

#define PARSE_REGEX_REQLINE "^" REG_COMMAND " /" REG_FILE " " REG_VERSION
#define PARSE_REGEX_HEADERS "(([a-zA-Z0-9._-]{1,128}): ([ -~]{0,128})\r\n)"
#define PARSE_RN            "(\r\n).*(\r\n\r\n)|(\r\n\r\n)"

int parse_request(char *buffer, Request *request, int buf_len) {
    regex_t re;
    regmatch_t matches[3];

    int rc_REQLINE;
    rc_REQLINE = regcomp(&re, PARSE_RN, REG_EXTENDED);
    assert(!rc_REQLINE);
    rc_REQLINE = regexec(&re, buffer, 3, matches, 0);

    int req_line_val = 0;

    if (rc_REQLINE == 0) {

        //Buffer for the Request Line
        char *request_line_buf = buffer;

        //If Request_Line > 2048
        if ((matches[0].rm_eo - matches[0].rm_so) > 2048) {
            regfree(&re);
            return 400;
        }

        int offset = matches[1].rm_eo;

        req_line_val = parse_request_line(request_line_buf, request);

        //If Request Line had an invalid input
        if (req_line_val != 0 && req_line_val != 201) {
            regfree(&re);
            return req_line_val;
        }

        //IF COMMAND IS "PUT"
        if (strcmp(get_request_command(request), "PUT") == 0) {
            //Buffer for the Header Field
            char *header_field_buf = buffer + offset;
            int count = 0;
            char container[300];
            int container_count = 0;
            int content_word_check = 0;

            while (count < buf_len) {

                container[container_count] = header_field_buf[count];

                //Checking an individual headerfield
                if (header_field_buf[count] == '\r' && header_field_buf[count + 1] == '\n') {
                    container[container_count + 1] = '\n';
                    container[container_count + 2] = '\0';

                    //STILL GOING THROUGH ALL THE HEADER FIELDS
                    if (compare_keyword(container, "Content-Length", request) != -1) {
                        content_word_check = 1;
                        break;
                    } else {
                        memset(container, '\0', sizeof(container));
                        container_count = 0;
                        count += 2;
                        continue;
                    }
                }
                count++;

                //If the header-size is past its limit (128 Key | 128 Value)
                if (container_count > 256) {
                    memset(container, '\0', sizeof(container));
                    container_count = 0;
                    count += 2;
                    continue;
                }
                container_count++;
            }
            if(content_word_check != 1){
                regfree(&re); //    Request is ill-formatted
                return 400;
            }
        }
    } else {
        regfree(&re); //    Request is ill-formatted
        return 400;
    }

    //Only goes here if the request was valid
    regfree(&re);
    return req_line_val;
}

//Parse the request line
//Return 0 if succesful, -1 if invalid
int parse_request_line(char *buffer, Request *request) {
    regex_t re;
    regmatch_t matches_RQ[5];
    int final
        = 0; // Variable will be used to send that the file has been created at the end if its a PUT request

    int rc_REQLINE;
    rc_REQLINE = regcomp(&re, PARSE_REGEX_REQLINE, REG_EXTENDED);
    assert(!rc_REQLINE);
    rc_REQLINE = regexec(&re, buffer, 5, matches_RQ, 0);

    if (rc_REQLINE == 0) {

        //Read in command
        request->command = buffer;
        request->command[matches_RQ[1].rm_eo] = '\0';

        //Checking for valid command
        if (strcmp(get_request_command(request), "GET") != 0
            && strcmp(get_request_command(request), "PUT") != 0) {
            regfree(&re);
            return 501; //Method Not Implemented
        }

        //Read in file path
        request->target_path = buffer + matches_RQ[2].rm_so;
        request->target_path[matches_RQ[2].rm_eo - matches_RQ[2].rm_so] = '\0';

        //Read in version
        char *version = NULL;
        version = buffer + matches_RQ[3].rm_so;
        version[matches_RQ[3].rm_eo - matches_RQ[3].rm_so] = '\0';

        //Checking for version
        if (strcmp(version, "HTTP/1.1") != 0) {
            regfree(&re);
            return 505; //  Request includes an unsupported version
        }

        //Checking file for "GET" || DOES NOT OPEN FILE
        if (strcmp(get_request_command(request), "GET") == 0) {

            //Checking if path is a directory
            struct stat path;
            stat(get_target_path(request), &path);
            if (S_ISDIR(path.st_mode) != 0) {
                regfree(&re);
                return 403; //  When the URI’s file does not exist
            }

            //Checking FILE stuff
            char *in_file = get_target_path(request);

            //If file DNE
            if (access(in_file, F_OK) != 0) {
                regfree(&re);
                return 404; //  When the URI’s file does not exist
            }

            //Checking if file has read access
            access(in_file, R_OK);
            if (errno == EACCES) {
                regfree(&re);
                return 403; //URI’s file is not accessible
            }
        }
    } else {
        //Free REGEX
        regfree(&re);
        return 400; //  Request is ill-formatted
    }
    //Free REGEX
    regfree(&re);
    return final;
}

//Checks buffer for keyword "Content-Length" in this case and returns its value |||| Returns -1 if word DNE
int compare_keyword(char *buffer, char *keyword, Request *request) {
    regex_t rh;
    regmatch_t matches[5];

    int rc_HEADER;
    rc_HEADER = regcomp(&rh, PARSE_REGEX_HEADERS, REG_EXTENDED);
    assert(!rc_HEADER);
    rc_HEADER = regexec(&rh, buffer, 5, matches, 0);

    if (rc_HEADER == 0) {

        char *header_field_key = buffer;

        header_field_key[matches[2].rm_eo] = '\0';

        if (strcmp(header_field_key, keyword) == 0) {

            request->header_key = keyword;

            //Buffer for the value of the key
            char *header_field_val = buffer + matches[3].rm_so;
            header_field_val[matches[3].rm_eo - matches[3].rm_so] = '\0';
            
            //Checking to make sure value is not null
            if((matches[3].rm_eo - matches[3].rm_so) == 0){
                regfree(&rh);
                return -1;
            }

            //Checking to make sure the value is a valid integer
            for (int i = 0; i < matches[3].rm_eo - matches[3].rm_so; i++) {
                if (header_field_val[i] < 48 || header_field_val[i] > 57) {
                    regfree(&rh);
                    return -1;
                }
            }
            int value = atoll(header_field_val);
            request->header_value = value;
            regfree(&rh);
            return 0;
        }
    } else {
        regfree(&rh);
        return -1;
    }
    regfree(&rh);
    return -1;
}
