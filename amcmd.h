/*
 * Amiigo commands and configs
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#ifndef AMCMD_H
#define AMCMD_H

#include "amidefs.h"

typedef enum _AMIIGO_CMD {
    AMIIGO_CMD_NONE = 0,      // Just to do a connection status test
    AMIIGO_CMD_FWUPDATE,      // Firmware update
    AMIIGO_CMD_RESET_LOGS,    // Reset log buffer
    AMIIGO_CMD_RESET_CPU,     // Reset CPU
    AMIIGO_CMD_RESET_CONFIGS, // Reset configurations to default
    AMIIGO_CMD_DOWNLOAD,      // Download all the logs
    AMIIGO_CMD_CONFIGLS,      // Configure light sensors
    AMIIGO_CMD_CONFIGACCEL,   // Configure acceleration sensors
    AMIIGO_CMD_BLINK,         // Configure a single blink
    AMIIGO_CMD_I2C_READ,      // Read i2c address and register
    AMIIGO_CMD_I2C_WRITE,     // Write to i2c address and register
    AMIIGO_CMD_RENAME,        // Rename the WED
    AMIIGO_CMD_TAG,           // Write a tag
} AMIIGO_CMD;

#define MAX_DEV_COUNT   10    // Maximum number of devices to work with
typedef struct amiigo_config {
    WEDDebugI2CCmd i2c;          // i2c debugging
    WEDConfigLS config_ls;       // Light configuration
    WEDConfigAccel config_accel; // Acceleration sensors configuration
    WEDConfigName name;          // WED name
    WEDMaintLED maint_led;       // Blink command
    WEDConfigGeneral general;    // For tags
    // Device and interface to use
    int count_dst;
    char * dst[MAX_DEV_COUNT];
} amcfg_t;

extern AMIIGO_CMD g_cmd;

// Parsed config that goes with the command
extern amcfg_t  g_cfg;

#endif // include guard
