/*
 * Amiigo Link command text parser
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#ifndef CMDPARSE_H
#define CMDPARSE_H

void cmd_init(void);

int is_parse_file_name(const char * szName);
int parse_command(const char * szName);
int parse_adapter(const char * szName);
int parse_device(const char * szName);
int parse_i2c_write(const char * szArg);
int parse_i2c_read(const char * szArg);
int parse_input_line(const char * szName) ;
int parse_input_file(const char * szName) ;
int parse_mode(const char * szName);


#endif // include guard
