/*
 * Amiigo Link firmware update process
 *
 * @date March 10, 2014
 * @author: dashesy
 * @copyright Amiigo Inc.
 */

#ifndef FWUPDATE_H
#define FWUPDATE_H

int set_update_file(const char * szName);
int process_fwstatus(amdev_t * dev, uint8_t * buf, ssize_t buflen);

#endif // include guard
