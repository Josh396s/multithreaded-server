#pragma once

#include <stdio.h>

#include "global.h"

int parse_request(char *buffer, Request *request, int buf_len);

int parse_request_line(char *buffer, Request *request);

int compare_keyword(char *buffer, char *keyword, Request *request);
