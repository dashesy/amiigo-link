/*
 * Amiigo Link command text parser
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#ifndef CMDPARSE_H
#define CMDPARSE_H

#include "amcmd.h"

extern AMIIGO_CMD g_cmd;

int parse_command(const char * szName);
int parse_adapter(const char * szName);
int parse_device(const char * szName);
int parse_i2c_write(const char * szArg);
int parse_i2c_read(const char * szArg);
int parse_input_file(const char * szName) ;


#endif // include guard
