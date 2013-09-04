#ifndef AMIDEFS_H
#define AMIDEFS_H

// iOS Compatibility
#ifdef __APPLE__
typedef UInt8 uint8;
typedef UInt16 uint16;
typedef UInt32 uint32;
typedef int8_t int8;
typedef int16_t int16;
#define PACKED __attribute__((__packed__))
#endif

#ifdef __linux__
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int8_t int8;
typedef int16_t int16;
#define PACKED __attribute__((__packed__))
#endif

#ifndef PACKED
#define PACKED
#endif


#ifdef __cplusplus
extern "C" {
#endif

// Base UUID: CCA30000-78C6-4785-9E45-0887D451317C
#define AMI_UUID(uuid) \
	0x7C, 0x31, 0x51, 0xD4, 0x87, 0x08, \
	0x45, 0x9E, \
	0x85, 0x47, \
	0xC6, 0x78, \
	LO_UINT16(uuid), HI_UINT16(uuid), \
	0xA3, 0xCC

#define AMI_ID16(id) BUILD_UINT16(id[12], id[13])

#define AMI_UUID_SIZE ATT_UUID_SIZE

#define WED_UUID_SERVICE  0x1000
#define WED_UUID_STATUS   0x0001 // Data type: WEDStatus
#define WED_UUID_CONFIG   0x0002 // Data type: WEDConfig
#define WED_UUID_LOGDATA  0x0003 // Data type: WEDLog
#define WED_UUID_FIRMWARE 0x0004 // Data type: WEDFirmwareCommand/Status
#define WED_UUID_DEBUG    0x0005 // Data type: WEDDebug
#define WED_UUID_BUILD    0x0006 // Data type: String
#define WED_UUID_VERSION  0x0007 // Data type: WEDVersion

// NOTE: Structs below are byte packed. Integers are little-endian.

// # of ticks per second for timestamps
#define WED_TIME_TICKS_PER_SEC 128

#define WED_TAG_SIZE 4

#define PING_TIMEOUT_SEC 120 // App must send any command in this interval or the device will disconnect
	                         // Suggested App sends at rate 3 or 4 times faster than this.

///////////////////////////////////////////////////////////////////////////////
// Version
typedef struct {
	uint8 Major;
	uint8 Minor;
	uint16 Build;
} PACKED WEDVersion;


///////////////////////////////////////////////////////////////////////////////
// Status

#define  STATUS_UPDATE        0x01
#define  STATUS_FASTMODE      0x02
#define  STATUS_CHARGING      0x04
#define  STATUS_LS_INPROGRESS 0x08

// Top-level struct for characteristic UUID AMI_UUID(WED_UUID_STATUS)
typedef struct {
	// Total # of stored entries
	uint32 num_log_entries;

	// A/D reading
	uint8 battery_level;

	// BIT FIELD
	// Bit 0: Is a firmware update in progress
	// Bit 1: Are we in fast mode?
	// bit 2: Are we charging.
	// bit 3: LS collection in progress.
	uint8 status; 

	// Current time in WED_TIME_TICKS_PER_SEC
	uint32 cur_time;

// last maint tag written to log
	uint8 cur_tag[WED_TAG_SIZE];
} PACKED WEDStatus;

///////////////////////////////////////////////////////////////////////////////
// Build

//This command returns a Ascii sting of the date and time of the build.

///////////////////////////////////////////////////////////////////////////////
// Configuration

#define WED_RATE_SCALE 10

// This is used to calculate the values for the slow/fast_rate
// parameters below. interval_msec is the data logging interval in
// milliseconds.  Max interval is 255 * WED_RATE_SCALE msec. Min
// interval is TBD. Setting the interval to 0 will disable logging for
// that parameter.
static inline uint16 WEDConfigRateParam(uint32 interval_msec) {
	if (!interval_msec) return 0;
	uint32 tmp = (interval_msec-1) / WED_RATE_SCALE + 1; // round up
	if (tmp > 0xffff) tmp = 0xffff;
	return tmp;
}

typedef struct {
	// TBD: params to determine when to use slow vs. fast logging rate

	// For testing, causes the test data to be generated according to
	// the fast or slow logging rates.
	uint8 usefast;
	uint16 fast_mode_timeout;  // in sec
} PACKED WEDConfigGeneral;

typedef struct {
	// Rate of data logging, calculate with WEDConfigRateParam
	uint16 slow_rate;
	uint16 fast_rate;

	// These values correspond directly to the same-name MMA8451Q registers.
	// NOTE: These are not hooked up right now.
	uint8 hp_filter_cutoff;
	uint8 ctrl_reg1;
	uint8 ctrl_reg2;
	uint8 off_x;
	uint8 off_y;
	uint8 off_z;
} PACKED WEDConfigAccel;

#define DAC_REF_VREF_BUF    0 // default
#define DAC_REF_VREF_UNBUF  1
#define DAC_REF_VCC         2
#define DAC_REF_VCC         2

#define ADC_REF_27          0  // default
#define ADC_REF_12          1
#define ADC_REF_22          2

enum {
    WED_SAMPLE_LED_RED  = (1 << 0),
    WED_SAMPLE_LED_IR   = (1 << 1),
    WED_SAMPLE_LED_NONE = (1 << 2),
};

#define CONFIGLS_FLAGS_START_NOW 0x80
#define CONFIGLS_FLAGS_ABORT     0x40

typedef struct {
	uint8 on_time;   // LED on time before ADC reading, in msec
	uint8 off_time;  // LED off time, in msec. if 0, LED will remain on the entire duration

	// Samples will be taken at the above rate for 'duration' seconds
	// every 'interval' seconds. Sampling is disabled if interval is 0.
	uint16 fast_interval;
	uint16 slow_interval;
	uint8 duration;

	uint8 gain;      // 0-127, pot setting for red/IR LED samples

	// WARRNING: DO NOT set the off time to 0 and then request the off reading, it will lockup.
	uint8 leds;      // bit 0 = sample red LED, bit 1 = sample IR LED, bit 2 = sample w/LEDs off
	// WARRING: DO NOT drive the LED past 32 with a off time of 0. You will burn out the RED LED.
	// IR LED may be driven higher, but testing is needed to verify. The spec says continious current of 65mA is allowed, but it may get hot.
	uint8 led_drv;   // 1 to 63, MAX LED drive strength
	uint8 norecal;   // 1 = Do NOT auto shift DAC when readings get too close to the min/max

	uint8 debug;     // Console output debug level, set to 0 for normal operation
	uint8 unused1;   // unused
	uint8 unused2;   // Which refernce voltage for the DAC
	uint8 unused3;   // Can be 8, 10, 12, or 14.  default = 14
	uint8 flags;     // Contol Bits
} PACKED WEDConfigLS;

typedef struct {
	// Rate of data logging, calculate with WEDConfigRateParam
	uint16 slow_rate;
	uint16 fast_rate;
} PACKED WEDConfigTemp;

typedef struct {
	// Connection interval in 1.25ms units. Min 6, max 32.
	uint8 conn_intr;

	// Supervision timeout in 10ms units. Min 10, max 3200.
	uint16 timeout;
} PACKED WEDConfigConn;

typedef enum {
	WED_MAINT_RESET_CONFIG, // Reset config to defaults
	WED_MAINT_CLEAR_LOG,    // Clear data log
	WED_MAINT_RESET,        // Reset CPU
	WED_MAINT_TAG,          // Insert WEDLogTag entry into log
} PACKED WED_MAINT_CMD;

typedef struct {
	// tag data for WEDLogTag entry
	uint8 tag[WED_TAG_SIZE];
} PACKED WEDMaintTag;

typedef struct {
	// WED_MAINT_CMD value
	uint8 command;

	union {
		WEDMaintTag tag;
	};
} PACKED WEDConfigMaint;

enum {
	// Turns on automatic data log download via notifications
	WED_CONFIG_LOG_DL_EN  = (1 << 0),

	// Enables WED_LOG_xxx_CMP compressed formats. Log must be cleared
	// when setting this from enabled to disabled.
	WED_CONFIG_LOG_CMP_EN = (1 << 1),
};

typedef struct {
	// WED_CONFIG_LOG_xxx flags
	uint8 flags;
} PACKED WEDConfigLog;

#define DEV_NAME_LEN 13

typedef struct {
	// WED_CONFIG_NAME
	char name[DEV_NAME_LEN];
} PACKED WEDConfigName;


typedef enum {
	WED_CFG_GENERAL,
	WED_CFG_ACCEL,
	WED_CFG_LS,
	WED_CFG_NOTHING,
	WED_CFG_TEMP,
	WED_CFG_MAINT,
	WED_CFG_LOG,
	WED_CFG_CONN,
	WED_CFG_NAME,
} WED_CFG_TYPE;

// Top-level struct for characteristic UUID AMI_UUID(WED_UUID_CONFIG)
typedef struct {
	// WED_CFG_TYPE identifying following struct type
	uint8 config_type;

	// Structure corresponding to config_type
	union {
		WEDConfigGeneral general;
		WEDConfigAccel accel;
		WEDConfigLS lightsensor;
		WEDConfigTemp temp;
		WEDConfigMaint maint;
		WEDConfigLog log;
		WEDConfigConn conn;
		WEDConfigName name;
	};
} PACKED WEDConfig;

///////////////////////////////////////////////////////////////////////////////
// Data Log

// Characteristic UUID AMI_UUID(WED_UUID_LOGDATA) will return zero or
// more of the following structs (depending on how many will fit in
// the BLE stack's buffer and how many log entries are left). Each
// struct holds the data for a single log entry. The first byte of
// each struct is a 'type' byte from the WED_LOG_TYPE enum that
// identifies the type of the structure. If multiple entries are
// present, they will be packed back-to-back in the data block. Log
// entries are erased as they are read. To read the whole log,
// continue reading this characteristic until
// WEDStatus.num_log_entries have been read.

// When log download is enabled in WEDConfigLog, notifications will be
// sent automatically when log data is available. The notification
// data will be in the same format as reading this characteristic,
// except that the upper 4 bits of the first byte of each packet will
// be overwritten with a rolling sequence number. The sequence number
// should be used to check for missing or out-of-order packets.

typedef enum {
	WED_LOG_TIME,
	WED_LOG_ACCEL,
	WED_LOG_LS_CONFIG,
	WED_LOG_LS_DATA,
	WED_LOG_TEMP,
	WED_LOG_TAG,
	WED_LOG_ACCEL_CMP,
} WED_LOG_TYPE;

// Timestamp records indicate the timestamp of the log entry
// immediately following it. They will be inserted periodically, or
// any time the sampling rate changes between fast and slow.
typedef struct {
	uint8 type; // WED_LOG_TIME

	// Timestamp for next log entry, in WED_TIME_TICKS_PER_SEC
	uint32 timestamp;

	// If 1, following log entries were sampled at the fast rate
	uint8 fast_rate;
} PACKED WEDLogTimestamp;

typedef struct {
	uint8 type; // WED_LOG_ACCEL

	// Accelerometer values directly from the MMA8451Q OUT_X/Y/Z_MSB registers.
	int8 accel[3];
} PACKED WEDLogAccel;

enum {
	WED_LOG_ACCEL_CMP_3_BIT,
	WED_LOG_ACCEL_CMP_4_BIT,
	WED_LOG_ACCEL_CMP_5_BIT,
	WED_LOG_ACCEL_CMP_6_BIT,
	WED_LOG_ACCEL_CMP_8_BIT,
};

typedef struct {
	uint8 type; // WED_LOG_ACCEL_CMP

	// Bits 3-0: Number of samples - 1 in this log entry (e.g. 0=1 sample, 15=16 samples).
	// Bits 6-4: Data size, one of WED_LOG_ACCEL_CMP_x_BIT
	// Bit 7: reserved
	uint8 count_bits;

	// For WED_LOG_ACCEL_CMP_8_BIT, the following samples are normal
	// 8-bit values. For the 3/4/5/6_BIT types, values are deltas of the
	// specified size. The delta is based on the previous sample, which
	// may have come in a regular WEDLogAccel entry. The deltas are
	// tracked per-axis (e.g. the Y1 delta is Y1 - Y0, not Y1 - X1).
	// Values are packed starting from the high bit of the first
	// byte. The ordering is X0, Y0, Z0, X1, Y1, Z1, etc.
	uint8 data[0];
} PACKED WEDLogAccelCmp;

// Returns the size of a WEDLogAccelCmp entry.
static inline uint8 WEDLogAccelCmpSize(void* buf) {

	WEDLogAccelCmp* pkt = buf;

	uint8 bps = 8;
	switch ((pkt->count_bits >> 4) & 0x7) {
	case WED_LOG_ACCEL_CMP_3_BIT: bps = 3*3; break;
	case WED_LOG_ACCEL_CMP_4_BIT: bps = 4*3; break;
	case WED_LOG_ACCEL_CMP_5_BIT: bps = 5*3; break;
	case WED_LOG_ACCEL_CMP_6_BIT: bps = 6*3; break;
	case WED_LOG_ACCEL_CMP_8_BIT: bps = 8*3; break;
	}

	uint16 bits = ((pkt->count_bits & 0xf) + 1) * bps;
	if (bits > 144)
		return 0;

	return 2 + ((uint8)bits - 1) / 8 + 1;
}

// This is generated once at the beginning and end of each interval.
// at the end, all the data will be 0xff
typedef struct {
	uint8 type; // WED_LOG_LS_CONFIG

	uint8 dac_red;   // DAC setting for red LED
	uint8 dac_ir;    // DAC setting for IR LED
	uint8 level_red; // LED driver setting for red LED
	uint8 level_ir;  // LED driver setting for IR LED
	uint8 gain;      // calabrated gain value
	uint8 log_size;  // bytes per log entry.
} PACKED WEDLogLSConfig;

// This is generated once for each red/IR/off sample group.
typedef struct {
	uint8 type; // WED_LOG_LS_DATA

	// A/D readings. There will be 1-3 values present depending on how
	// many readings were enabled in WEDConfigLS.leds. The order is red,
	// IR, off, excluding readings that aren't enabled. The upper two
	// bits of val[0] indicate how many values are present: 0=val[0],
	// 1=val[0]+val[1], 2=val[0]+val[1]+val[2].
	uint16 val[3];
} PACKED WEDLogLSData;

// Returns the size of a WEDLogLSData entry.
static inline uint8 WEDLogLSDataSize(void* buf) {
	return 3 + (sizeof(uint16) * (((WEDLogLSData*)buf)->val[0] >> 14));
}

typedef struct {
	uint8 type; // WED_LOG_TEMP

	int16 temperature;  // DegC * 10
} PACKED WEDLogTemp;

// This is generated when a WED_MAINT_TAG command is issued.
typedef struct {
	uint8 type; // WED_LOG_TAG

	// Tag data from WED_MAINT_TAG command
	uint8 tag[WED_TAG_SIZE];
} PACKED WEDLogTag;

///////////////////////////////////////////////////////////////////////////////
// Firmware Update

typedef enum {
	WED_FIRMWARE_INIT,
	WED_FIRMWARE_DATA_BLOCK,
	WED_FIRMWARE_DATA_DONE,
	WED_FIRMWARE_UPDATE,
} WED_FIRMWARE_TYPE;

#define WED_FW_HEADER_SIZE 16
#define WED_FW_BLOCK_SIZE 16
#define WED_FW_STREAM_BLOCKS 16

// Top-level struct for writing characteristic UUID AMI_UUID(WED_UUID_FIRMWARE)
typedef struct {
	// WED_FIRMWARE_TYPE identifying following struct type
	uint8 pkt_type;

	// Structure corresponding to config_type
	union {
		uint8 header[WED_FW_HEADER_SIZE];
		uint8 data[WED_FW_BLOCK_SIZE];
	};
} PACKED WEDFirmwareCommand;

// Returned when writing a firmware command in an invalid state
#define FW_ERR_INVALID_STATE 0x81

typedef enum {
	WED_FWSTATUS_IDLE,         // No firmware update is in progress
	WED_FWSTATUS_WAIT,         // Processing, continue polling status
	WED_FWSTATUS_UPLOAD_READY, // Ready for data upload
	WED_FWSTATUS_UPDATE_READY, // Ready to update flash
	WED_FWSTATUS_ERROR,        // Error, update aborted
} WED_FIRMWARE_STATUS;

typedef enum {
	WED_FWERROR_NONE,    // No error
	WED_FWERROR_HEADER,  // Firmware image header was unrecognized
	WED_FWERROR_SIZE,    // Image size was too large
	WED_FWERROR_CRC,     // CRC check failed
	WED_FWERROR_FLASH,   // SPI flash error
	WED_FWERROR_OTHER,   // Internal error
} WED_FIRMWARE_ERROR;

// Top-level struct for reading characteristic UUID AMI_UUID(WED_UUID_FIRMWARE)
typedef struct {
	// WED_FIRMWARE_STATUS value
	uint8 status;

	// WED_FIRMWARE_ERROR value, valid if status is WED_FWSTATUS_ERROR
	uint8 error_code;
} PACKED WEDFirmwareStatus;

// Pseudocode for Firmware Update Procedure
//
// Once the WED_FIRMWARE_INIT command is sent, the unit will stop
// logging and all other operations not necessary to complete the
// update. To abort the update and return to normal operation, send
// the WED_MAINT_RESET command.

#if 0
update() {
	WEDFirmwareCommand init = { WED_FIRMWARE_INIT	};
	read(firmware_file, init.header, WED_FW_HEADER_SIZE);

	ble_write(WED_UUID_FIRMWARE, init);

	seek_to_beginning(firmware_file);
	while (!end_of_file(firmware_file)) {
		WEDFirmwareStatus status = {0};
		ble_read(WED_UUID_FIRMWARE, status);
	
		if (status.status == WED_FWSTATUS_WAIT)
			continue;

		if (status.status != WED_FWSTATUS_UPLOAD_READY)
			return ERROR;

		for (int i = 0; i < WED_FW_STREAM_BLOCKS; i++) {
			WEDFirmwareCommand data = { WED_FIRMWARE_DATA_BLOCK };
			read(firmware_file, data.data, WED_FW_BLOCK_SIZE);
			ble_write(WED_UUID_FIRMWARE, data);
		}
	}

	WEDFirmwareCommand done = { WED_FIRMWARE_DATA_DONE };
	ble_write(WED_UUID_FIRMWARE, done);
	
	while (1) {
		WEDFirmwareStatus status = {0};
		ble_read(WED_UUID_FIRMWARE, status);

		if (status.status == WED_FWSTATUS_WAIT)
			continue;

		if (status.status != WED_FWSTATUS_UPDATE_READY)
			return ERROR;
	}

	WEDFirmwareCommand update = { WED_FIRMWARE_UPDATE };
	ble_write(WED_UUID_FIRMWARE, update);

	// The device will now go offline for a while to complete the update.
}
#endif

///////////////////////////////////////////////////////////////////////////////
// DEBUG, Direct access to all I2C recisters.

typedef struct {
	uint8 address; // I2C Address
	uint8 reg;     // I2C Register
	uint8 write;   // in non-zero this is a write, else it's a read
	uint8 data;    // must be valid of if write is non-zero
} PACKED WEDDebugI2CCmd;

typedef struct {
	uint8 status; // status of command. 0 for no errors
	uint8 data;   // value of last good read.
} PACKED WEDDebugI2CResult;

typedef struct {
	uint8 cmd; // 1 to force the 2.7V regulator on
} PACKED WEDDebug27V;


typedef enum {
	WED_DBG_I2C,
	WED_DBG_27V,
} PACKED WED_DBG_TYPE;

// Top-level struct for characteristic UUID AMI_UUID(WED_UUID_CONFIG)
typedef struct {
	// WED_DBG_TYPE identifying following struct type
	uint8 debug_type;

	// Structure corresponding to config_type
	union {
		WEDDebugI2CCmd i2ccmd;
		WEDDebug27V reg27v;
	};
} PACKED WEDDebug;


#ifdef __cplusplus
}
#endif

#endif // include guard
