/*
 * Amiigo Link data processing
 *
 * @date March 9, 2014
 * @author: dashesy
 * @copyright Amiigo Inc.
 */

#ifndef AMLPROCESS_H
#define AMLPROCESS_H

#include "amdev.h"

// Optional all-inclusive handles
#define OPT_START_HANDLE 0x0001
#define OPT_END_HANDLE   0xffff

int discover_device(amdev_t * dev);
int process_data(amdev_t * dev, uint8_t * buf, ssize_t buflen);
void print_status(uint8_t status);

#endif // include guard
