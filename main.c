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

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "btio.h"
#include "att.h"
#include "hcitool.h"
#include "amidefs.h"

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
//char g_dst[512] = "90:59:AF:04:32:F0";
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
const char * g_szUUID[]= {
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
	AMIIGO_CMD_RESET_LOGS,    // Reset log buffer
	AMIIGO_CMD_RESET_CPU,     // Reset CPU
	AMIIGO_CMD_RESET_CONFIGS, // Reset configurations to default
	AMIIGO_CMD_DOWNLOAD,      // Download all the logs
	AMIIGO_CMD_CONFIGLS,      // Configure light sensors
	AMIIGO_CMD_CONFIGACCEL,   // Configure acceleration sensors
} g_cmd = AMIIGO_CMD_NONE;

WEDVersion g_ver; // Firmware version
WEDConfigLS g_config_ls; // Light configuration
WEDConfigAccel g_config_accel; // Acceleration sensors configuration

WEDStatus g_status; // Device status

// Soem log packets to keep their state
WEDLogTag g_logTag;
WEDLogTimestamp g_logTime;
WEDLogAccel g_logAccel;
WEDLogLSConfig g_logLSConfig;

int g_sock; // Link socket
size_t g_buflen; // Uplink MTU
uint32_t g_total_logs = 0; // Total number of logs

#define MAX_LOG_ENTRIES (WED_LOG_ACCEL_CMP + 1)
FILE * g_logFile[MAX_LOG_ENTRIES] = {NULL};

enum DISCOVERY_STATE {
	STATE_NONE = 0,
	STATE_BUILD,
	STATE_VERSION,
	STATE_STATUS,
	STATE_DOWNLOAD,

	STATE_COUNT, // This must be the last
} g_state = STATE_NONE;

// Dump content of the buffer
int dump_buffer(uint8_t * buf, ssize_t buflen)
{
	int i;
	printf("Unhandled (%s): \t", att_op2str(buf[0]));
	for (i = 0; i < buflen; ++i)
		printf("%02x ", buf[i]);
	printf("\n");

	return 0;
}

// Read a characteristics
// Inputs:
//   handle - characteristics handle to read from
int exec_read(uint16_t handle)
{
	if (handle == 0)
		return -1;
	uint16_t plen;
	uint8_t * buf = malloc(g_buflen);
    memset(buf, 0, g_buflen);

	plen = enc_read_req(handle, buf, g_buflen);

	ssize_t len = send(g_sock, buf, plen, 0);

	free(buf);
	if (len < 0 || len != plen)
	{
		return -1;
	}
	return 0;
}

// Start downloading the log packets
int exec_download()
{
	g_state = STATE_DOWNLOAD; // Download in progress

	uint8_t * buf = malloc(g_buflen);
    memset(buf, 0, g_buflen);

	uint16_t plen;
	uint16_t handle = g_char[AMIIGO_UUID_CONFIG].value_handle;
	if (handle == 0)
		return -1; // Not ready yet
	printf("\n\n");
	 // Config for notify
	WEDConfig config;
	config.config_type = WED_CFG_LOG;
	config.log.flags = WED_CONFIG_LOG_DL_EN | WED_CONFIG_LOG_CMP_EN;

	plen = enc_write_cmd(handle, (uint8_t *)&config, sizeof(config), buf, g_buflen);

	ssize_t len = send(g_sock, buf, plen, 0);

	free(buf);
	if (len < 0 || len != plen)
	{
		return -1;
	}
	return 0;
}

// Start configuration of light sensors
int exec_configls()
{
	g_state = STATE_COUNT; // Done with command

	uint8_t * buf = malloc(g_buflen);
    memset(buf, 0, g_buflen);

	uint16_t plen;
	uint16_t handle = g_char[AMIIGO_UUID_CONFIG].value_handle;
	if (handle == 0)
		return -1; // Not ready yet
	WEDConfig config;
	config.config_type = WED_CFG_LS;
	config.lightsensor = g_config_ls;

	plen = enc_write_cmd(handle, (uint8_t *)&config, sizeof(config), buf, g_buflen);

	ssize_t len = send(g_sock, buf, plen, 0);

	free(buf);
	if (len < 0 || len != plen)
	{
		return -1;
	}
	return 0;
}

// Start configuration of accel sensors
int exec_configaccel()
{
	g_state = STATE_COUNT; // Done with command

	uint8_t * buf = malloc(g_buflen);
    memset(buf, 0, g_buflen);

	uint16_t plen;
	uint16_t handle = g_char[AMIIGO_UUID_CONFIG].value_handle;
	if (handle == 0)
		return -1; // Not ready yet
	WEDConfig config;
	config.config_type = WED_CFG_ACCEL;
	config.accel = g_config_accel;

	plen = enc_write_cmd(handle, (uint8_t *)&config, sizeof(config), buf, g_buflen);

	ssize_t len = send(g_sock, buf, plen, 0);

	free(buf);
	if (len < 0 || len != plen)
	{
		return -1;
	}
	return 0;
}

// Reset config, or CPU or log buffer
int exec_reset()
{
	g_state = STATE_COUNT; // Done with command

	uint8_t * buf = malloc(g_buflen);
    memset(buf, 0, g_buflen);

	uint16_t plen;
	uint16_t handle = g_char[AMIIGO_UUID_CONFIG].value_handle;
	if (handle == 0)
		return -1; // Not ready yet
	WEDConfigMaint config_maint;
	memset(&config_maint, 0, sizeof(config_maint));
	switch (g_cmd)
	{
	case AMIIGO_CMD_RESET_CPU:
		printf("Restarting the device ...\n");
		config_maint.command = WED_MAINT_RESET;
		break;
	case AMIIGO_CMD_RESET_LOGS:
		printf("Clearing the logs ...\n");
		config_maint.command = WED_MAINT_CLEAR_LOG;
		break;
	case AMIIGO_CMD_RESET_CONFIGS:
		printf("Resetign the configurations to default ...\n");
		config_maint.command = WED_MAINT_RESET_CONFIG;
		break;
	default:
		return -1;
		break;
	}
	WEDConfig config;
	config.config_type = WED_CFG_MAINT;
	config.maint = config_maint;

	plen = enc_write_cmd(handle, (uint8_t *)&config, sizeof(config), buf, g_buflen);

	ssize_t len = send(g_sock, buf, plen, 0);

	free(buf);
	if (len < 0 || len != plen)
	{
		return -1;
	}
	return 0;
}

// Continue downloading packets
int process_download(uint8_t * buf, ssize_t buflen)
{
	int i;
	uint16_t handle = 0;

	handle = att_get_u16(&buf[1]);
	if (buflen < 4)
	{
		// TODO: Collin: do I need to turn off notification first?
		printf("Last notification handle = 0x%04x\n", handle);
		g_state = STATE_COUNT;
		return 0;
	}

	// Note: Each packet starts a log entry

	WEDLogAccelCmp logAccelCmp;
	WEDLogLSData logLSData;
	WEDLogTemp logTemp;

	// TODO: use date-time to avoid overwriting logs
	// TODO: check packet sizes
	// TODO: check data integrity

	int packet_len;
	int payload = 3; // Payload starting position
	int seq_number = buf[payload] >> 4;

	// TODO: use seq_number for reordering packets

	WED_LOG_TYPE log_type = buf[payload] & 0x0F;
	while (payload < buflen)
	{
		g_total_logs++; // Total number of log points downloaded so far
		switch(log_type)
		{
		uint16_t val16;
		uint8_t field_count, nbits;
		uint8_t * pdu;

		case WED_LOG_TIME:

			// TODO: use this to split log files

			packet_len = sizeof(g_logTime);
			g_logTime.type = log_type;
			g_logTime.timestamp = att_get_u32(&buf[payload + 1]);
			g_logTime.fast_rate = buf[payload + 5];

			// Move forward
			payload += packet_len;
			break;
		case WED_LOG_ACCEL:
			if (g_logFile[log_type] == NULL)
			{
				g_logFile[log_type] = fopen("./Accel.log", "w");
				fprintf(g_logFile[log_type], "x,y,z\n");
			}

			packet_len = sizeof(g_logAccel);
			g_logAccel.type = log_type;
			for (i = 0; i < 3; ++i)
				g_logAccel.accel[i] = buf[payload + 1 + i];
			fprintf(g_logFile[log_type], "%d,%d,%d\n", g_logAccel.accel[0], g_logAccel.accel[1], g_logAccel.accel[2]);
			// Move forward
			payload += packet_len;
			break;
		case WED_LOG_LS_CONFIG:
			if (g_logFile[log_type] == NULL)
			{
				g_logFile[log_type] = fopen("./LS_Config.log", "w");
				fprintf(g_logFile[log_type], "dac_red,dac_ir,level_red,level_ir\n");
			}

			packet_len = sizeof(g_logLSConfig);
			g_logLSConfig.type = log_type;
			g_logLSConfig.dac_red = buf[payload + 1];
			g_logLSConfig.dac_ir = buf[payload + 2];
			g_logLSConfig.level_red = buf[payload + 3];
			g_logLSConfig.level_ir = buf[payload + 4];
			fprintf(g_logFile[log_type], "%u,%u,%u,%u\n",
					g_logLSConfig.dac_red, g_logLSConfig.dac_ir, g_logLSConfig.level_red, g_logLSConfig.level_ir);
			// Move forward
			payload += packet_len;
			break;
		case WED_LOG_LS_DATA:
			if (g_logFile[log_type] == NULL)
			{
				g_logFile[log_type] = fopen("./LS_Data.log", "w");
			}

			packet_len = WEDLogLSDataSize(&buf[payload]);
			logLSData.type = log_type;
			val16 = att_get_u16(&buf[payload + 1]);
			field_count = ((val16 & 0xC000) >> 14) + 1;
			if (field_count > 3)
			{
				fprintf(stderr, "Invalid LS_DATA ignored\n");
				break;
			}
			logLSData.val[0] = val16 & 0x3FFF;
			for (i = 1; i < field_count; ++i)
				logLSData.val[i] = att_get_u16(&buf[payload + 1 + i * 2]);

			for (i = 0; i < field_count; ++i)
			{
				fprintf(g_logFile[log_type], "%u", logLSData.val[i]);
				if (i < field_count)
					fprintf(g_logFile[log_type], ",");
			}
			fprintf(g_logFile[log_type], "\n");
			// Move forward
			payload += packet_len;
			break;
		case WED_LOG_TEMP:
			if (g_logFile[log_type] == NULL)
				g_logFile[log_type] = fopen("./Temp.log", "w");

			packet_len = sizeof(logTemp);
			logTemp.type = log_type;
			logTemp.temperature = att_get_u16(&buf[payload + 1]);
			fprintf(g_logFile[log_type], "%d\n", logTemp.temperature);
			// Move forward
			payload += packet_len;
			break;
		case WED_LOG_TAG:
			if (g_logFile[log_type] == NULL)
				g_logFile[log_type] = fopen("./Tag.log", "w");

			packet_len = sizeof(g_logTag);
			g_logTag.type = log_type;
			for (i = 0; i < 4; ++i)
				g_logTag.tag[i] = buf[payload + 1 + i];
			for (i = 0; i < 4; ++i)
			{
				fprintf(g_logFile[log_type], "%u", g_logTag.tag[i]);
				if (i < 4)
					fprintf(g_logFile[log_type], ",");
			}
			fprintf(g_logFile[log_type], "\n");
			// Move forward
			payload += packet_len;
			break;
		case WED_LOG_ACCEL_CMP:
			packet_len = WEDLogAccelCmpSize(&buf[payload]);
			payload += packet_len;

			logAccelCmp.type = log_type;
			logAccelCmp.count_bits = buf[payload + 1];
			field_count = (logAccelCmp.count_bits & 0xF) + 1;
			nbits = 0;
			switch ((logAccelCmp.count_bits & 0x70) >> 4)
			{
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
				// This is also uncompressed data
				if (g_logFile[WED_LOG_ACCEL] == NULL)
				{
					g_logFile[WED_LOG_ACCEL] = fopen("./Accel.log", "w");
					fprintf(g_logFile[WED_LOG_ACCEL], "x,y,z\n");
				}
				break;
			default:
				break;
			}
			if (val16 == 0)
			{
				fprintf(stderr, "Invalid ACCEL_CMP ignored\n");
				break;
			}

			// We must first get at least one uncompressed accel log
			if (g_logFile[WED_LOG_ACCEL] == NULL)
				break;

			pdu = buf;
			pdu += 2;

			if (nbits == 8) {
				while (field_count--) {
					for (i = 0; i < 3; ++i)
						g_logAccel.accel[i] = pdu[i];
					pdu += 3;
					fprintf(g_logFile[WED_LOG_ACCEL], "%d,%d,%d\n", g_logAccel.accel[0], g_logAccel.accel[1], g_logAccel.accel[2]);
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
					fprintf(g_logFile[WED_LOG_ACCEL], "%d,%d,%d\n", g_logAccel.accel[0], g_logAccel.accel[1], g_logAccel.accel[2]);
				}
			}

			break;
		default:
			printf("Notification handle = 0x%04x value: ", handle);
			for (i = 3; i < buflen; ++i)
				printf("%02x ", buf[i]);
			printf("\n");
			payload = buflen;
			break;
		}

		log_type = buf[payload];
	}

	printf("\rdownloading ... %u out of %u  (%2.0f%%)", g_total_logs, g_status.num_log_entries, (100.0 * g_total_logs) / g_status.num_log_entries);
	fflush(stdout);
	if (g_total_logs >= g_status.num_log_entries)
		g_state = STATE_COUNT; // Done with command
	return 0;
}

// Process the requested command
int process_command()
{
	switch (g_cmd)
	{
	case AMIIGO_CMD_DOWNLOAD:
		return exec_download();
		break;
	case AMIIGO_CMD_CONFIGLS:
		return exec_configls();
		break;
	case AMIIGO_CMD_CONFIGACCEL:
		return exec_configaccel();
		break;
	case AMIIGO_CMD_RESET_CPU:
	case AMIIGO_CMD_RESET_LOGS:
	case AMIIGO_CMD_RESET_CONFIGS:
		return exec_reset();
		break;
	default:
		return 0;
		break;
	}
	return 0;
}

// Find handle associated with given UUID
int discover_handles(uint16_t start_handle, uint16_t end_handle)
{
	uint16_t plen;
	bt_uuid_t type_uuid;

    uint8_t * buf = malloc(g_buflen);
    memset(buf, 0, g_buflen);

	bt_uuid16_create(&type_uuid, GATT_CHARAC_UUID);

	plen = enc_read_by_type_req(start_handle, end_handle, &type_uuid, buf, g_buflen);

	ssize_t len = send(g_sock, buf, plen, 0);

	free(buf);
	if (len < 0 || len != plen)
	{
		return -1;
	}
	return 0;
}

// State machine to get necessary information.
// Discover device current status, and running firmware
int discover_device()
{
	// TODO: this should go to a stack state-machine instead

	uint16_t handle;

	enum DISCOVERY_STATE new_state = STATE_NONE;
	switch (g_state)
	{
	case STATE_NONE:
		handle = g_char[AMIIGO_UUID_BUILD].value_handle;
		new_state = STATE_BUILD;
		if (handle == 0)
		{
			// Ignore information
			g_state = new_state;
			return discover_device();
		}
		break;
	case STATE_BUILD:
		handle = g_char[AMIIGO_UUID_VERSION].value_handle;
		new_state = STATE_VERSION;
		if (handle == 0)
		{
			// Ignore information
			g_state = new_state;
			return discover_device();
		}
		break;
	case STATE_VERSION:
		handle = g_char[AMIIGO_UUID_STATUS].value_handle;
		new_state = STATE_STATUS;
		if (handle == 0)
		{
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
int process_status(uint8_t * buf, ssize_t buflen)
{
	if (buflen < sizeof(WEDStatus) + 1)
		return -1;
	uint8_t * pdu = &buf[1];
	g_status.num_log_entries = att_get_u32(&pdu[0]);
	g_status.battery_level = pdu[4];
	g_status.status = pdu[5];
	g_status.cur_time = att_get_u32(&pdu[6]);
	memcpy(&g_status.cur_tag, &pdu[10], WED_TAG_SIZE);

	printf("\nStatus: Build: %s\t Version: %s \n\t Logs: %u\t Battery: %2.1f%%\t Time: %3.3f s\t",
			g_szBuild, g_szVersion,
			g_status.num_log_entries, g_status.battery_level * 100.0 / 255.0, g_status.cur_time * 1.0 / WED_TIME_TICKS_PER_SEC);

	if (g_status.status & STATUS_UPDATE)
		printf(" (Updating) ");
	if (g_status.status & STATUS_FASTMODE)
		printf(" (Fast Mode) ");
	else
		printf(" (Slow Mode) ");
	if (g_status.status & STATUS_CHARGING)
		printf(" (Charging) ");
	printf("\n");

	return 0;
}

// Process incoming raw data
int process_data(uint8_t * buf, ssize_t buflen)
{
	int ret = 0;
	struct att_data_list *list = NULL;
	uint16_t handle = 0;
	uint8_t err = ATT_ECODE_IO;
	int i, j;
	switch (buf[0])
	{
	case ATT_OP_ERROR:
		if (buflen > 4)
			err = buf[4];
		if (err == ATT_ECODE_ATTR_NOT_FOUND)
		{
			// If no battery information, we do not have status
			if (g_status.battery_level == 0)
				discover_device();
		} else {
			if (buflen > 1)
				handle = att_get_u16(&buf[1]);
			fprintf(stderr, "Error (%s) on handle (0x%4.4x)\n", att_ecode2str(err), handle);
		}
		break;
	case ATT_OP_HANDLE_NOTIFY:
		// Proceed with download
		process_download(buf, buflen);
		break;
	case ATT_OP_READ_BY_TYPE_RESP:
		list = dec_read_by_type_resp((uint8_t *)buf, buflen);
		if (list == NULL)
			return -1;

		handle = 0;
		for (i = 0; i < list->num; i++)
		{
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
			for (j = 0; j < AMIIGO_UUID_COUNT; ++j)
			{
				if (bt_uuid_cmp(&uuid, &g_char[j].uuid) == 0)
				{
					g_char[j].handle = handle;
					g_char[j].properties = properties;
					g_char[j].value_handle = value_handle;
					break;
				}
			}
			char str_uuid[MAX_LEN_UUID_STR + 1] = {0};
			bt_uuid_to_string(&uuid, str_uuid, sizeof(str_uuid));
			printf("handle: 0x%04x\t properties: 0x%04x\t value handle: 0x%04x\t UUID: %s \n", handle, properties, value_handle, str_uuid);
		} // end for(i = 0

		// Get the rest of handles
		if (handle != 0 && (handle + 1) < OPT_END_HANDLE)
		{
			discover_handles(handle + 1, OPT_END_HANDLE);
		}
		break;
	case ATT_OP_READ_RESP:
		switch (g_state)
		{
		case STATE_BUILD:
			strncpy(g_szBuild, (const char *)&buf[1], buflen - 1);
			// More to discover
			discover_device();
			break;
		case STATE_VERSION:
			g_ver.Major = buf[1];
			g_ver.Minor = buf[2];
			g_ver.Build = att_get_u16(&buf[3]);
			sprintf(g_szVersion, "%u.%u.%u", g_ver.Major, g_ver.Minor, g_ver.Build);
			// More to discover
			discover_device();
			break;
		case STATE_STATUS:
			process_status(buf, buflen);
			// Done with discovery, perform the command
			if (g_cmd == AMIIGO_CMD_NONE)
			{
				g_state = STATE_COUNT;
			} else {
				if (g_state == STATE_NONE)
				{
					// Now that we have status (e.g. number of logs)
					//  Start execution of the requested command
					ret = process_command();
				}
			}
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

int set_command(const char * szName)
{
	if (strcasecmp(szName, "download") == 0)
	{
		g_cmd = AMIIGO_CMD_DOWNLOAD;
	}
	else if (strcasecmp(szName, "resetcpu") == 0)
	{
		g_cmd = AMIIGO_CMD_RESET_CPU;
	}
	else if (strcasecmp(szName, "resetlogs") == 0)
	{
		g_cmd = AMIIGO_CMD_RESET_LOGS;
	}
	else if (strcasecmp(szName, "resetconfigs") == 0)
	{
		g_cmd = AMIIGO_CMD_RESET_CONFIGS;
	}
	else if (strcasecmp(szName, "configls") == 0)
	{
		g_cmd = AMIIGO_CMD_CONFIGLS;
	}
	else if (strcasecmp(szName, "configaccel") == 0)
	{
		g_cmd = AMIIGO_CMD_CONFIGACCEL;
	}
	else if (strcasecmp(szName, "status") == 0)
	{
		g_cmd = AMIIGO_CMD_NONE;
	} else {
		fprintf(stderr, "Invalid command (%s)!\n", szName);
		return -1;
	}
	return 0;
}

int set_adapter(const char * szName)
{
	strcpy(g_src, szName);
	return 0;
}

int set_device(const char * szName)
{
	strcpy(g_dst, szName);
	return 0;
}

int set_input_file(const char * szName)
{
	FILE * fp = fopen(szName, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "Configuration file (%s) not accessible!\n", szName);
		return -1;
	}
	char szParam[256];
	char szVal[256];
	while (fscanf(fp, "%s %s", szParam, szVal) != EOF)
	{
		//---------- Light ----------------------------
		if (strcasecmp(szParam, "ls_on_time") == 0)
		{
			g_config_ls.on_time = (uint8_t)atoi(szVal);
		}
		else if (strcasecmp(szParam, "ls_off_time") == 0)
		{
			g_config_ls.off_time = (uint8_t)atoi(szVal);
		}
		else if (strcasecmp(szParam, "ls_fast_interval") == 0)
		{
			// Seconds between samples in fast mode
			g_config_ls.fast_interval = (uint8_t)atoi(szVal);
		}
		else if (strcasecmp(szParam, "ls_slow_interval") == 0)
		{
			// Seconds between samples in slow mode
			g_config_ls.slow_interval = (uint8_t)atoi(szVal);
		}
		else if (strcasecmp(szParam, "ls_duration") == 0)
		{
			g_config_ls.duration = (uint8_t)atoi(szVal);
		}
		else if (strcasecmp(szParam, "ls_gain") == 0)
		{
			g_config_ls.gain = (uint8_t)atoi(szVal);
		}
		else if (strcasecmp(szParam, "ls_leds") == 0)
		{
			g_config_ls.leds = (uint8_t)atoi(szVal);
		}
		else if (strcasecmp(szParam, "ls_led_drv") == 0)
		{
			g_config_ls.led_drv = (uint8_t)atoi(szVal);
		}
		else if (strcasecmp(szParam, "ls_norecal") == 0)
		{
			g_config_ls.norecal = (uint8_t)atoi(szVal);
		}
		else if (strcasecmp(szParam, "ls_debug") == 0)
		{
			g_config_ls.debug = (uint8_t)atoi(szVal);
		}
		else if (strcasecmp(szParam, "ls_samples") == 0)
		{
			g_config_ls.samples = (uint8_t)atoi(szVal);
		}
		else if (strcasecmp(szParam, "ls_dac_ref") == 0)
		{
			g_config_ls.dac_ref = (uint8_t)atoi(szVal);
		}
		else if (strcasecmp(szParam, "ls_adc_bits") == 0)
		{
			g_config_ls.adc_bits = (uint8_t)atoi(szVal);
		}
		else if (strcasecmp(szParam, "ls_adc_ref") == 0)
		{
			g_config_ls.adc_ref = (uint8_t)atoi(szVal);
		}
		// ---------------- Accel ---------------------
		else if (strcasecmp(szParam, "accel_slow_rate") == 0)
		{
			g_config_accel.slow_rate = WEDConfigRateParam((uint32_t)atoi(szVal));
		}
		else if (strcasecmp(szParam, "accel_fast_rate") == 0)
		{
			g_config_accel.fast_rate = WEDConfigRateParam((uint32_t)atoi(szVal));
		}
		else
		{
			fprintf(stderr, "Configuration parameter (%s) not recognized!\n", szName);
			fclose(fp);
			return -1;
		}
	}

	fclose(fp);

	return 0;
}

void show_usage_screen(void)
{
	printf ("\n");
    printf("Usage: amlink [options] [command]\n"
            "Options:\n"
            "  -V             Verbose mode, messages are dumped to console\n"
            "  --verbose\n"
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
            "       resetlogs: reset buffered logs\n"
            "       resetcpu: restart the board\n"
            "       resetconfigs: set all configs to default\n"
            "  --input file\n"
    		"    Input configuration file to use for given command.\n"
            "  --help         Display this usage screen\n"
            );
    printf("amlink is Copyright Amiigo inc\n");
};

static void do_command_line(int argc, char * const argv[])
{
    // Parse the input
    while (1)
    {
        int c;
        int option_index = 0;
        static struct option long_options[] =
        {
            {"verbose", 0, 0, 'V'},
            {"lescan", 0, 0, 'l'},
            {"i", 1, 0, 'i'},
            {"adapter", 1, 0, 'i'},
            {"b", 1, 0, 'b'},
            {"device", 1, 0, 'b'},
            {"command", 1, 0, 'x'},
            {"input", 1, 0, 'f'},
            {"help", 0, 0, '?'},
            {0, 0, 0, 0}
        };

        c = getopt_long(argc, argv, "Vl?", long_options, &option_index);
        if (c == -1)
            break;

        switch (c)
        {
        case 0:
            printf ("option %s", long_options[option_index].name);
            if (optarg)
                printf (" with arg %s", optarg);
            printf ("unsupported\n");
            break;

        case 'l':
        	do_lescan();
        	exit(0);
            break;

        case 'V':
        	// TODO: implement
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

        case '?':
            show_usage_screen();
            exit(0);
            break;

        default:
            printf ("?? getopt returned character code 0%o ??\n", c);
            break;
        }
    }


    if (optind < argc)
    {
        printf("Unrecognized command line arguments: ");
        while (optind < argc)
            printf ("%s ", argv[optind++]);
        printf ("\n");
        show_usage_screen();
        exit(1);
    }
}

bool kbhit()
{
	struct timeval tv;
	fd_set fds;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
	return (FD_ISSET(0, &fds));
}


char getch() {
	char buf = 0;
	struct termios old = {0};
	if (tcgetattr(0, &old) < 0)
		perror("tcgetattr()");
	old.c_lflag &= ~ICANON;
	old.c_lflag &= ~ECHO;
	old.c_cc[VMIN] = 1;
	old.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSANOW, &old) < 0)
		perror("tcsetattr ICANON");
	if (read(0, &buf, 1) < 0)
		perror ("read()");
	old.c_lflag |= ICANON;
	old.c_lflag |= ECHO;
	if (tcsetattr(0, TCSADRAIN, &old) < 0)
		perror ("tcsetattr ~ICANON");
	return (buf);
}

int main(int argc, char **argv)
{
	int ret;
	time_t start_time, stop_time;

	memset(g_char, 0, sizeof(g_char));
	int i;
	for (i = 0; i < AMIIGO_UUID_COUNT; ++i)
		bt_string_to_uuid(&g_char[i].uuid, g_szUUID[i]);
	struct set_opts opts;
	memset(&opts, 0, sizeof(opts));

	memset(&g_config_ls, 0, sizeof(g_config_ls));
	memset(&g_config_accel, 0, sizeof(g_config_accel));
	memset(&g_status, 0, sizeof(g_status));
	memset(&g_logTag, 0, sizeof(g_logTag));
	memset(&g_logTime, 0, sizeof(g_logTime));
	memset(&g_logAccel, 0, sizeof(g_logAccel));
	memset(&g_logLSConfig, 0, sizeof(g_logLSConfig));

	// Set parameters based on command line
	do_command_line(argc, argv);

	if (strlen(g_dst) == 0)
	{
		fprintf(stderr, "Device address must be specified (use --help to find the usag)\n");
		return -1;
	}
	if (str2ba(g_dst, &opts.dst))
	{
		fprintf(stderr, "Invalid device address (use --help to find the usage)!\n");
		return -1;
	}

	if (g_src != NULL)
	{
		if (!strncmp(g_src, "hci", 3))
			ret= hci_devba(atoi(g_src + 3), &opts.src);
		else
			ret = str2ba(g_src, &opts.src);
		if (ret)
		{
			fprintf(stderr, "Invalid interface (use --help to find the usage)!\n");
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
	if (g_sock < -1)
	{
		fprintf(stderr, "bt_io_connect (%d)\n", errno);
		return -1;
	}
	// Increase socket's receive buffer
	int opt_rcvbuf = (2 * 1024 * 1024);
	ret = setsockopt(g_sock, SOL_SOCKET, SO_RCVBUF, (char *)&opt_rcvbuf, sizeof(int));
	if (ret)
	{
		fprintf(stderr, "setsockopt SO_RCVBUF (%d)\n", errno);
		return -1;
	}

	int ready;
	struct timeval tv;
	fd_set write_fds, read_fds;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	FD_ZERO(&write_fds);
	FD_SET(g_sock, &write_fds);

	ready = select(g_sock + 1, NULL, &write_fds, NULL, &tv);
	if (!(ready > 0 && FD_ISSET(g_sock, &write_fds)))
	{
		fprintf(stderr, "Connection time out %d error (%d)\n", ready, errno);
		return -1;
	}

    // Get the CID and MTU information
    bt_io_get(g_sock, BT_IO_OPT_OMTU, &opts.omtu,
    		BT_IO_OPT_IMTU, &opts.imtu,
    		BT_IO_OPT_CID, &opts.cid,
    		BT_IO_OPT_INVALID);

    g_buflen = (opts.cid == ATT_CID) ? ATT_DEFAULT_LE_MTU : opts.imtu;

    printf("Session started ('q' to quit):\n\t"
           " SRC: %s OMTU: %d IMTU %d CID %d\n\n", g_src, opts.omtu, opts.imtu, opts.cid);

    // Start by discovering Amiigo handles
    ret = discover_handles(OPT_START_HANDLE, OPT_END_HANDLE);
    if (ret)
    {
		fprintf(stderr, "discover_handles() error\n");
		return -1;
    }

	tv.tv_sec = 0;
	tv.tv_usec = 10000;
	FD_ZERO(&read_fds);
	FD_SET(g_sock, &read_fds);

	start_time = time(NULL);
	uint8_t buf[1024];

    for(;;)
    {
    	stop_time = time(NULL);
    	double diff = difftime(stop_time, start_time);
    	// Keep-alive by readin status every 60s
    	if (diff > 60)
    	{
    		start_time = stop_time;
    		exec_read(g_char[AMIIGO_UUID_STATUS].value_handle);
    	}

    	FD_ZERO(&read_fds);
    	FD_SET(g_sock, &read_fds);
    	ready = select(g_sock + 1, &read_fds, NULL, NULL, &tv);
    	if (ready == -1)
    	{
    		fprintf(stderr, "main select() error\n");
    		break;
    	}
		ssize_t len = 0, buflen = 0;
    	if (ready > 0 && FD_ISSET(g_sock, &read_fds))
    	{
    		do {
				len = recv(g_sock, &buf[len], sizeof(buf), 0);
				if (len < 0)
				{
					if (errno == EAGAIN)
					{
						len = 0;
					} else {
						fprintf(stderr, "main recv() error (%d)\n", errno);
						len = -1;
						break;
					}
				}
				if (len > 0)
					buflen = buflen + len;
    		} while (len > 0);
    		// If error in receive, quit
    		if (len < 0)
    			break;
    		if (buflen == 0)
    			continue;
    		// Process incoming data
    		ret = process_data(buf, buflen);
    		if (ret)
    			fprintf(stderr, "main process_data() error\n");
    		if (g_state == STATE_COUNT)
    			break; // Done the the command
    	}
    	if (kbhit())
    		if(getch() == 'q')
    			break;
    }

    // Close soecket
	shutdown(g_sock, SHUT_RDWR);
	close(g_sock);

	// Close log files
	for (i = 0; i < MAX_LOG_ENTRIES; ++i)
	{
		if (g_logFile[i] != NULL)
		{
			fclose(g_logFile[i]);
		}
	}

	return 0;
}
