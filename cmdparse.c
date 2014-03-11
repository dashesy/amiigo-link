/*
 * Amiigo Link command text parser
 *
 * @date March 8, 2014
 * @author: dashesy
 */

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "amcmd.h"
#include "cmdparse.h"

AMIIGO_CMD g_cmd = AMIIGO_CMD_NONE;
amcfg_t g_cfg;
extern char g_src[512];

// Initialize the command configs to defaults
void cmd_init(void) {
    //---------- config packets ------------
    memset(&g_cfg, 0, sizeof(g_cfg));

    // Some defulats
    g_cfg.maint_led.duration = 5;
    g_cfg.maint_led.led = 6;
    g_cfg.maint_led.speed = 1;
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
    } else if (strcasecmp(szName, "rename") == 0) {
        g_cmd = AMIIGO_CMD_RENAME;
    } else if (strcasecmp(szName, "tag") == 0) {
        g_cmd = AMIIGO_CMD_TAG;
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

    char * str = strdup(szName);

    char * pch;
    pch = strtok (str, ",");
    int dev_count = 0;
    while (pch != NULL) {
        if (dev_count >= MAX_DEV_COUNT) {
            fprintf(stderr, "Maximum of %d devices can be connected to\n", MAX_DEV_COUNT);
            return -1;
        }
        g_cfg.dst[dev_count] = pch;
        dev_count++;
        pch = strtok (NULL, ",");
    }
    if (dev_count == 0) {
        fprintf(stderr, "Invalid device address");
        return -1;
    }
    g_cfg.count_dst = dev_count;
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
            g_cfg.i2c.address = (uint8)atoi(pch);
        else if (arg_count == 2) {
            g_cfg.i2c.reg = (uint8)atoi(pch);
        } else if (arg_count == 3) {
            g_cfg.i2c.data = (uint8)atoi(pch);
        } else {
            break;
        }
        pch = strtok (NULL, ":");
    }
    if (arg_count != 3) {
        fprintf(stderr, "Invalid i2c address:reg:value format");
        return -1;
    }
    g_cfg.i2c.write = 1;
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
            g_cfg.i2c.address = (uint8)atoi(pch);
        } else if (arg_count == 2) {
            g_cfg.i2c.reg = (uint8)atoi(pch);
        } else {
            break;
        }
        pch = strtok (NULL, ":");
    }
    if (arg_count != 2) {
        printf("Invalid i2c address:reg format");
        return -1;
    }
    g_cfg.i2c.write = 0;
    g_cmd = AMIIGO_CMD_I2C_READ;
    return 0;
}

// Parse a single parameter
int parse_config_single(const char * szParam) {
    int err = 0;
    switch (g_cmd) {
    case AMIIGO_CMD_RENAME:
        if (strlen(szParam) > DEV_NAME_LEN)
            return -1;
        strcpy(&g_cfg.name.name[0], szParam);
        break;
    default:
        err = -1;
        break;
    }
    return err;
}

// Parse param = value pairs
int set_config_pairs(const char * szParam, const char * szVal) {

    if (szVal != NULL || szVal[0] == 0 || isspace(szVal[0]))
        return parse_config_single(szParam);

    long val = strtol(szVal, NULL, 0);
    //---------- Light ----------------------------
    if (strcasecmp(szParam, "ls_fast_interval") == 0) {
        // Seconds between samples in fast mode
        g_cfg.config_ls.fast_interval = (uint16_t) val;
    } else if (strcasecmp(szParam, "ls_slow_interval") == 0) {
        // Seconds between samples in slow mode
        g_cfg.config_ls.slow_interval = (uint16_t) val;
    } else if (strcasecmp(szParam, "ls_duration") == 0) {
        g_cfg.config_ls.duration = (uint8_t) val;
    } else if (strcasecmp(szParam, "ls_debug") == 0) {
        g_cfg.config_ls.debug = (uint8_t) val;
    } else if (strcasecmp(szParam, "ls_flags") == 0) {
        g_cfg.config_ls.flags = (uint8_t) val;
    } else if (strcasecmp(szParam, "ls_movement") == 0) {
        g_cfg.config_ls.movement = (uint8_t) val;
    }
    // --------------- Blink ----------------------
    else if (strcasecmp(szParam, "blink_duration") == 0) {
        g_cfg.maint_led.duration = (uint8_t) val;
    }
    else if (strcasecmp(szParam, "blink_led") == 0) {
        g_cfg.maint_led.led = (uint8_t) val;
    }
    else if (strcasecmp(szParam, "blink_speed") == 0) {
        g_cfg.maint_led.speed = (uint8_t) val;
    }
    // ---------------- Accel ---------------------
    else if (strcasecmp(szParam, "accel_slow_rate") == 0) {
        g_cfg.config_accel.slow_rate = WEDConfigRateParam(
                (uint32_t) atoi(szVal));
    } else if (strcasecmp(szParam, "accel_fast_rate") == 0) {
        g_cfg.config_accel.fast_rate = WEDConfigRateParam(
                (uint32_t) atoi(szVal));
    }
    // ---------------- rename ---------------------
    else if (strcasecmp(szParam, "name") == 0) {
        if (strlen(szVal) > DEV_NAME_LEN) {
            fprintf(stderr, "Name cannot be more than %d characters", DEV_NAME_LEN);
            return -1;
        }
        strcpy(&g_cfg.name.name[0], szVal);
    }
    // ---------------- tag ---------------------
    else if (strcasecmp(szParam, "tag") == 0) {
        uint32_t uval = (uint32_t)val;
        memcpy(&g_cfg.general.tag[0], &uval, 4);
    } else {
        fprintf(stderr,
                "Configuration parameter %s not recognized!\n",
                szParam);
        return -1;
    }
    return 0;
}

// Parse one line of param or param=value
int parse_one_pair(char * szLine) {
    char * pch;
    pch = strtok (szLine, "=");
    if (pch == NULL)
        return parse_config_single(szLine);

    return set_config_pairs(szLine, pch);
}

// Parse single command line
int parse_input_line(const char * szName) {
    int err = 0;
    char * szArg = strdup(szName);

    char * pch;
    pch = strtok (szArg, ",");
    while (pch != NULL) {
        err = parse_one_pair(pch);
        if (err)
            break;
        pch = strtok (NULL, ",");
    }

    free(szArg);
    return err;
}

// Parse file
int parse_input_file(const char * szName) {

    int is_path = (strpbrk(szName, "./") != NULL);
    // If it is detected to be sequence of param=value[,...] parse the line
    if (strpbrk(szName, "=,") && !is_path)
        return parse_input_line(szName);

    char szCmd[256];
    char szParam[256];
    char szVal[256];
    FILE * fp = fopen(szName, "r");
    if (fp == NULL) {
        // Try it as a single param
        if (!is_path) {
            int err = parse_config_single(szName);
            if (err == 0)
                return 0;
        }
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

        int err = set_config_pairs(szParam, szVal);
        if (err) {
            fclose(fp);
            return err;
        }
    }

    fclose(fp);

    return 0;
}
