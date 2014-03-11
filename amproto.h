/*
 * Amiigo low-level protocol
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#ifndef AMPROTO_H
#define AMPROTO_H

#include "amcmd.h"

int exec_status(int sock);
int exec_fwupdate(int sock);
int exec_debug_i2c(int sock);
int exec_download(int sock);
int exec_configls(int sock);
int exec_configaccel(int sock);
int exec_blink(int sock);
int exec_tag(int sock);
int exec_rename(int sock);
int exec_reset(int sock, AMIIGO_CMD cmd);
int discover_handles(int sock, uint16_t start_handle, uint16_t end_handle);

#endif // include guard
