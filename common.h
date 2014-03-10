/*
 * Amiigo Link common
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#ifndef COMMON_H
#define COMMON_H

typedef struct _aml_options {
    int g_leave_compressed; // If packets should be left in compressed form
    int g_raw;              // If should download logs in raw format (no compression in firmware)
    int g_verbosity;        // verbose output
    int g_live;             // if live output is needed
    int g_console;          // if should print accel to console
    int g_bFull;            // If full characteristcs should be discovered
} aml_options_t;

extern aml_options_t g_opt;

#endif // include guard
