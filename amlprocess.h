/*
 * Amiigo Link data processing
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#ifndef AMLPROCESS_H
#define AMLPROCESS_H

typedef struct _amdev {
} amdev_t;

void process_init(void);

int process_data(uint8_t * buf, ssize_t buflen);

#endif // include guard
