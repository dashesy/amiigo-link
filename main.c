/*
 * Amiigo offline utility
 *
 * @date Aug 25, 2013
 * @author: dashesy
 */

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <getopt.h>
#include <time.h>
#include <ctype.h>

#include "jni/bluetooth.h"
#include "jni/l2cap.h"
#include "jni/hci.h"
#include "jni/hci_lib.h"
#if 0
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#endif

#include "btio.h"
#include "att.h"
#include "hcitool.h"
#include "amidefs.h"

#define FWUP_HDR_ID 0x0101

/******************************************************************************/
typedef struct {
    uint8* buf;
    uint8 pos;
} GetBits;
/******************************************************************************/
static inline void cmpGetBitsInit(GetBits* gb, void* buf) {

    gb->buf = buf;
    gb->pos = 0;
}
/******************************************************************************/
static int8 cmpGetBits(GetBits* gb, uint8 nbits) {

    int8 val = gb->buf[0] << gb->pos;

    uint8 buf0bits = 8 - gb->pos;
    if (buf0bits < nbits)
        val |= gb->buf[1] >> buf0bits;

    gb->pos += nbits;
    if (gb->pos >= 8) {
        gb->buf++;
        gb->pos -= 8;
    }

    return val >> (8 - nbits);
}

// Device and interface to use
char g_dst[512] = "";
char g_src[512] = "hci0";

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

// Firmware build and version information
char g_szBuild[512] = "Unknown";
char g_szVersion[512] = "Unknown";

// Optional all-inclusive handles
#define OPT_START_HANDLE 0x0001
#define OPT_END_HANDLE   0xffff

enum AMIIGO_CMD {
    AMIIGO_CMD_NONE = 0,      // Just to do a connection status test
    AMIIGO_CMD_FWUPDATE,      // Firmware update
    AMIIGO_CMD_RESET_LOGS,    // Reset log buffer
    AMIIGO_CMD_RESET_CPU,     // Reset CPU
    AMIIGO_CMD_RESET_CONFIGS, // Reset configurations to default
    AMIIGO_CMD_DOWNLOAD,      // Download all the logs
    AMIIGO_CMD_CONFIGLS,      // Configure light sensors
    AMIIGO_CMD_CONFIGACCEL,   // Configure acceleration sensors
    AMIIGO_CMD_BLINK,         // Configure a single blink
} g_cmd = AMIIGO_CMD_NONE;

WEDVersion g_ver; // Firmware version
unsigned int g_flat_ver = 0; // Flat version number to compare
#define FW_VERSION(Major, Minor, Build) (Major * 100000u + Minor * 1000u + Build)

WEDConfigLS g_config_ls; // Light configuration
WEDConfigAccel g_config_accel; // Acceleration sensors configuration
WEDMaintLED g_maint_led; // Blink command

WEDStatus g_status; // Device status

// Some log packets to keep their state
struct {
    uint8 type; // WED_LOG_TAG

    // Tag data from WED_MAINT_TAG command
    uint32_t tag;
} PACKED g_logTag;
WEDLogTimestamp g_logTime;
WEDLogAccel g_logAccel;
WEDLogLSConfig g_logLSConfig;

int g_sock; // Link socket
size_t g_buflen; // Established MTU
uint32_t g_read_logs = 0;  // Logs downloaded so far
uint32_t g_total_logs = 0; // Total number of logs tp be downloaded
int g_bValidAccel = 0; // If any uncompressed accel is received

FILE * g_logFile = NULL; // file to download logs

uint32_t g_fwup_speedup = 1; // How much to overload firmware update
FILE * g_fwImageFile = NULL; // Firmware image file
uint32_t g_fwImageSize = 0; // Firmware image size in bytes
uint32_t g_fwImageWrittenSize = 0; // bytes transmitted
uint16_t g_fwImagePage = 0; // Firmwate image total pages
uint16_t g_fwImageWrittenPage = 0; // pages transmitted

// Downloaded file
char g_szFullName[1024] = { 0 };

enum DISCOVERY_STATE {
    STATE_NONE = 0,
    STATE_BUILD,
    STATE_VERSION,
    STATE_STATUS,
    STATE_DOWNLOAD,
    STATE_FWSTATUS,
    STATE_FWSTATUS_WAIT,

    STATE_COUNT, // This must be the last
} g_state = STATE_NONE;


int g_bFull = 0; // If full characteristcs should be discovered

// Dump content of the buffer
int dump_buffer(uint8_t * buf, ssize_t buflen) {
    int i;
    printf("Unhandled (%s): \t", att_op2str(buf[0]));
    for (i = 0; i < buflen; ++i)
        printf("%02x ", buf[i]);
    printf("\n");

    return 0;
}

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
    config.log.flags = WED_CONFIG_LOG_DL_EN | WED_CONFIG_LOG_CMP_EN;

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

// Open file for logging
// Inputs:
//   szBase - the base name of the log
FILE * log_file_open() {
    char szDateTime[256];
    time_t now = time(NULL);
    // Use date-time to avoid overwriting logs
    strftime(szDateTime, 256, "%Y-%m-%d-%H-%M-%S", localtime(&now));
    // Use other metadata to distinguish each log
    sprintf(g_szFullName, "Log_%s.log", szDateTime);

    FILE * fp = fopen(g_szFullName, "w");
    return fp;
}

// Firmware update in progress
int process_fwstatus(uint8_t * buf, ssize_t buflen) {
    int ret = 0;
    WEDFirmwareStatus fwstatus;
    memset(&fwstatus, 0, sizeof(fwstatus));
    fwstatus.status = buf[1];
    fwstatus.error_code = buf[2];

    uint16_t handle = g_char[AMIIGO_UUID_FIRMWARE].value_handle;

    if (g_state == STATE_FWSTATUS)
    {
        if (fwstatus.status != WED_FWSTATUS_IDLE)
        {
            printf("Unfinished previous update detected.\nReset CPU and try again\n");
            return -1;
        }
    }

    if (fwstatus.status == WED_FWSTATUS_WAIT
            || fwstatus.status == WED_FWSTATUS_IDLE) {
        if (g_state == STATE_FWSTATUS)
        {
            WEDFirmwareCommand fwcmd;
            memset(&fwcmd, 0, sizeof(fwcmd));
            fwcmd.pkt_type = WED_FIRMWARE_INIT;
            fread(fwcmd.header, WED_FW_HEADER_SIZE, 1, g_fwImageFile);
            fseek(g_fwImageFile, 0, SEEK_SET);
            g_fwImageWrittenSize = 0;

            ret = exec_write(handle, (uint8_t *) &fwcmd, sizeof(fwcmd));
            if (ret)
                return -1;
            // We have already written the header
            g_state = STATE_FWSTATUS_WAIT;
        }
        ret = exec_read(handle); // Continue polling
    } else if (fwstatus.status == WED_FWSTATUS_ERROR) {
        switch (fwstatus.error_code) {
        case WED_FWERROR_HEADER:
            fprintf(stderr, "Firmware image header was unrecognized\n");
            break;
        case WED_FWERROR_SIZE:
            fprintf(stderr, "Lost packets or image size was too large\n");
            break;
        case WED_FWERROR_CRC:
            fprintf(stderr, "CRC check failed\n");
            break;
        case WED_FWERROR_FLASH:
            fprintf(stderr, "SPI flash error\n");
            break;
        case WED_FWERROR_OTHER:
            fprintf(stderr, "Internal error\n");
            break;
        default:
            fprintf(stderr, "Unknown error (%d)\n", fwstatus.status);
            break;
        }
        ret = -1;
    } else if (fwstatus.status == WED_FWSTATUS_UPLOAD_READY) {

        int bRetry = 0;
        int bFinished = 0;
        int i, j;
        for (j = 0; j < g_fwup_speedup && !bFinished && !bRetry; ++j)
        {
            for (i = 0; i < WED_FW_STREAM_BLOCKS; ++i) {
                WEDFirmwareCommand fwdata;
                memset(&fwdata, 0, sizeof(fwdata));
                fwdata.pkt_type = WED_FIRMWARE_DATA_BLOCK;
                long int offset = ftell(g_fwImageFile);
                size_t len = fread(fwdata.data, 1, WED_FW_BLOCK_SIZE, g_fwImageFile);
                if (len == 0) {
                    bFinished = 1;
                    printf("(ended)\n");
                    break;
                }
                if (len < WED_FW_BLOCK_SIZE) {
                    bFinished = 1;
                    printf("(uneven image size!)\n");
                    break;
                }
                g_fwImageWrittenSize += len;
                ret = exec_write(handle, (uint8_t *) &fwdata, sizeof(fwdata));
                if (ret)
                {
                    fseek(g_fwImageFile, offset, SEEK_SET);
                    bRetry = 1;
                    printf("(write retry)\n");
                    break;
                }
                if (feof(g_fwImageFile) || g_fwImageWrittenSize == g_fwImageSize)
                {
                    bFinished = 1;
                    break;
                }
            } // end for (i
            g_fwImageWrittenPage++;
            printf("\rUpdating ... %u/%u  page %u/%u (%2.0f%%)", g_fwImageWrittenSize, g_fwImageSize,
                    g_fwImageWrittenPage, g_fwImagePage, (g_fwImageWrittenSize * 100.0) / g_fwImageSize);
            fflush(stdout);
            usleep(100);
        }

        // Done uploding in our end
        if (bFinished) {
            printf(" (data done)");
            WEDFirmwareCommand fwcmd;
            memset(&fwcmd, 0, sizeof(fwcmd));
            fwcmd.pkt_type = WED_FIRMWARE_DATA_DONE;

            ret = exec_write(handle, (uint8_t *) &fwcmd, sizeof(fwcmd));
        }
        fflush(stdout);

        ret = exec_read(handle); // Continue polling
    } else if (fwstatus.status == WED_FWSTATUS_UPDATE_READY) {
        if (g_fwImageWrittenSize != g_fwImageSize) {
            fprintf(stderr, " Update not ready!\n");
            return -1;
        }
        WEDFirmwareCommand fwcmd;
        memset(&fwcmd, 0, sizeof(fwcmd));
        fwcmd.pkt_type = WED_FIRMWARE_UPDATE;

        ret = exec_write(handle, (uint8_t *) &fwcmd, sizeof(fwcmd));
        printf(" (Updating done)\n");
        // Done with command
        g_state = STATE_COUNT;
    } else {
        fprintf(stderr, "Unknown firmware update state (%u)\n", fwstatus.status);
        ret = -1;
    }
    return ret;
}

// Continue downloading packets
int process_download(uint8_t * buf, ssize_t buflen) {
    int i;
    uint16_t handle = 0;

    handle = att_get_u16(&buf[1]);
    if (buflen < 4) {
        printf("Last notification handle = 0x%04x\n", handle);
        g_state = STATE_COUNT;
        return 0;
    }

    // Note: Each packet starts a log entry

    WEDLogAccelCmp logAccelCmp;
    WEDLogLSData logLSData;
    WEDLogTemp logTemp;

    // TODO: check packet sizes
    // TODO: check data integrity

    int packet_len;
    int payload = 3; // Payload starting position

    WED_LOG_TYPE log_type = buf[payload] & 0x0F;
    while (payload < buflen) {
        if (log_type != WED_LOG_ACCEL_CMP)
            g_read_logs++; // Total number of log points downloaded so far

        switch (log_type) {
        uint16_t val16;
        uint8_t field_count, reset_detected;
        uint8_t * pdu;
        int nbits;

        case WED_LOG_TIME:
            packet_len = sizeof(g_logTime);

            if (g_logFile == NULL)
                g_logFile = log_file_open();

            g_logTime.type = log_type;
            g_logTime.timestamp = att_get_u32(&buf[payload + 1]);
            g_logTime.flags = buf[payload + 5];
            reset_detected = g_logTime.flags & TIMESTAMP_REBOOTED;

            if (reset_detected)
                printf(" (reboot detected)\n");

            fprintf(g_logFile, "[\"timestamp\",[%u,%u]]\n", g_logTime.timestamp, g_logTime.flags);

            break;
        case WED_LOG_ACCEL:
            packet_len = sizeof(g_logAccel);

            if (g_logFile == NULL)
                g_logFile = log_file_open();

            g_bValidAccel = 1;
            g_logAccel.type = log_type;
            for (i = 0; i < 3; ++i)
                g_logAccel.accel[i] = buf[payload + 1 + i];
            fprintf(g_logFile, "[\"accelerometer\",[%d,%d,%d]]\n", g_logAccel.accel[0],
                    g_logAccel.accel[1], g_logAccel.accel[2]);
            break;
        case WED_LOG_LS_CONFIG:
            packet_len = sizeof(g_logLSConfig);

            if (g_logFile == NULL)
                g_logFile = log_file_open();

            g_logLSConfig.type = log_type;
            g_logLSConfig.dac_on = buf[payload + 1];
            g_logLSConfig.reserved = buf[payload + 2];
            g_logLSConfig.level_led = buf[payload + 3];
            g_logLSConfig.gain = buf[payload + 4];
            g_logLSConfig.log_size = buf[payload + 5];
            fprintf(g_logFile, "[\"lightsensor_config\",[\"dac_on\",%u],"
                    "[\"level_led\",%u],[\"gain\",%u],[\"log_size\",%u]]\n", g_logLSConfig.dac_on,
                    g_logLSConfig.level_led, g_logLSConfig.gain, g_logLSConfig.log_size);
            break;
        case WED_LOG_LS_DATA:
            logLSData.type = log_type;
            if (g_flat_ver < FW_VERSION(1,8,84))
            {
                packet_len = 3 + (sizeof(uint16) * (((WEDLogLSData*)&buf[payload])->val[0] >> 14));
                val16 = att_get_u16(&buf[payload + 1]);
                field_count = ((val16 & 0xC000) >> 14) + 1;
                if (field_count > 3) {
                    fprintf(stderr, "Invalid LS_DATA ignored\n");
                    break;
                }
                logLSData.val[0] = val16 & 0x3FFF;
                for (i = 1; i < field_count; ++i)
                    logLSData.val[i] = att_get_u16(&buf[payload + 1 + i * 2]);
                val16 = 0;
                switch(field_count)
                {
                case 1:
                    val16 = 2;
                    break;
                case 2:
                    val16 = 6;
                    break;
                case 3:
                    val16 = 7;
                    break;
                }
            } else {
                packet_len = WEDLogLSDataSize(&buf[payload]);
                val16 = (buf[payload] & 0xE0) >> 5;
                field_count = (val16 & 1 ? 1 : 0) + (val16 & 2 ? 1 : 0) + (val16 & 4 ? 1 : 0);
                for (i = 0; i < field_count; ++i)
                    logLSData.val[i] = att_get_u16(&buf[payload + 1 + i * 2]);
            }

            if (g_logFile == NULL)
                g_logFile = log_file_open();

            if (field_count)
            {
                int cnt = 0;
                fprintf(g_logFile, "[\"lightsensor\"");
                if (val16 & 1 ? 1 : 0)
                    fprintf(g_logFile, ",[\"red\",%u]", logLSData.val[cnt++]);
                if (val16 & 2 ? 1 : 0)
                    fprintf(g_logFile, ",[\"ir\",%u]", logLSData.val[cnt++]);
                if (val16 & 4 ? 1 : 0)
                    fprintf(g_logFile, ",[\"off\",%u]", logLSData.val[cnt++]);
                fprintf(g_logFile, "]\n");
            }

            break;
        case WED_LOG_TEMP:
            packet_len = sizeof(logTemp);

            if (g_logFile == NULL)
                g_logFile = log_file_open();

            logTemp.type = log_type;
            logTemp.temperature = att_get_u16(&buf[payload + 1]);
            fprintf(g_logFile, "[\"temperature\",%d]\n", logTemp.temperature);
            break;
        case WED_LOG_TAG:
            packet_len = sizeof(g_logTag);

            if (g_logFile == NULL)
                g_logFile = log_file_open();

            g_logTag.type = log_type;
            g_logTag.tag = att_get_u32(&buf[payload + 1]);
            fprintf(g_logFile, "[\"tag\",%u]\n", g_logTag.tag);
            break;
        case WED_LOG_ACCEL_CMP:
            packet_len = WEDLogAccelCmpSize(&buf[payload]);

            logAccelCmp.type = log_type;
            logAccelCmp.count_bits = buf[payload + 1];
            field_count = (logAccelCmp.count_bits & 0xF) + 1;
            g_read_logs += field_count;
            nbits = -1;
            switch ((logAccelCmp.count_bits & 0x70) >> 4) {
            case WED_LOG_ACCEL_CMP_3_BIT:
                nbits = 3;
                break;
            case WED_LOG_ACCEL_CMP_4_BIT:
                nbits = 4;
                break;
            case WED_LOG_ACCEL_CMP_5_BIT:
                nbits = 5;
                break;
            case WED_LOG_ACCEL_CMP_6_BIT:
                nbits = 6;
                break;
            case WED_LOG_ACCEL_CMP_8_BIT:
                nbits = 8;
                g_bValidAccel = 1;
                break;
            case WED_LOG_ACCEL_CMP_STILL:
                if (logAccelCmp.count_bits & 0x80)
                    nbits = 0;
                break;
            default:
                break;
            }
            if (nbits < 0) {
                fprintf(stderr, "Invalid ACCEL_CMP ignored\n");
                break;
            }


            // We must first get at least one uncompressed accel log
            if (!g_bValidAccel)
                break;

            if (g_logFile == NULL)
                g_logFile = log_file_open();

            if (nbits == 0) {
            	// It is still, just replicate
                while (field_count--) {
                    fprintf(g_logFile, "[\"accelerometer\",[%d,%d,%d]]\n", g_logAccel.accel[0],
                            g_logAccel.accel[1], g_logAccel.accel[2]);
                }
                break;
            }
            pdu = &buf[payload];
            pdu += 2;

            if (nbits == 8) {
                while (field_count--) {
                    for (i = 0; i < 3; ++i)
                        g_logAccel.accel[i] = pdu[i];
                    pdu += 3;
                    fprintf(g_logFile, "[\"accelerometer\",[%d,%d,%d]]\n", g_logAccel.accel[0],
                            g_logAccel.accel[1], g_logAccel.accel[2]);
                }
            } else {
                GetBits gb;
                cmpGetBitsInit(&gb, pdu);
                while (field_count--) {
                    uint8 i;
                    for (i = 0; i < 3; i++) {
                        int8 diff = cmpGetBits(&gb, nbits);
                        g_logAccel.accel[i] += diff;
                    }
                    fprintf(g_logFile, "[\"accelerometer\",[%d,%d,%d]]\n", g_logAccel.accel[0],
                            g_logAccel.accel[1], g_logAccel.accel[2]);
                }
            }

            break;
        default:
            packet_len = (buflen - payload);

            printf("Notification handle: 0x%04x total: %u value: ", handle, (uint32_t)buflen);
            for (i = payload; i < buflen; ++i)
                printf("%02x ", buf[i]);
            printf("\n");
            break;
        }

        // Move forward within aggregate packet
        payload += packet_len;

        //printf("Type: %u, len: %u, total: %u\n", log_type, packet_len, buflen);
        if (payload < buflen)
            log_type = buf[payload] & 0x0F;
    } // end while (payload < buflen

    printf("\rdownloading ... %u out of %u  (%2.0f%%)", g_read_logs,
            g_total_logs, (100.0 * g_read_logs) / g_total_logs);
    fflush(stdout);
    if (g_read_logs >= g_total_logs || g_status.num_log_entries == 0)
    {
        printf("\n%s", g_szFullName);
        g_state = STATE_COUNT; // Done with command
    }
    return 0;
}

// Process the requested command
int process_command() {
    switch (g_cmd) {
    case AMIIGO_CMD_DOWNLOAD:
        return exec_download();
        break;
    case AMIIGO_CMD_CONFIGLS:
        return exec_configls();
        break;
    case AMIIGO_CMD_CONFIGACCEL:
        return exec_configaccel();
        break;
    case AMIIGO_CMD_BLINK:
        return exec_blink();
        break;
    case AMIIGO_CMD_RESET_CPU:
    case AMIIGO_CMD_RESET_LOGS:
    case AMIIGO_CMD_RESET_CONFIGS:
        return exec_reset(g_cmd);
        break;
    case AMIIGO_CMD_FWUPDATE:
        return exec_fwupdate();
        break;
    default:
        return 0;
        break;
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

// State machine to get necessary information.
// Discover device current status, and running firmware
int discover_device() {
    // TODO: this should go to a stack state-machine instead

    uint16_t handle;

    enum DISCOVERY_STATE new_state = STATE_NONE;
    switch (g_state) {
    case STATE_NONE:
        handle = g_char[AMIIGO_UUID_BUILD].value_handle;
        new_state = STATE_BUILD;
        if (handle == 0) {
            // Ignore information
            g_state = new_state;
            return discover_device();
        }
        break;
    case STATE_BUILD:
        handle = g_char[AMIIGO_UUID_VERSION].value_handle;
        new_state = STATE_VERSION;
        if (handle == 0) {
            // Ignore information
            g_state = new_state;
            return discover_device();
        }
        break;
    case STATE_VERSION:
        handle = g_char[AMIIGO_UUID_STATUS].value_handle;
        new_state = STATE_STATUS;
        if (handle == 0) {
            printf("No device status to proceed!\n");
            return -1; // Not ready yet
        }
        break;
    default:
        return 0; // already handled
        break;
    }
    g_state = new_state;

    // Read the handle of interest
    int ret = exec_read(handle);
    return ret;
}

// Set device status
int process_status(uint8_t * buf, ssize_t buflen) {
    uint8_t * pdu = &buf[1];
    g_status.num_log_entries = att_get_u32(&pdu[0]);
    g_status.battery_level = pdu[4];
    g_status.status = pdu[5];
    g_status.cur_time = att_get_u32(&pdu[6]);
    memcpy(&g_status.cur_tag, &pdu[10], WED_TAG_SIZE);
    if (buflen >= sizeof(WEDStatus) + 1)
        g_status.reboot_count = pdu[10 + WED_TAG_SIZE];


    printf("\nStatus: Build: %s\t Version: %s \n\t Logs: %u\t Battery: %u%%\t Time: %3.3f s\tReboots: %u\t",
            g_szBuild, g_szVersion, g_status.num_log_entries, g_status.battery_level,
            g_status.cur_time * 1.0 / WED_TIME_TICKS_PER_SEC, g_status.reboot_count);

    if (g_status.status & STATUS_UPDATE)
        printf(" (Updating) ");
    if (g_status.status & STATUS_FASTMODE)
        printf(" (Fast Mode) ");
    else if (g_status.status & STATUS_SLEEPMODE)
        printf(" (Sleep Mode) ");
    else
        printf(" (Slow Mode) ");
    if (g_status.status & STATUS_CHARGING)
        printf(" (Charging) ");
    if (g_status.status & STATUS_LS_INPROGRESS)
        printf(" (Light Capture) ");
    printf("\n");

    return 0;
}

// Process incoming raw data
int process_data(uint8_t * buf, ssize_t buflen) {
    int ret = 0;
    struct att_data_list *list = NULL;
    uint16_t handle = 0;
    uint8_t err = ATT_ECODE_IO;
    int i, j;
    switch (buf[0]) {
    case ATT_OP_ERROR:
        if (buflen > 4)
            err = buf[4];
        if (err == ATT_ECODE_ATTR_NOT_FOUND) {
            // If no battery information, we do not have status
            if (g_status.battery_level == 0)
                discover_device();
        } else {
            if (buflen > 1)
                handle = att_get_u16(&buf[1]);
            fprintf(stderr, "Error (%s) on handle (0x%4.4x)\n",
                    att_ecode2str(err), handle);
        }
        break;
    case ATT_OP_HANDLE_NOTIFY:
        // Proceed with download
        process_download(buf, buflen);
        break;
    case ATT_OP_READ_BY_TYPE_RESP:
        list = dec_read_by_type_resp((uint8_t *) buf, buflen);
        if (list == NULL)
            return -1;

        handle = 0;
        for (i = 0; i < list->num; i++) {
            uint8_t *value = list->data[i];
            bt_uuid_t uuid;

            handle = att_get_u16(value);
            if (list->len == 7) {
                bt_uuid_t uuid16 = att_get_uuid16(&value[5]);
                bt_uuid_to_uuid128(&uuid16, &uuid);
            } else {
                uuid = att_get_uuid128(&value[5]);
            }
            uint8_t properties = value[2];
            uint16_t value_handle = att_get_u16(&value[3]);
            for (j = 0; j < AMIIGO_UUID_COUNT; ++j) {
                if (bt_uuid_cmp(&uuid, &g_char[j].uuid) == 0) {
                    g_char[j].handle = handle;
                    g_char[j].properties = properties;
                    g_char[j].value_handle = value_handle;
                    break;
                }
            }
            char str_uuid[MAX_LEN_UUID_STR + 1] = { 0 };
            bt_uuid_to_string(&uuid, str_uuid, sizeof(str_uuid));
            printf(
                    "handle: 0x%04x\t properties: 0x%04x\t value handle: 0x%04x\t UUID: %s \n",
                    handle, properties, value_handle, str_uuid);
        } // end for(i = 0

        // Get the rest of handles
        if (handle != 0 && (handle + 1) < OPT_END_HANDLE) {
            discover_handles(handle + 1, OPT_END_HANDLE);
        }
        break;
    case ATT_OP_READ_RESP:
        switch (g_state) {
        case STATE_BUILD:
            strncpy(g_szBuild, (const char *) &buf[1], buflen - 1);
            // More to discover
            discover_device();
            break;
        case STATE_VERSION:
            g_ver.Major = buf[1];
            g_ver.Minor = buf[2];
            g_ver.Build = att_get_u16(&buf[3]);
            g_flat_ver = FW_VERSION(g_ver.Major, g_ver.Minor, g_ver.Build);
            sprintf(g_szVersion, "%u.%u.%u%s", g_ver.Major, g_ver.Minor, g_ver.Build,
                    g_flat_ver < FW_VERSION(1,8,89) ? " (< 1.8.89: incompatible config)" : "");
            // More to discover
            discover_device();
            break;
        case STATE_STATUS:
            process_status(buf, buflen);
            // Done with discovery, perform the command
            if (g_cmd == AMIIGO_CMD_NONE) {
                g_state = STATE_COUNT;
            } else {
                // Now that we have status (e.g. number of logs)
                //  Start execution of the requested command
                ret = process_command();
            }
            break;
        case STATE_DOWNLOAD:
            // This must be keep-alive
            process_status(buf, buflen);
            break;
        case STATE_FWSTATUS:
        case STATE_FWSTATUS_WAIT:
            // Act upon new firmware update status
            ret = process_fwstatus(buf, buflen);
            break;
        default:
            dump_buffer(buf, buflen);
            break;
        }
        break;
    default:
        dump_buffer(buf, buflen);
        break;
    }

    if (list != NULL)
        att_data_list_free(list);

    return ret;
}

int set_command(const char * szName) {
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

int set_adapter(const char * szName) {
    strcpy(g_src, szName);
    return 0;
}

int set_device(const char * szName) {
    strcpy(g_dst, szName);
    return 0;
}

int set_update_file(const char * szName) {
    g_fwImageFile = fopen(szName, "r");
    if (g_fwImageFile == NULL) {
        fprintf(stderr, "Firmware image file (%s) not accessible!\n", szName);
        return -1;
    }
    fseek(g_fwImageFile, 0, SEEK_END);
    g_fwImageSize = ftell(g_fwImageFile);
    fseek(g_fwImageFile, 0, SEEK_SET);
    if (g_fwImageSize < WED_FW_HEADER_SIZE) {
        fprintf(stderr, "Firmware image file (%s) too small!\n", szName);
        return -1;
    }
    WEDFirmwareCommand fwcmd;
    memset(&fwcmd, 0, sizeof(fwcmd));
    fwcmd.pkt_type = WED_FIRMWARE_INIT;
    fread(fwcmd.header, WED_FW_HEADER_SIZE, 1, g_fwImageFile);

    uint16_t * hdr = (uint16_t *) &fwcmd.header[0];
    // TODO: check CRC
    //uint16_t fw_crc = hdr[0];
    uint16_t fw_id = hdr[1];
    g_fwImagePage = hdr[2];

    if (fw_id != FWUP_HDR_ID) {
        fprintf(stderr, "Firmware image (%s) invalid!\n", szName);
        return -1;
    }

    if (WED_FW_BLOCK_SIZE * WED_FW_STREAM_BLOCKS * g_fwImagePage != g_fwImageSize) {
        fprintf(stderr, "Firmware image (%s) invalid size!\n", szName);
        return -1;
    }

    g_fwImageWrittenSize = 0;
    g_fwImageWrittenPage = 0;
    fseek(g_fwImageFile, 0, SEEK_SET);

    g_cmd = AMIIGO_CMD_FWUPDATE;
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

int set_input_file(const char * szName) {
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

void show_usage_screen(void) {
    printf("Amiigo Link command line utility.\n");
    printf("Usage: amlink [options] [command]\n"
            "Options:\n"
            "  -v, --verbose Verbose mode \n"
            "    More messages are dumped to console.\n"
            "  --full Full characteristics discovery. \n"
            "    If specified handles are queried.\n"
            "  --i, --adapter uuid|hci<N>\n"
            "    Interface adapter to use (default is hci0)\n"
            "  --b, --device uuid \n"
            "    Amiigo device to connect to.\n"
            "    Can specify a UUID\n"
            "    Example: --b 90:59:AF:04:32:82\n"
            "    Use --lescan to find the UUID list\n"
            "  --lescan, -l \n"
            "    Low energy scan (needs root priviledge)\n"
            "  --command cmd \n"
            "    Command to execute:\n"
            "       status: (default) perform device discovery\n"
            "       download: download the logs\n"
            "       configls: configure light sensor\n"
            "       configaccel: configure acceleration sensors\n"
            "       blink: configure blink LED\n"
            "       resetlogs: reset buffered logs\n"
            "       resetcpu: restart the board\n"
            "       resetconfigs: set all configs to default\n"
            "  --input file\n"
            "    Configuration file to use for given command.\n"
            "  --fwupdate file\n"
            "    Firmware image file to to use for update.\n"
            "  --help         Display this usage screen\n");
    printf("amlink is Copyright Amiigo inc\n");
}

static void do_command_line(int argc, char * const argv[]) {
    // Parse the input
    while (1) {
        int c;
        int option_index = 0;
        static struct option long_options[] = {
              { "verbose", 0, 0, 'v' },
              { "full", 0, 0, 'a' },
              { "lescan", 0, 0, 'l'  },
              { "i", 1, 0, 'i' },
              { "adapter", 1, 0, 'i' },
              { "b", 1, 0, 'b' },
              { "device", 1, 0, 'b' },
              { "c", 1, 0, 'x' },
              { "command", 1, 0, 'x' },
              { "input", 1, 0, 'f' },
              { "fwupdate", 1, 0, 'u' },
              { "help", 0, 0, '?' },
              { 0, 0, 0, 0 } };

        c = getopt_long(argc, argv, "vl?", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 0:
            printf("option %s", long_options[option_index].name);
            if (optarg)
                printf(" with arg %s", optarg);
            printf("unsupported\n");
            break;

        case 'l':
            do_lescan();
            exit(0);
            break;

        case 'v':
            // TODO: implement
            break;

        case 'a':
            g_bFull = 1;
            break;

        case 'i':
            if (set_adapter(optarg))
                exit(1);
            break;

        case 'b':
            if (set_device(optarg))
                exit(1);
            break;

        case 'x':
            if (set_command(optarg))
                exit(1);
            break;

        case 'f':
            if (set_input_file(optarg))
                exit(1);
            break;

        case 'u':
            if (set_update_file(optarg))
                exit(1);
            break;

        case '?':
            show_usage_screen();
            exit(0);
            break;

        default:
            printf("?? getopt returned character code 0%o ??\n", c);
            break;
        }
    }

    if (optind < argc) {
        printf("Unrecognized command line arguments: ");
        while (optind < argc)
            printf("%s ", argv[optind++]);
        printf("\n");
        show_usage_screen();
        exit(1);
    }
}

char kbhit() {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO);
    newt.c_cc[VMIN] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int nread = read(STDIN_FILENO, &ch, 1);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    if (nread == 1)
        return ch;
    else
        return 0;
}

int main(int argc, char **argv) {
    int ret;
    time_t start_time, stop_time, download_time;

    memset(g_char, 0, sizeof(g_char));
    int i;
    for (i = 0; i < AMIIGO_UUID_COUNT; ++i)
        bt_string_to_uuid(&g_char[i].uuid, g_szUUID[i]);
    struct set_opts opts;
    memset(&opts, 0, sizeof(opts));

    memset(&g_config_ls, 0, sizeof(g_config_ls));
    memset(&g_config_accel, 0, sizeof(g_config_accel));
    memset(&g_maint_led, 0, sizeof(g_maint_led));
    memset(&g_status, 0, sizeof(g_status));
    memset(&g_logTag, 0, sizeof(g_logTag));
    memset(&g_logTime, 0, sizeof(g_logTime));
    memset(&g_logAccel, 0, sizeof(g_logAccel));
    memset(&g_logLSConfig, 0, sizeof(g_logLSConfig));
    memset(&g_ver, 0, sizeof(g_ver));

    // Some defulats
    g_maint_led.duration = 5;
    g_maint_led.led = 6;
    g_maint_led.speed = 1;

    // Set default handles
    g_char[AMIIGO_UUID_STATUS].value_handle = 0x0025;
    g_char[AMIIGO_UUID_CONFIG].value_handle = 0x0027;
    g_char[AMIIGO_UUID_LOGBLOCK].value_handle = 0x0029;
    g_char[AMIIGO_UUID_FIRMWARE].value_handle = 0x002c;
    g_char[AMIIGO_UUID_DEBUG].value_handle = 0x002e;
    g_char[AMIIGO_UUID_BUILD].value_handle = 0x0030;
    g_char[AMIIGO_UUID_VERSION].value_handle = 0x0032;

    // Set parameters based on command line
    do_command_line(argc, argv);

    if (strlen(g_dst) == 0) {
        fprintf(stderr,
                "Device address must be specified (use --help to find the usage)\n");
        return -1;
    }
    if (str2ba(g_dst, &opts.dst)) {
        fprintf(stderr,
                "Invalid device address (use --help to find the usage)!\n");
        return -1;
    }

    if (g_src != NULL) {
        if (!strncmp(g_src, "hci", 3))
            ret = hci_devba(atoi(g_src + 3), &opts.src);
        else
            ret = str2ba(g_src, &opts.src);
        if (ret) {
            fprintf(stderr,
                    "Invalid interface (use --help to find the usage)!\n");
            return -1;
        }
    } else {
        bacpy(&opts.src, BDADDR_ANY);
    }
    opts.type = BT_IO_L2CAP;
    opts.src_type = BDADDR_LE_PUBLIC;
    opts.dst_type = BDADDR_LE_PUBLIC;
    opts.sec_level = BT_SECURITY_LOW;
    opts.master = -1;
    opts.flushable = -1;
    opts.mode = L2CAP_MODE_BASIC;
    opts.priority = 0;
    opts.cid = ATT_CID;

    g_sock = bt_io_connect(&opts);
    if (g_sock <= 0) {
        fprintf(stderr, "bt_io_connect (%d)\n", errno);
        return -1;
    }
    {
        // Increase socket's receive buffer
        int opt_rcvbuf = (2 * 1024 * 1024);
        ret = setsockopt(g_sock, SOL_SOCKET, SO_RCVBUF, (char *) &opt_rcvbuf, sizeof(int));
        if (ret) {
            fprintf(stderr, "setsockopt SO_RCVBUF (%d)\n", errno);
            return -1;
        }
        // Increase socket's send buffer
        int opt_sndbuf = (4 * 1024 * 1024);
        ret = setsockopt(g_sock, SOL_SOCKET, SO_SNDBUF, (char *) &opt_sndbuf, sizeof(int));
        if (ret) {
            fprintf(stderr, "setsockopt SO_SNDBUF (%d)\n", errno);
            return -1;
        }
    }

    int ready;
    struct timeval tv;
    fd_set write_fds, read_fds;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    FD_ZERO(&write_fds);
    FD_SET(g_sock, &write_fds);

    ready = select(g_sock + 1, NULL, &write_fds, NULL, &tv);
    if (!(ready > 0 && FD_ISSET(g_sock, &write_fds))) {
        fprintf(stderr, "Connection time out %d error (%d)\n", ready, errno);
        return -1;
    }
    {
        int soerr = 0;
        socklen_t olen = sizeof(soerr);
        if (getsockopt(g_sock, SOL_SOCKET , SO_ERROR, &soerr, &olen) < 0)
        {
            fprintf(stderr, "getsockopt() time out error (%d)\n", errno);
            return -1;
        }
        if (soerr)
        {
            fprintf(stderr, "Socket error %d error (%d)\n", soerr, errno);
            return -1;
        }
        // Clear errno
        errno = 0;
    }

    // Get the CID and MTU information
    bt_io_get(g_sock, BT_IO_OPT_OMTU, &opts.omtu, BT_IO_OPT_IMTU, &opts.imtu,
            BT_IO_OPT_CID, &opts.cid, BT_IO_OPT_INVALID);

    g_buflen = (opts.cid == ATT_CID) ? ATT_DEFAULT_LE_MTU : opts.imtu;

    printf("Session started ('q' to quit):\n\t"
            " SRC: %s OMTU: %d IMTU %d CID %d\n\n", g_src, opts.omtu, opts.imtu,
            opts.cid);

    if (g_bFull) {
        // Start by discovering Amiigo handles
        ret = discover_handles(OPT_START_HANDLE, OPT_END_HANDLE);
        if (ret) {
            fprintf(stderr, "discover_handles() error\n");
            return -1;
        }
    } else {
        // Use default handles and discover the device
        ret = discover_device();
        if (ret) {
            fprintf(stderr, "discover_device() error\n");
            return -1;
        }
    }

    tv.tv_sec = 0;
    tv.tv_usec = 10000;
    FD_ZERO(&read_fds);
    FD_SET(g_sock, &read_fds);

    start_time = time(NULL);
    stop_time = start_time;
    download_time = start_time;

    for (;;) {
        // No need to keep-alive during firmware update
        if (g_state != STATE_FWSTATUS_WAIT) {
            stop_time = time(NULL);
            double diff = difftime(stop_time, start_time);
            // Keep-alive by readin status every 60s
            if (diff > 60) {
                printf(" (Keep alive)\n");
                start_time = stop_time;
                exec_read(g_char[AMIIGO_UUID_STATUS].value_handle);
            }
            if (g_state == STATE_DOWNLOAD) {
                diff = difftime(stop_time, download_time);
                // Download timeout reached
                if (diff > 2)
                {
                    printf(" (Timeout)\n");
                    break;
                }
            }
        }

        FD_ZERO(&read_fds);
        FD_SET(g_sock, &read_fds);
        ready = select(g_sock + 1, &read_fds, NULL, NULL, &tv);
        if (ready == -1) {
            fprintf(stderr, "main select() error\n");
            break;
        }
        uint8_t buf[1024] = {0};
        ssize_t len = 0, buflen = 0;
        if (ready > 0 && FD_ISSET(g_sock, &read_fds)) {
            len = recv(g_sock, &buf[len], sizeof(buf), 0);
            if (len < 0) {
                if (errno == EAGAIN) {
                    len = 0;
                } else {
                    fprintf(stderr, "main recv() error (%d)\n", errno);
                    len = -1;
                    break;
                }
            }
            if (len > 0)
                buflen = buflen + len;
            // If error in receive, quit
            if (len < 0)
                break;
            if (buflen == 0)
                continue;
            download_time = stop_time;
            // Process incoming data
            ret = process_data(buf, buflen);
            if (ret) {
                fprintf(stderr, "main process_data() error\n");
                break;
            }
            if (g_state == STATE_COUNT)
                break; // Done the the command
        }
        if (kbhit() == 'q')
            break;
    } //end for(;;

    // Reset CPU if need to exit in the middle of firmware update
    if (g_state == STATE_FWSTATUS_WAIT)
        exec_reset(AMIIGO_CMD_RESET_CPU);

    printf("\n");

    // Close socket
    shutdown(g_sock, SHUT_RDWR);
    close(g_sock);

    // Close log files
    if (g_logFile != NULL)
    {
        fclose(g_logFile);
        g_logFile = NULL;
    }

    // Close fw iamge file
    if (g_fwImageFile != NULL) {
        fclose(g_fwImageFile);
        g_fwImageFile = NULL;
    }

    return 0;
}
