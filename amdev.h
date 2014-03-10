/*
 * Amiigo device
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#ifndef AMDEV_H
#define AMDEV_H

#include <stdio.h>
#include "amidefs.h"

typedef enum _DISCOVERY_STATE {
    STATE_NONE = 0,
    STATE_BUILD,
    STATE_VERSION,
    STATE_STATUS,
    STATE_DOWNLOAD,
    STATE_FWSTATUS,
    STATE_FWSTATUS_WAIT,
    STATE_I2C,

    STATE_COUNT, // This must be the last
} DISCOVERY_STATE;

// Keep the state of each device here
typedef struct _amdev {
    DISCOVERY_STATE state;     // Device state machine state
    int sock;                  // Socket openned for this device
    WEDVersion ver;            // Firmware version
    unsigned int ver_flat;     // Flat version number to compare
    WEDStatus status;          // Firmware status
    struct {
        uint8 type; // WED_LOG_TAG
        // Tag data from WED_MAINT_TAG command
        uint32_t tag;
    } PACKED logTag;           // Last tag
    WEDLogTimestamp logTime;   // Last timestamp packet
    WEDLogAccel logAccel;      // Last accel
    uint32_t read_logs;        // Logs downloaded so far
    uint32_t total_logs;       // Total number of logs tp be downloaded
    int bValidAccel;           // If any uncompressed accel is received
    char szBuild[512];         // Firmware build text
    char szVersion[512];       // Firmware version text
    FILE * logFile;            // file to download logs
} amdev_t;

#endif // include guard
