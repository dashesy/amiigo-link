/*
 * Amiigo characteristics
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "amchar.h"

struct gatt_char g_char[AMIIGO_UUID_COUNT];

// Client configuration (for notification and indication)
const char * g_szUUID[] = {
    "00002902-0000-1000-8000-00805f9b34fb",
    "cca31000-78c6-4785-9e45-0887d451317c",
    "cca30001-78c6-4785-9e45-0887d451317c",
    "cca30002-78c6-4785-9e45-0887d451317c",
    "cca30003-78c6-4785-9e45-0887d451317c",
    "cca30004-78c6-4785-9e45-0887d451317c",
    "cca30005-78c6-4785-9e45-0887d451317c",
    "cca30006-78c6-4785-9e45-0887d451317c",
    "cca30007-78c6-4785-9e45-0887d451317c",
};

// Initialize the characteristics with Amiigo defaults
void char_init() {
    memset(g_char, 0, sizeof(g_char));
    int i;
    for (i = 0; i < AMIIGO_UUID_COUNT; ++i)
        bt_string_to_uuid(&g_char[i].uuid, g_szUUID[i]);

    // Set default handles
    g_char[AMIIGO_UUID_STATUS].value_handle = 0x0025;
    g_char[AMIIGO_UUID_CONFIG].value_handle = 0x0027;
    g_char[AMIIGO_UUID_LOGBLOCK].value_handle = 0x0029;
    g_char[AMIIGO_UUID_FIRMWARE].value_handle = 0x002c;
    g_char[AMIIGO_UUID_DEBUG].value_handle = 0x002e;
    g_char[AMIIGO_UUID_BUILD].value_handle = 0x0030;
    g_char[AMIIGO_UUID_VERSION].value_handle = 0x0032;
}
