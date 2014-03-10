/*
 * Amiigo Link data processing
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#ifndef AMLPROCESS_H
#define AMLPROCESS_H

// Optional all-inclusive handles
#define OPT_START_HANDLE 0x0001
#define OPT_END_HANDLE   0xffff

int discover_device(amdev_t * dev);
int process_data(amdev_t * dev, uint8_t * buf, ssize_t buflen);

#endif // include guard
