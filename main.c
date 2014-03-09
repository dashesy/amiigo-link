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
#include <string.h>

#include "jni/bluetooth.h"
#include "jni/hci.h"
#include "jni/hci_lib.h"
#if 0
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#endif

#include "btio.h"
#include "att.h"
#include "hcitool.h"
#include "amidefs.h"

#include "gapproto.h"
#include "amprocess.h"

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

// Device and interface to use
char g_dst[512] = "";
char g_src[512] = "hci0";

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
    AMIIGO_CMD_I2C_READ,      // Read i2c address and register
    AMIIGO_CMD_I2C_WRITE,     // Write to i2c address and register
} g_cmd = AMIIGO_CMD_NONE;

WEDVersion g_ver; // Firmware version
unsigned int g_flat_ver = 0; // Flat version number to compare
#define FW_VERSION(Major, Minor, Build) (Major * 100000u + Minor * 1000u + Build)

WEDDebugI2CCmd g_i2c; // i2c debugging

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

uint32_t g_read_logs = 0;  // Logs downloaded so far
uint32_t g_total_logs = 0; // Total number of logs tp be downloaded
int g_bValidAccel = 0; // If any uncompressed accel is received
int g_decompress = 1; // If packets should be decompressed
int g_raw = 0; // If should download raw logs (no compression)

FILE * g_logFile = NULL; // file to download logs

uint32_t g_fwup_speedup = 1; // How much to overload firmware update
FILE * g_fwImageFile = NULL; // Firmware image file
uint32_t g_fwImageSize = 0; // Firmware image size in bytes
uint32_t g_fwImageWrittenSize = 0; // bytes transmitted
uint16_t g_fwImagePage = 0; // Firmwate image total pages
uint16_t g_fwImageWrittenPage = 0; // pages transmitted

int g_bVerbose = 0; // verbose output
int g_live = 0; // if live output is needed
int g_console = 0; // if should print accel to console

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
    STATE_I2C,

    STATE_COUNT, // This must be the last
} g_state = STATE_NONE;


int g_bFull = 0; // If full characteristcs should be discovered

// Execute the requested command
int exec_command() {
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
    case AMIIGO_CMD_I2C_READ:
    case AMIIGO_CMD_I2C_WRITE:
        return exec_debug_i2c();
        break;
    default:
        return 0;
        break;
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
            fprintf(stderr, "No device status to proceed!\n");
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
            "  --lescan \n"
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
            "  -l, --live Live download as a stream.\n"
            "    Hit `q` to end the stream.\n"
            "  --print\n"
            "    Print accelerometer to console as well as the log file.\n"
            "  --fwupdate file\n"
            "    Firmware image file to to use for update.\n"
            "  --i2c_read address:reg\n"
            "    Read i2c address and register (debugging only).\n"
            "  --i2c_write address:reg:value\n"
            "    Write value to i2c address and register (debugging only).\n"
            "  --compressed Leave logs in compressed form.\n"
            "  --raw Download logs in raw format (no compression).\n"
            "  --help         Display this usage screen\n");
    printf("\namlink is Copyright Amiigo inc\n");
}

static void do_command_line(int argc, char * const argv[]) {
    // Parse the input
    while (1) {
        int c;
        int option_index = 0;
        static struct option long_options[] = {
              { "verbose", 0, 0, 'v' },
              { "live", 0, 0, 'l' },
              { "full", 0, 0, 'a' },
              { "compressed", 0, 0, 'p'},
              { "raw", 0, 0, 'r'},
              { "lescan", 0, 0, 's'  },
              { "i", 1, 0, 'i' },
              { "adapter", 1, 0, 'i' },
              { "b", 1, 0, 'b' },
              { "device", 1, 0, 'b' },
              { "c", 1, 0, 'x' },
              { "command", 1, 0, 'x' },
              { "input", 1, 0, 'f' },
              { "print", 0, 0, 'o' },
              { "i2c_read", 1, 0, 'd'},
              { "i2c_write", 1, 0, 'w'},
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

        case 's':
            do_lescan();
            exit(0);
            break;

        case 'o':
            g_console = 1;
            break;

        case 'l':
            g_live = 1;
            break;

        case 'v':
            g_bVerbose = 1;
            break;

        case 'a':
            g_bFull = 1;
            break;

        case 'p':
            g_decompress = 0;
            break;

        case 'r':
            g_raw = 1;
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

        case 'd':
            if (set_i2c_read(optarg))
                exit(1);
            break;

        case 'w':
            if (set_i2c_write(optarg))
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

    memset(&g_config_ls, 0, sizeof(g_config_ls));
    memset(&g_config_accel, 0, sizeof(g_config_accel));
    memset(&g_maint_led, 0, sizeof(g_maint_led));
    memset(&g_status, 0, sizeof(g_status));
    memset(&g_logTag, 0, sizeof(g_logTag));
    memset(&g_logTime, 0, sizeof(g_logTime));
    memset(&g_logAccel, 0, sizeof(g_logAccel));
    memset(&g_logLSConfig, 0, sizeof(g_logLSConfig));
    memset(&g_ver, 0, sizeof(g_ver));

    memset(&g_i2c, 0, sizeof(g_i2c));

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

    int sock; // Link socket(s)

    // Connect to all devices
    g_sock = gap_connect(g_src, g_dst);


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
                if (g_bVerbose)
                    printf(" (Keep alive)\n");
                start_time = stop_time;
                exec_read(g_char[AMIIGO_UUID_STATUS].value_handle);
            }
            if (g_state == STATE_DOWNLOAD && !g_live) {
                diff = difftime(stop_time, download_time);
                // Download timeout reached
                if (diff > 2)
                {
                    printf(" (Timeout)\n");
                    break;
                }
            }
        }

        uint8_t buf[1024] = {0};
        int len = gap_recv(g_sock, &buf[0], sizeof(buf));
        if (len < 0)
            break;
        if (len == 0)
            continue;

        download_time = stop_time;
        // Process incoming data
        ret = process_data(buf, len);
        if (ret) {
            fprintf(stderr, "main process_data() error\n");
            break;
        }
        if (g_state == STATE_COUNT)
            break; // Done the the command

        // See if user ended the run
        if (kbhit() == 'q')
            break;
    } //end for(;;

    // Reset CPU if need to exit in the middle of firmware update
    if (g_state == STATE_FWSTATUS_WAIT)
        exec_reset(AMIIGO_CMD_RESET_CPU);

    printf("\n");

    // Close the socket
    gap_shutdown(g_sock);

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
