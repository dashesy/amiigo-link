/*
 * GAP protocol
 *
 * @date March 8, 2014
 * @author: dashesy
 */

#include <stdlib.h>
#include <stdint.h>

#include "jni/bluetooth.h"
#include "att.h"

#include "amidefs.h"

extern size_t g_buflen;

// Read a characteristic
// Inputs:
//   handle - characteristics handle to read from
int exec_read(uint16_t handle) {
    if (handle == 0)
        return -1;

    uint8_t * buf = malloc(g_buflen);
    memset(buf, 0, g_buflen);

    uint16_t plen = enc_read_req(handle, buf, g_buflen);

    ssize_t len = send(g_sock, buf, plen, 0);

    free(buf);
    if (len < 0 || len != plen) {
        return -1;
    }
    return 0;
}

// Write to a characteristic
// Inputs:
//   handle - characteristics handle to write to
//   value  - value to write
//   vlen   - size of value in bytes
int exec_write(uint16_t handle, const uint8_t * value, size_t vlen) {
    if (handle == 0)
        return -1;

    uint8_t * buf = malloc(g_buflen);
    memset(buf, 0, g_buflen);
    uint16_t plen = enc_write_cmd(handle, value, vlen, buf, g_buflen);

    ssize_t len = send(g_sock, buf, plen, 0);

    free(buf);
    if (len < 0 || len != plen) {
        return -1;
    }

    return 0;
}

// Write to a characteristic with response
// Inputs:
//   handle - characteristics handle to write to
//   value  - value to write
//   vlen   - size of value in bytes
int exec_write_req(uint16_t handle, const uint8_t * value, size_t vlen) {
    if (handle == 0)
        return -1;

    uint8_t * buf = malloc(g_buflen);
    memset(buf, 0, g_buflen);
    uint16_t plen = enc_write_req(handle, value, vlen, buf, g_buflen);

    ssize_t len = send(g_sock, buf, plen, 0);

    free(buf);
    if (len < 0 || len != plen) {
        return -1;
    }

    return 0;
}

// Find handle associated with given UUID
int discover_handles(uint16_t start_handle, uint16_t end_handle) {
    uint16_t plen;
    bt_uuid_t type_uuid;

    uint8_t * buf = malloc(g_buflen);
    memset(buf, 0, g_buflen);

    bt_uuid16_create(&type_uuid, GATT_CHARAC_UUID);

    plen = enc_read_by_type_req(start_handle, end_handle, &type_uuid, buf,
            g_buflen);

    ssize_t len = send(g_sock, buf, plen, 0);

    free(buf);
    if (len < 0 || len != plen) {
        return -1;
    }
    return 0;
}


