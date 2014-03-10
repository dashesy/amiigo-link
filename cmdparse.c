/*
 * Amiigo Link command text parser
 *
 * @date March 8, 2014
 * @author: dashesy
 */

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>


#include "cmdparse.h"

extern AMIIGO_CMD g_cmd;

int parse_command(const char * szName) {
    if (strcasecmp(szName, "download") == 0) {
        g_cmd = AMIIGO_CMD_DOWNLOAD;
    } else if (strcasecmp(szName, "resetcpu") == 0) {
        g_cmd = AMIIGO_CMD_RESET_CPU;
    } else if (strcasecmp(szName, "resetlogs") == 0) {
        g_cmd = AMIIGO_CMD_RESET_LOGS;
    } else if (strcasecmp(szName, "resetconfigs") == 0) {
        g_cmd = AMIIGO_CMD_RESET_CONFIGS;
    } else if (strcasecmp(szName, "configls") == 0) {
        g_cmd = AMIIGO_CMD_CONFIGLS;
    } else if (strcasecmp(szName, "configaccel") == 0) {
        g_cmd = AMIIGO_CMD_CONFIGACCEL;
    } else if (strcasecmp(szName, "blink") == 0) {
        g_cmd = AMIIGO_CMD_BLINK;
    } else if (strcasecmp(szName, "status") == 0) {
        g_cmd = AMIIGO_CMD_NONE;
    } else {
        fprintf(stderr, "Invalid command (%s)!\n", szName);
        return -1;
    }
    return 0;
}

int parse_adapter(const char * szName) {
    strcpy(g_src, szName);
    return 0;
}

int parse_device(const char * szName) {
    strcpy(g_dst, szName);
    return 0;
}

int parse_i2c_write(const char * szArg) {

    char * str = strdup(szArg);
    char * pch;
    pch = strtok (str, ":");
    int arg_count = 0;
    while (pch != NULL) {
        arg_count++;
        if (arg_count == 1)
            g_i2c.address = (uint8)atoi(pch);
        else if (arg_count == 2) {
            g_i2c.reg = (uint8)atoi(pch);
        } else if (arg_count == 3) {
            g_i2c.data = (uint8)atoi(pch);
        } else {
            break;
        }
        pch = strtok (NULL, ":");
    }
    if (arg_count != 3) {
        printf("Invalid i2c address:reg:value format");
        return -1;
    }
    g_i2c.write = 1;
    g_cmd = AMIIGO_CMD_I2C_WRITE;
    return 0;
}

int parse_i2c_read(const char * szArg) {
    char * str = strdup(szArg);
    char * pch;
    pch = strtok (str, ":");
    int arg_count = 0;
    while (pch != NULL) {
        arg_count++;
        if (arg_count == 1) {
            g_i2c.address = (uint8)atoi(pch);
        } else if (arg_count == 2) {
            g_i2c.reg = (uint8)atoi(pch);
        } else {
            break;
        }
        pch = strtok (NULL, ":");
    }
    if (arg_count != 2) {
        printf("Invalid i2c address:reg format");
        return -1;
    }
    g_i2c.write = 0;
    g_cmd = AMIIGO_CMD_I2C_READ;
    return 0;
}

void trim(char *str)
{
    int i;
    int begin = 0;
    int end = strlen(str) - 1;

    while (isspace(str[begin]))
        begin++;

    while ((end >= begin) && isspace(str[end]))
        end--;

    // Shift all characters back to the start of the string array.
    for (i = begin; i <= end; i++)
        str[i - begin] = str[i];

    str[i - begin] = '\0'; // Null terminate string.
}

int parse_input_file(const char * szName) {
    char szCmd[256];
    char szParam[256];
    char szVal[256];
    FILE * fp = fopen(szName, "r");
    if (fp == NULL) {
        fprintf(stderr, "Configuration file (%s) not accessible!\n", szName);
        return -1;
    }
    while (fgets(szCmd, 256, fp) != NULL) {
        trim(szCmd);
        if (szCmd[0] == '#')
            continue; // Ignore comments
        if (sscanf(szCmd, "%s %s", szParam, szVal) != 2)
        {
            if (strlen(szCmd) > 1)
            {
                fprintf(stderr, "Command %s in %s not recognized!\n", szCmd, szName);
                fclose(fp);
                return -1;
            }
            continue;
        }

        long val = strtol(szVal, NULL, 0);
        //---------- Light ----------------------------
        if (strcasecmp(szParam, "ls_on_time") == 0) {
            g_config_ls.on_time = (uint8_t) val;
        } else if (strcasecmp(szParam, "ls_off_time") == 0) {
            g_config_ls.off_time = (uint8_t) val;
        } else if (strcasecmp(szParam, "ls_fast_interval") == 0) {
            // Seconds between samples in fast mode
            g_config_ls.fast_interval = (uint16_t) val;
        } else if (strcasecmp(szParam, "ls_slow_interval") == 0) {
            // Seconds between samples in slow mode
            g_config_ls.slow_interval = (uint16_t) val;
        } else if (strcasecmp(szParam, "ls_duration") == 0) {
            g_config_ls.duration = (uint8_t) val;
        } else if (strcasecmp(szParam, "ls_gain") == 0) {
            g_config_ls.gain = (uint8_t) val;
        } else if (strcasecmp(szParam, "ls_leds") == 0) {
            g_config_ls.leds = (uint8_t) val;
        } else if (strcasecmp(szParam, "ls_led_drv") == 0) {
            g_config_ls.led_drv = (uint8_t) val;
        } else if (strcasecmp(szParam, "ls_norecal") == 0) {
            g_config_ls.norecal = (uint8_t) val;
        } else if (strcasecmp(szParam, "ls_debug") == 0) {
            g_config_ls.debug = (uint8_t) val;
        } else if (strcasecmp(szParam, "ls_flags") == 0) {
            g_config_ls.flags = (uint8_t) val;
        } else if (strcasecmp(szParam, "ls_movement") == 0) {
            g_config_ls.movement = (uint8_t) val;
        }
        // --------------- Blink ----------------------
        else if (strcasecmp(szParam, "blink_duration") == 0) {
            g_maint_led.duration = (uint8_t) val;
        }
        else if (strcasecmp(szParam, "blink_led") == 0) {
            g_maint_led.led = (uint8_t) val;
        }
        else if (strcasecmp(szParam, "blink_speed") == 0) {
            g_maint_led.speed = (uint8_t) val;
        }
        // ---------------- Accel ---------------------
        else if (strcasecmp(szParam, "accel_slow_rate") == 0) {
            g_config_accel.slow_rate = WEDConfigRateParam(
                    (uint32_t) atoi(szVal));
        } else if (strcasecmp(szParam, "accel_fast_rate") == 0) {
            g_config_accel.fast_rate = WEDConfigRateParam(
                    (uint32_t) atoi(szVal));
        } else {
            fprintf(stderr,
                    "Configuration parameter %s in %s not recognized!\n",
                    szParam, szName);
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);

    return 0;
}
