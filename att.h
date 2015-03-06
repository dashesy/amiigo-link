/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2010  Nokia Corporation
 *  Copyright (C) 2010  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*
 * This code was originally part of the BlueZ protocol stack (hence the comments above)
 * but has been heavily modified to remove dependencies on glib, and make it a true low-level
 * protocol stack.
 *
 * Bluetooth UUID also added here as it seems symbols do not get exported in newer BlueZ-lib version.
 *
 */

#ifndef ATT_H
#define ATT_H

/* GATT Profile Attribute types */
#define GATT_PRIM_SVC_UUID		0x2800
#define GATT_SND_SVC_UUID		0x2801
#define GATT_INCLUDE_UUID		0x2802
#define GATT_CHARAC_UUID		0x2803

/* GATT Characteristic Types */
#define GATT_CHARAC_DEVICE_NAME			0x2A00
#define GATT_CHARAC_APPEARANCE			0x2A01
#define GATT_CHARAC_PERIPHERAL_PRIV_FLAG	0x2A02
#define GATT_CHARAC_RECONNECTION_ADDRESS	0x2A03
#define GATT_CHARAC_PERIPHERAL_PREF_CONN	0x2A04
#define GATT_CHARAC_SERVICE_CHANGED		0x2A05

/* GATT Characteristic Descriptors */
#define GATT_CHARAC_EXT_PROPER_UUID	0x2900
#define GATT_CHARAC_USER_DESC_UUID	0x2901
#define GATT_CLIENT_CHARAC_CFG_UUID	0x2902
#define GATT_SERVER_CHARAC_CFG_UUID	0x2903
#define GATT_CHARAC_FMT_UUID		0x2904
#define GATT_CHARAC_AGREG_FMT_UUID	0x2905
#define GATT_CHARAC_VALID_RANGE_UUID	0x2906
#define GATT_EXTERNAL_REPORT_REFERENCE	0x2907
#define GATT_REPORT_REFERENCE		0x2908

/* Client Characteristic Configuration bit field */
#define GATT_CLIENT_CHARAC_CFG_NOTIF_BIT	0x0001
#define GATT_CLIENT_CHARAC_CFG_IND_BIT		0x0002

// ------------------------------------ UUID -----------------------------
#define GENERIC_AUDIO_UUID	"00001203-0000-1000-8000-00805f9b34fb"

#define HSP_HS_UUID		"00001108-0000-1000-8000-00805f9b34fb"
#define HSP_AG_UUID		"00001112-0000-1000-8000-00805f9b34fb"

#define HFP_HS_UUID		"0000111e-0000-1000-8000-00805f9b34fb"
#define HFP_AG_UUID		"0000111f-0000-1000-8000-00805f9b34fb"

#define ADVANCED_AUDIO_UUID	"0000110d-0000-1000-8000-00805f9b34fb"

#define A2DP_SOURCE_UUID	"0000110a-0000-1000-8000-00805f9b34fb"
#define A2DP_SINK_UUID		"0000110b-0000-1000-8000-00805f9b34fb"

#define AVRCP_REMOTE_UUID	"0000110e-0000-1000-8000-00805f9b34fb"
#define AVRCP_TARGET_UUID	"0000110c-0000-1000-8000-00805f9b34fb"

#define PANU_UUID		"00001115-0000-1000-8000-00805f9b34fb"
#define NAP_UUID		"00001116-0000-1000-8000-00805f9b34fb"
#define GN_UUID			"00001117-0000-1000-8000-00805f9b34fb"
#define BNEP_SVC_UUID		"0000000f-0000-1000-8000-00805f9b34fb"

#define PNPID_UUID		"00002a50-0000-1000-8000-00805f9b34fb"
#define DEVICE_INFORMATION_UUID	"0000180a-0000-1000-8000-00805f9b34fb"

#define GATT_UUID		"00001801-0000-1000-8000-00805f9b34fb"
#define IMMEDIATE_ALERT_UUID	"00001802-0000-1000-8000-00805f9b34fb"
#define LINK_LOSS_UUID		"00001803-0000-1000-8000-00805f9b34fb"
#define TX_POWER_UUID		"00001804-0000-1000-8000-00805f9b34fb"

#define SAP_UUID		"0000112D-0000-1000-8000-00805f9b34fb"

#define HEART_RATE_UUID			"0000180d-0000-1000-8000-00805f9b34fb"
#define HEART_RATE_MEASUREMENT_UUID	"00002a37-0000-1000-8000-00805f9b34fb"
#define BODY_SENSOR_LOCATION_UUID	"00002a38-0000-1000-8000-00805f9b34fb"
#define HEART_RATE_CONTROL_POINT_UUID	"00002a39-0000-1000-8000-00805f9b34fb"

#define HEALTH_THERMOMETER_UUID		"00001809-0000-1000-8000-00805f9b34fb"
#define TEMPERATURE_MEASUREMENT_UUID	"00002a1c-0000-1000-8000-00805f9b34fb"
#define TEMPERATURE_TYPE_UUID		"00002a1d-0000-1000-8000-00805f9b34fb"
#define INTERMEDIATE_TEMPERATURE_UUID	"00002a1e-0000-1000-8000-00805f9b34fb"
#define MEASUREMENT_INTERVAL_UUID	"00002a21-0000-1000-8000-00805f9b34fb"

#define CYCLING_SC_UUID		"00001816-0000-1000-8000-00805f9b34fb"
#define CSC_MEASUREMENT_UUID	"00002a5b-0000-1000-8000-00805f9b34fb"
#define CSC_FEATURE_UUID	"00002a5c-0000-1000-8000-00805f9b34fb"
#define SENSOR_LOCATION_UUID	"00002a5d-0000-1000-8000-00805f9b34fb"
#define SC_CONTROL_POINT_UUID	"00002a55-0000-1000-8000-00805f9b34fb"

#define RFCOMM_UUID_STR		"00000003-0000-1000-8000-00805f9b34fb"

#define HDP_UUID		"00001400-0000-1000-8000-00805f9b34fb"
#define HDP_SOURCE_UUID		"00001401-0000-1000-8000-00805f9b34fb"
#define HDP_SINK_UUID		"00001402-0000-1000-8000-00805f9b34fb"

#define HID_UUID		"00001124-0000-1000-8000-00805f9b34fb"

#define DUN_GW_UUID		"00001103-0000-1000-8000-00805f9b34fb"

#define GAP_UUID		"00001800-0000-1000-8000-00805f9b34fb"
#define PNP_UUID		"00001200-0000-1000-8000-00805f9b34fb"

#define SPP_UUID		"00001101-0000-1000-8000-00805f9b34fb"

#define OBEX_SYNC_UUID		"00001104-0000-1000-8000-00805f9b34fb"
#define OBEX_OPP_UUID		"00001105-0000-1000-8000-00805f9b34fb"
#define OBEX_FTP_UUID		"00001106-0000-1000-8000-00805f9b34fb"
#define OBEX_PCE_UUID		"0000112e-0000-1000-8000-00805f9b34fb"
#define OBEX_PSE_UUID		"0000112f-0000-1000-8000-00805f9b34fb"
#define OBEX_PBAP_UUID		"00001130-0000-1000-8000-00805f9b34fb"
#define OBEX_MAS_UUID		"00001132-0000-1000-8000-00805f9b34fb"
#define OBEX_MNS_UUID		"00001133-0000-1000-8000-00805f9b34fb"
#define OBEX_MAP_UUID		"00001134-0000-1000-8000-00805f9b34fb"

typedef struct {
    enum {
        BT_UUID_UNSPEC = 0, BT_UUID16 = 16, BT_UUID32 = 32, BT_UUID128 = 128,
    } type;
    union {
        uint16_t u16;
        uint32_t u32;
        uint128_t u128;
    } value;
} bt_uuid_t;

int bt_uuid_strcmp(const void *a, const void *b);

int bt_uuid16_create(bt_uuid_t *btuuid, uint16_t value);
int bt_uuid32_create(bt_uuid_t *btuuid, uint32_t value);
int bt_uuid128_create(bt_uuid_t *btuuid, uint128_t value);

int bt_uuid_cmp(const bt_uuid_t *uuid1, const bt_uuid_t *uuid2);
void bt_uuid_to_uuid128(const bt_uuid_t *src, bt_uuid_t *dst);

#define MAX_LEN_UUID_STR 37

int bt_uuid_to_string(const bt_uuid_t *uuid, char *str, size_t n);
int bt_string_to_uuid(bt_uuid_t *uuid, const char *string);

// ------------------------------------ ATT ------------------------------------

/* Attribute Protocol Opcodes */
#define ATT_OP_ERROR			0x01
#define ATT_OP_MTU_REQ			0x02
#define ATT_OP_MTU_RESP			0x03
#define ATT_OP_FIND_INFO_REQ		0x04
#define ATT_OP_FIND_INFO_RESP		0x05
#define ATT_OP_FIND_BY_TYPE_REQ		0x06
#define ATT_OP_FIND_BY_TYPE_RESP	0x07
#define ATT_OP_READ_BY_TYPE_REQ		0x08
#define ATT_OP_READ_BY_TYPE_RESP	0x09
#define ATT_OP_READ_REQ			0x0A
#define ATT_OP_READ_RESP		0x0B
#define ATT_OP_READ_BLOB_REQ		0x0C
#define ATT_OP_READ_BLOB_RESP		0x0D
#define ATT_OP_READ_MULTI_REQ		0x0E
#define ATT_OP_READ_MULTI_RESP		0x0F
#define ATT_OP_READ_BY_GROUP_REQ	0x10
#define ATT_OP_READ_BY_GROUP_RESP	0x11
#define ATT_OP_WRITE_REQ		0x12
#define ATT_OP_WRITE_RESP		0x13
#define ATT_OP_WRITE_CMD		0x52
#define ATT_OP_PREP_WRITE_REQ		0x16
#define ATT_OP_PREP_WRITE_RESP		0x17
#define ATT_OP_EXEC_WRITE_REQ		0x18
#define ATT_OP_EXEC_WRITE_RESP		0x19
#define ATT_OP_HANDLE_NOTIFY		0x1B
#define ATT_OP_HANDLE_IND		0x1D
#define ATT_OP_HANDLE_CNF		0x1E
#define ATT_OP_SIGNED_WRITE_CMD		0xD2

/* Error codes for Error response PDU */
#define ATT_ECODE_INVALID_HANDLE		0x01
#define ATT_ECODE_READ_NOT_PERM			0x02
#define ATT_ECODE_WRITE_NOT_PERM		0x03
#define ATT_ECODE_INVALID_PDU			0x04
#define ATT_ECODE_AUTHENTICATION		0x05
#define ATT_ECODE_REQ_NOT_SUPP			0x06
#define ATT_ECODE_INVALID_OFFSET		0x07
#define ATT_ECODE_AUTHORIZATION			0x08
#define ATT_ECODE_PREP_QUEUE_FULL		0x09
#define ATT_ECODE_ATTR_NOT_FOUND		0x0A
#define ATT_ECODE_ATTR_NOT_LONG			0x0B
#define ATT_ECODE_INSUFF_ENCR_KEY_SIZE		0x0C
#define ATT_ECODE_INVAL_ATTR_VALUE_LEN		0x0D
#define ATT_ECODE_UNLIKELY			0x0E
#define ATT_ECODE_INSUFF_ENC			0x0F
#define ATT_ECODE_UNSUPP_GRP_TYPE		0x10
#define ATT_ECODE_INSUFF_RESOURCES		0x11
/* Application error */
#define ATT_ECODE_IO				0x80
#define ATT_ECODE_TIMEOUT			0x81
#define ATT_ECODE_ABORTED			0x82

/* Characteristic Property bit field */
#define ATT_CHAR_PROPER_BROADCAST		0x01
#define ATT_CHAR_PROPER_READ			0x02
#define ATT_CHAR_PROPER_WRITE_WITHOUT_RESP	0x04
#define ATT_CHAR_PROPER_WRITE			0x08
#define ATT_CHAR_PROPER_NOTIFY			0x10
#define ATT_CHAR_PROPER_INDICATE		0x20
#define ATT_CHAR_PROPER_AUTH			0x40
#define ATT_CHAR_PROPER_EXT_PROPER		0x80

#define ATT_MAX_VALUE_LEN			512
#define ATT_DEFAULT_L2CAP_MTU			48
#define ATT_DEFAULT_LE_MTU			23

#define ATT_CID					4
#define ATT_PSM					31

/* Flags for Execute Write Request Operation */
#define ATT_CANCEL_ALL_PREP_WRITES              0x00
#define ATT_WRITE_ALL_PREP_WRITES               0x01

/* Find Information Response Formats */
#define ATT_FIND_INFO_RESP_FMT_16BIT		0x01
#define ATT_FIND_INFO_RESP_FMT_128BIT		0x02

uint8_t is_response(uint8_t opcode);

uint8_t opcode2expected(uint8_t opcode);

struct att_data_list {
    uint16_t num;
    uint16_t len;
    uint8_t **data;
};

struct att_range {
    uint16_t start;
    uint16_t end;
};

struct gatt_primary {
    bt_uuid_t uuid;
    struct att_range range;
};

struct gatt_included {
    bt_uuid_t uuid;
    uint16_t handle;
    struct att_range range;
};

struct gatt_char {
    bt_uuid_t uuid;
    uint16_t handle;
    uint8_t properties;
    uint16_t value_handle;
};

#define MAX_LEN_UUID_STR 37

int bt_uuid_to_string(const bt_uuid_t *uuid, char *str, size_t n);
int bt_string_to_uuid(bt_uuid_t *uuid, const char *string);

/* These functions do byte conversion */
static inline uint8_t att_get_u8(const void *ptr) {
    const uint8_t *u8_ptr = (const uint8_t *) ptr;
    return bt_get_unaligned(u8_ptr);
}

static inline uint16_t att_get_u16(const void *ptr) {
    const uint16_t *u16_ptr = (const uint16_t *) ptr;
    return btohs(bt_get_unaligned(u16_ptr));
}

static inline uint32_t att_get_u32(const void *ptr) {
    const uint32_t *u32_ptr = (const uint32_t *) ptr;
    return btohl(bt_get_unaligned(u32_ptr));
}

static inline uint128_t att_get_u128(const void *ptr) {
    const uint128_t *u128_ptr = (const uint128_t *) ptr;
    uint128_t dst;

    btoh128(u128_ptr, &dst);

    return dst;
}

static inline void att_put_u8(uint8_t src, void *dst) {
    bt_put_unaligned(src, (uint8_t *) dst);
}

static inline void att_put_u16(uint16_t src, void *dst) {
    bt_put_unaligned(htobs(src), (uint16_t *) dst);
}

static inline void att_put_u32(uint32_t src, void *dst) {
    bt_put_unaligned(htobl(src), (uint32_t *) dst);
}

static inline void att_put_u128(uint128_t src, void *dst) {
    uint128_t *d128 = (uint128_t *) dst;

    htob128(&src, d128);
}

static inline void att_put_uuid16(bt_uuid_t src, void *dst) {
    att_put_u16(src.value.u16, dst);
}

static inline void att_put_uuid128(bt_uuid_t src, void *dst) {
    att_put_u128(src.value.u128, dst);
}

static inline void att_put_uuid(bt_uuid_t src, void *dst) {
    if (src.type == BT_UUID16)
        att_put_uuid16(src, dst);
    else
        att_put_uuid128(src, dst);
}

static inline bt_uuid_t att_get_uuid16(const void *ptr) {
    bt_uuid_t uuid;

    bt_uuid16_create(&uuid, att_get_u16(ptr));

    return uuid;
}

static inline bt_uuid_t att_get_uuid128(const void *ptr) {
    bt_uuid_t uuid;
    uint128_t value;

    value = att_get_u128(ptr);
    bt_uuid128_create(&uuid, value);

    return uuid;
}

struct att_data_list *att_data_list_alloc(uint16_t num, uint16_t len);
void att_data_list_free(struct att_data_list *list);

const char *att_ecode2str(uint8_t status);
const char *att_op2str(uint8_t op);

uint16_t enc_read_by_grp_req(uint16_t start, uint16_t end, bt_uuid_t *uuid,
        uint8_t *pdu, size_t len);
uint16_t dec_read_by_grp_req(const uint8_t *pdu, size_t len, uint16_t *start,
        uint16_t *end, bt_uuid_t *uuid);
uint16_t enc_read_by_grp_resp(struct att_data_list *list, uint8_t *pdu,
        size_t len);
uint16_t enc_find_by_type_req(uint16_t start, uint16_t end, bt_uuid_t *uuid,
        const uint8_t *value, size_t vlen, uint8_t *pdu, size_t len);
uint16_t dec_find_by_type_req(const uint8_t *pdu, size_t len, uint16_t *start,
        uint16_t *end, bt_uuid_t *uuid, uint8_t *value, size_t *vlen);
struct att_data_list *dec_read_by_grp_resp(const uint8_t *pdu, size_t len);
uint16_t enc_read_by_type_req(uint16_t start, uint16_t end, bt_uuid_t *uuid,
        uint8_t *pdu, size_t len);
uint16_t dec_read_by_type_req(const uint8_t *pdu, size_t len, uint16_t *start,
        uint16_t *end, bt_uuid_t *uuid);
uint16_t enc_read_by_type_resp(struct att_data_list *list, uint8_t *pdu,
        size_t len);
uint16_t enc_write_cmd(uint16_t handle, const uint8_t *value, size_t vlen,
        uint8_t *pdu, size_t len);
uint16_t dec_write_cmd(const uint8_t *pdu, size_t len, uint16_t *handle,
        uint8_t *value, size_t *vlen);
struct att_data_list *dec_read_by_type_resp(const uint8_t *pdu, size_t len);
uint16_t enc_write_req(uint16_t handle, const uint8_t *value, size_t vlen,
        uint8_t *pdu, size_t len);
uint16_t dec_write_req(const uint8_t *pdu, size_t len, uint16_t *handle,
        uint8_t *value, size_t *vlen);
uint16_t enc_write_resp(uint8_t *pdu);
uint16_t dec_write_resp(const uint8_t *pdu, size_t len);
uint16_t enc_read_req(uint16_t handle, uint8_t *pdu, size_t len);
uint16_t enc_read_blob_req(uint16_t handle, uint16_t offset, uint8_t *pdu,
        size_t len);
uint16_t dec_read_req(const uint8_t *pdu, size_t len, uint16_t *handle);
uint16_t dec_read_blob_req(const uint8_t *pdu, size_t len, uint16_t *handle,
        uint16_t *offset);
uint16_t enc_read_resp(uint8_t *value, size_t vlen, uint8_t *pdu, size_t len);
uint16_t enc_read_blob_resp(uint8_t *value, size_t vlen, uint16_t offset,
        uint8_t *pdu, size_t len);
ssize_t dec_read_resp(const uint8_t *pdu, size_t len, uint8_t *value,
        size_t vlen);
uint16_t enc_error_resp(uint8_t opcode, uint16_t handle, uint8_t status,
        uint8_t *pdu, size_t len);
uint16_t enc_find_info_req(uint16_t start, uint16_t end, uint8_t *pdu,
        size_t len);
uint16_t dec_find_info_req(const uint8_t *pdu, size_t len, uint16_t *start,
        uint16_t *end);
uint16_t enc_find_info_resp(uint8_t format, struct att_data_list *list,
        uint8_t *pdu, size_t len);
struct att_data_list *dec_find_info_resp(const uint8_t *pdu, size_t len,
        uint8_t *format);
uint16_t enc_notification(uint16_t handle, uint8_t *value, size_t vlen,
        uint8_t *pdu, size_t len);
uint16_t enc_indication(uint16_t handle, uint8_t *value, size_t vlen,
        uint8_t *pdu, size_t len);
uint16_t dec_indication(const uint8_t *pdu, size_t len, uint16_t *handle,
        uint8_t *value, size_t vlen);
uint16_t enc_confirmation(uint8_t *pdu, size_t len);

uint16_t enc_mtu_req(uint16_t mtu, uint8_t *pdu, size_t len);
uint16_t dec_mtu_req(const uint8_t *pdu, size_t len, uint16_t *mtu);
uint16_t enc_mtu_resp(uint16_t mtu, uint8_t *pdu, size_t len);
uint16_t dec_mtu_resp(const uint8_t *pdu, size_t len, uint16_t *mtu);

uint16_t enc_prep_write_req(uint16_t handle, uint16_t offset,
        const uint8_t *value, size_t vlen, uint8_t *pdu, size_t len);
uint16_t dec_prep_write_req(const uint8_t *pdu, size_t len, uint16_t *handle,
        uint16_t *offset, uint8_t *value, size_t *vlen);
uint16_t enc_prep_write_resp(uint16_t handle, uint16_t offset,
        const uint8_t *value, size_t vlen, uint8_t *pdu, size_t len);
uint16_t dec_prep_write_resp(const uint8_t *pdu, size_t len, uint16_t *handle,
        uint16_t *offset, uint8_t *value, size_t *vlen);
uint16_t enc_exec_write_req(uint8_t flags, uint8_t *pdu, size_t len);
uint16_t dec_exec_write_req(const uint8_t *pdu, size_t len, uint8_t *flags);
uint16_t enc_exec_write_resp(uint8_t *pdu);
uint16_t dec_exec_write_resp(const uint8_t *pdu, size_t len);

#endif
