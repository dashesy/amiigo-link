/*
 * Amiigo characteristics
 *
 * @date March 9, 2014
 * @author: dashesy
 * @copyright Amiigo Inc.
 */

#ifndef AMCHAR_H
#define AMCHAR_H

#include "jni/bluetooth.h"
#include "att.h"

enum {
    STD_UUID_CCC = 0,
    AMIIGO_UUID_SERVICE,
    AMIIGO_UUID_STATUS,
    AMIIGO_UUID_CONFIG,
    AMIIGO_UUID_LOGBLOCK,
    AMIIGO_UUID_FIRMWARE,
    AMIIGO_UUID_DEBUG,
    AMIIGO_UUID_BUILD,
    AMIIGO_UUID_VERSION,

    AMIIGO_UUID_COUNT // This must be the last
};

extern struct gatt_char g_char[AMIIGO_UUID_COUNT];

#endif // include guard
