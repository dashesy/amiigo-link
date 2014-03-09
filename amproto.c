/*
 * Amiigo low-level protocol
 *
 * @date March 8, 2014
 * @author: dashesy
 */

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

#include "amidefs.h"
#include "amproto.h"

// Start firmware update procedure
int exec_fwupdate() {
    int ret;
    g_state = STATE_FWSTATUS;

    uint16_t handle = g_char[AMIIGO_UUID_FIRMWARE].value_handle;
    if (handle == 0)
        return -1; // Not ready yet

    printf("\nPreparing for update ...\n");

    // Now read for status
    ret = exec_read(handle);

    return ret;
}

// Debug i2c by reading or writing register on given address
int exec_debug_i2c() {
    int ret;
    g_state = STATE_I2C;

    uint16_t handle = g_char[AMIIGO_UUID_DEBUG].value_handle;
    if (handle == 0)
        return -1; // Not ready yet

    ret = exec_write(handle, (uint8_t *) &g_i2c, sizeof(g_i2c));
    if (ret)
        return -1;

    // Wait for it a little
    usleep(100);
    ret = exec_read(handle);

    return ret;
}

// Start downloading the log packets
int exec_download() {
    if (g_status.num_log_entries == 0) {
        // Nothing to download!
        g_state = STATE_COUNT;
        return 0;
    }

    g_state = STATE_DOWNLOAD; // Download in progress
    g_total_logs = g_status.num_log_entries; // How many logs to download

    uint16_t handle = g_char[AMIIGO_UUID_CONFIG].value_handle;
    if (handle == 0)
        return -1; // Not ready yet
    printf("\n\n");
    // Config for notify
    WEDConfig config;
    memset(&config, 0, sizeof(config));
    config.config_type = WED_CFG_LOG;
    if (g_raw)
        config.log.flags = WED_CONFIG_LOG_DL_EN;
    else
        config.log.flags = WED_CONFIG_LOG_DL_EN | WED_CONFIG_LOG_CMP_EN;
    if (g_live)
        config.log.flags |= WED_CONFIG_LOG_LOOPBACK;


    int ret = exec_write(handle, (uint8_t *) &config, sizeof(config));
    if (ret)
        return -1;

    return 0;
}

// Start configuration of light sensors
int exec_configls() {
    g_state = STATE_COUNT; // Done with command

    uint16_t handle = g_char[AMIIGO_UUID_CONFIG].value_handle;
    if (handle == 0)
        return -1; // Not ready yet

    WEDConfig config;
    memset(&config, 0, sizeof(config));
    config.config_type = WED_CFG_LS;
    config.lightsensor = g_config_ls;

    int ret = exec_write(handle, (uint8_t *) &config, sizeof(config));
    if (ret)
        return -1;

    return 0;
}

// Start configuration of accel sensors
int exec_configaccel() {
    g_state = STATE_COUNT; // Done with command

    uint16_t handle = g_char[AMIIGO_UUID_CONFIG].value_handle;
    if (handle == 0)
        return -1; // Not ready yet

    WEDConfig config;
    memset(&config, 0, sizeof(config));
    config.config_type = WED_CFG_ACCEL;
    config.accel = g_config_accel;

    int ret = exec_write(handle, (uint8_t *) &config, sizeof(config));
    if (ret)
        return -1;

    return 0;
}

// Start LED blinking
int exec_blink() {
    g_state = STATE_COUNT; // Done with command

    uint16_t handle = g_char[AMIIGO_UUID_CONFIG].value_handle;
    if (handle == 0)
        return -1; // Not ready yet

    WEDConfigMaint config_maint;
    memset(&config_maint, 0, sizeof(config_maint));
    config_maint.command = WED_MAINT_BLINK_LED;
    config_maint.led = g_maint_led;
    WEDConfig config;
    memset(&config, 0, sizeof(config));
    config.config_type = WED_CFG_MAINT;
    config.maint = config_maint;

    int ret = exec_write(handle, (uint8_t *) &config, sizeof(config));
    if (ret)
        return -1;

    return 0;
}

// Reset config, or CPU or log buffer
int exec_reset(enum AMIIGO_CMD cmd) {
    g_state = STATE_COUNT; // Done with command

    uint16_t handle = g_char[AMIIGO_UUID_CONFIG].value_handle;
    if (handle == 0)
        return -1; // Not ready yet
    WEDConfigMaint config_maint;
    memset(&config_maint, 0, sizeof(config_maint));
    switch (cmd) {
    case AMIIGO_CMD_RESET_CPU:
        printf("Restarting the device ...\n");
        config_maint.command = WED_MAINT_RESET;
        break;
    case AMIIGO_CMD_RESET_LOGS:
        printf("Clearing the logs ...\n");
        config_maint.command = WED_MAINT_CLEAR_LOG;
        break;
    case AMIIGO_CMD_RESET_CONFIGS:
        printf("Reseting the configurations to default ...\n");
        config_maint.command = WED_MAINT_RESET_CONFIG;
        break;
    default:
        return -1;
        break;
    }
    WEDConfig config;
    memset(&config, 0, sizeof(config));
    config.config_type = WED_CFG_MAINT;
    config.maint = config_maint;

    int ret = exec_write(handle, (uint8_t *) &config, sizeof(config));
    if (ret)
        return -1;

    return 0;
}

