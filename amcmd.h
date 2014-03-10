/*
 * Amiigo commands
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#ifndef AMCMD_H
#define AMCMD_H

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
} AMIIGO_CMD;

#endif // include guard
