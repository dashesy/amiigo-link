/*
 * Old Amiigo low-level protocol for backward compatibility
 *
 * @date March 14, 2014
 * @author: dashesy
 * @copyright Amiigo Inc.
 *
 * @notes:
 *
 *  same as amproto.c
 *
 */

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

#include "amidefs.h"
#include "common.h"
#include "gapproto.h"
#include "amoldproto.h"
#include "amchar.h"
#include "amcmd.h"

// Start configuration of light sensors
int exec_configls_18116(int sock) {

    uint16_t handle = g_char[AMIIGO_UUID_CONFIG].value_handle;
    if (handle == 0)
        return -1; // Not ready yet

    WEDConfig_1816 config;
    memset(&config, 0, sizeof(config));
    config.config_type = WED_CFG_LS;
    // backward compatibility

    config.lightsensor.fast_interval = g_cfg.config_ls.fast_interval;
    config.lightsensor.slow_interval = g_cfg.config_ls.slow_interval;
    config.lightsensor.sleep_interval = g_cfg.config_ls.sleep_interval;
    config.lightsensor.duration = g_cfg.config_ls.manual_duration;
    config.lightsensor.debug = g_cfg.config_ls.debug;
    config.lightsensor.movement = g_cfg.config_ls.movement;
    config.lightsensor.flags = g_cfg.config_ls.flags;

    int ret = exec_write(sock, handle, (uint8_t *) &config, sizeof(config.config_type) + sizeof(config.lightsensor));
    if (ret)
        return -1;

    return 0;
}
