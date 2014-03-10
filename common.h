/*
 * Amiigo Link common
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#ifndef COMMON_H
#define COMMON_H

typedef struct _aml_options {
    int leave_compressed; // If packets should be left in compressed form
    int raw;              // If should download logs in raw format (no compression in firmware)
    int verbosity;        // verbose output
    int live;             // if live output is needed
    int console;          // if should print accel to console
    int full;             // If full characteristcs should be discovered
} aml_options_t;

extern aml_options_t g_opt;

#endif // include guard