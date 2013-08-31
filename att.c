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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <bluetooth/bluetooth.h>

#include "att.h"

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

bool is_response(uint8_t opcode)
{
	switch (opcode) {
	case ATT_OP_ERROR:
	case ATT_OP_MTU_RESP:
	case ATT_OP_FIND_INFO_RESP:
	case ATT_OP_FIND_BY_TYPE_RESP:
	case ATT_OP_READ_BY_TYPE_RESP:
	case ATT_OP_READ_RESP:
	case ATT_OP_READ_BLOB_RESP:
	case ATT_OP_READ_MULTI_RESP:
	case ATT_OP_READ_BY_GROUP_RESP:
	case ATT_OP_WRITE_RESP:
	case ATT_OP_PREP_WRITE_RESP:
	case ATT_OP_EXEC_WRITE_RESP:
	case ATT_OP_HANDLE_CNF:
		return true;
	}

	return false;
}

uint8_t opcode2expected(uint8_t opcode)
{
	switch (opcode) {
	case ATT_OP_MTU_REQ:
		return ATT_OP_MTU_RESP;

	case ATT_OP_FIND_INFO_REQ:
		return ATT_OP_FIND_INFO_RESP;

	case ATT_OP_FIND_BY_TYPE_REQ:
		return ATT_OP_FIND_BY_TYPE_RESP;

	case ATT_OP_READ_BY_TYPE_REQ:
		return ATT_OP_READ_BY_TYPE_RESP;

	case ATT_OP_READ_REQ:
		return ATT_OP_READ_RESP;

	case ATT_OP_READ_BLOB_REQ:
		return ATT_OP_READ_BLOB_RESP;

	case ATT_OP_READ_MULTI_REQ:
		return ATT_OP_READ_MULTI_RESP;

	case ATT_OP_READ_BY_GROUP_REQ:
		return ATT_OP_READ_BY_GROUP_RESP;

	case ATT_OP_WRITE_REQ:
		return ATT_OP_WRITE_RESP;

	case ATT_OP_PREP_WRITE_REQ:
		return ATT_OP_PREP_WRITE_RESP;

	case ATT_OP_EXEC_WRITE_REQ:
		return ATT_OP_EXEC_WRITE_RESP;

	case ATT_OP_HANDLE_IND:
		return ATT_OP_HANDLE_CNF;
	}

	return 0;
}

/* Attribute Protocol Opcodes */
const char *att_op2str(uint8_t op)
{
	switch (op) {
	case ATT_OP_ERROR:
		return "Error";
	case ATT_OP_MTU_REQ:
		return "MTU req";
	case ATT_OP_MTU_RESP:
		return "MTU resp";
	case ATT_OP_FIND_INFO_REQ:
		return "Find Information req";
	case ATT_OP_FIND_INFO_RESP:
		return "Find Information resp";
	case ATT_OP_FIND_BY_TYPE_REQ:
		return "Find By Type req";
	case ATT_OP_FIND_BY_TYPE_RESP:
		return "Find By Type resp";
	case ATT_OP_READ_BY_TYPE_REQ:
		return "Read By Type req";
	case ATT_OP_READ_BY_TYPE_RESP:
		return "Read By Type resp";
	case ATT_OP_READ_REQ:
		return "Read req";
	case ATT_OP_READ_RESP:
		return "Read resp";
	case ATT_OP_READ_BLOB_REQ:
		return "Read Blob req";
	case ATT_OP_READ_BLOB_RESP:
		return "Read Blob resp";
	case ATT_OP_READ_MULTI_REQ:
		return "Read Multi req";
	case ATT_OP_READ_MULTI_RESP:
		return "Read Multi resp";
	case ATT_OP_READ_BY_GROUP_REQ:
		return "Read By Group req";
	case ATT_OP_READ_BY_GROUP_RESP:
		return "Read By Group resp";
	case ATT_OP_WRITE_REQ:
		return "Write req";
	case ATT_OP_WRITE_RESP:
		return "Write resp";
	case ATT_OP_WRITE_CMD:
		return "Write cmd";
	case ATT_OP_PREP_WRITE_REQ:
		return "Prepare Write req";
	case ATT_OP_PREP_WRITE_RESP:
		return "Prepare Write resp";
	case ATT_OP_EXEC_WRITE_REQ:
		return "Exec Write req";
	case ATT_OP_EXEC_WRITE_RESP:
		return "Exec Write resp";
	case ATT_OP_HANDLE_NOTIFY:
		return "Handle notify";
	case ATT_OP_HANDLE_IND:
		return "Handle indicate";
	case ATT_OP_HANDLE_CNF:
		return "Handle CNF";
	case ATT_OP_SIGNED_WRITE_CMD:
		return "Signed Write Cmd";
	default:
		return "Unknown";
	}
}

/* Attribute Protocol Error Codes */
const char *att_ecode2str(uint8_t status)
{
	switch (status)  {
	case ATT_ECODE_INVALID_HANDLE:
		return "Invalid handle";
	case ATT_ECODE_READ_NOT_PERM:
		return "Attribute can't be read";
	case ATT_ECODE_WRITE_NOT_PERM:
		return "Attribute can't be written";
	case ATT_ECODE_INVALID_PDU:
		return "Attribute PDU was invalid";
	case ATT_ECODE_AUTHENTICATION:
		return "Attribute requires authentication before read/write";
	case ATT_ECODE_REQ_NOT_SUPP:
		return "Server doesn't support the request received";
	case ATT_ECODE_INVALID_OFFSET:
		return "Offset past the end of the attribute";
	case ATT_ECODE_AUTHORIZATION:
		return "Attribute requires authorization before read/write";
	case ATT_ECODE_PREP_QUEUE_FULL:
		return "Too many prepare writes have been queued";
	case ATT_ECODE_ATTR_NOT_FOUND:
		return "No attribute found within the given range";
	case ATT_ECODE_ATTR_NOT_LONG:
		return "Attribute can't be read/written using Read Blob Req";
	case ATT_ECODE_INSUFF_ENCR_KEY_SIZE:
		return "Encryption Key Size is insufficient";
	case ATT_ECODE_INVAL_ATTR_VALUE_LEN:
		return "Attribute value length is invalid";
	case ATT_ECODE_UNLIKELY:
		return "Request attribute has encountered an unlikely error";
	case ATT_ECODE_INSUFF_ENC:
		return "Encryption required before read/write";
	case ATT_ECODE_UNSUPP_GRP_TYPE:
		return "Attribute type is not a supported grouping attribute";
	case ATT_ECODE_INSUFF_RESOURCES:
		return "Insufficient Resources to complete the request";
	case ATT_ECODE_IO:
		return "Internal application error: I/O";
	case ATT_ECODE_TIMEOUT:
		return "A timeout occurred";
	case ATT_ECODE_ABORTED:
		return "The operation was aborted";
	default:
		return "Unexpected error code";
	}
}

void att_data_list_free(struct att_data_list *list)
{
	if (list == NULL)
		return;

	if (list->data) {
		int i;
		for (i = 0; i < list->num; i++)
			free(list->data[i]);
	}

	free(list->data);
	free(list);
}

struct att_data_list *att_data_list_alloc(uint16_t num, uint16_t len)
{
	struct att_data_list *list;
	int i;

	if (len > UINT8_MAX)
		return NULL;

	list = malloc(sizeof(struct att_data_list));
	list->len = len;
	list->num = num;

	list->data = malloc(sizeof(uint8_t *) * num);

	for (i = 0; i < num; i++)
		list->data[i] = malloc(sizeof(uint8_t) * len);

	return list;
}

uint16_t enc_read_by_grp_req(uint16_t start, uint16_t end, bt_uuid_t *uuid,
						uint8_t *pdu, size_t len)
{
	uint16_t min_len = sizeof(pdu[0]) + sizeof(start) + sizeof(end);
	uint16_t length;

	if (!uuid)
		return 0;

	if (uuid->type == BT_UUID16)
		length = 2;
	else if (uuid->type == BT_UUID128)
		length = 16;
	else
		return 0;

	if (len < min_len + length)
		return 0;

	pdu[0] = ATT_OP_READ_BY_GROUP_REQ;
	att_put_u16(start, &pdu[1]);
	att_put_u16(end, &pdu[3]);

	att_put_uuid(*uuid, &pdu[5]);

	return min_len + length;
}

uint16_t dec_read_by_grp_req(const uint8_t *pdu, size_t len, uint16_t *start,
						uint16_t *end, bt_uuid_t *uuid)
{
	const size_t min_len = sizeof(pdu[0]) + sizeof(*start) + sizeof(*end);

	if (pdu == NULL)
		return 0;

	if (start == NULL || end == NULL || uuid == NULL)
		return 0;

	if (pdu[0] != ATT_OP_READ_BY_GROUP_REQ)
		return 0;

	if (len < min_len + 2)
		return 0;

	*start = att_get_u16(&pdu[1]);
	*end = att_get_u16(&pdu[3]);
	if (len == min_len + 2)
		*uuid = att_get_uuid16(&pdu[5]);
	else
		*uuid = att_get_uuid128(&pdu[5]);

	return len;
}

uint16_t enc_read_by_grp_resp(struct att_data_list *list, uint8_t *pdu,
								size_t len)
{
	int i;
	uint16_t w;
	uint8_t *ptr;

	if (list == NULL)
		return 0;

	if (len < list->len + sizeof(uint8_t) * 2)
		return 0;

	pdu[0] = ATT_OP_READ_BY_GROUP_RESP;
	pdu[1] = list->len;

	ptr = &pdu[2];

	for (i = 0, w = 2; i < list->num && w + list->len <= len; i++) {
		memcpy(ptr, list->data[i], list->len);
		ptr += list->len;
		w += list->len;
	}

	return w;
}

struct att_data_list *dec_read_by_grp_resp(const uint8_t *pdu, size_t len)
{
	struct att_data_list *list;
	const uint8_t *ptr;
	uint16_t elen, num;
	int i;

	if (pdu[0] != ATT_OP_READ_BY_GROUP_RESP)
		return NULL;

	elen = pdu[1];
	num = (len - 2) / elen;
	list = att_data_list_alloc(num, elen);
	if (list == NULL)
		return NULL;

	ptr = &pdu[2];

	for (i = 0; i < num; i++) {
		memcpy(list->data[i], ptr, list->len);
		ptr += list->len;
	}

	return list;
}

uint16_t enc_find_by_type_req(uint16_t start, uint16_t end, bt_uuid_t *uuid,
					const uint8_t *value, size_t vlen,
					uint8_t *pdu, size_t len)
{
	uint16_t min_len = sizeof(pdu[0]) + sizeof(start) + sizeof(end) +
							sizeof(uint16_t);

	if (pdu == NULL)
		return 0;

	if (!uuid)
		return 0;

	if (uuid->type != BT_UUID16)
		return 0;

	if (len < min_len)
		return 0;

	if (vlen > len - min_len)
		vlen = len - min_len;

	pdu[0] = ATT_OP_FIND_BY_TYPE_REQ;
	att_put_u16(start, &pdu[1]);
	att_put_u16(end, &pdu[3]);
	att_put_uuid16(*uuid, &pdu[5]);

	if (vlen > 0) {
		memcpy(&pdu[7], value, vlen);
		return min_len + vlen;
	}

	return min_len;
}

uint16_t dec_find_by_type_req(const uint8_t *pdu, size_t len, uint16_t *start,
						uint16_t *end, bt_uuid_t *uuid,
						uint8_t *value, size_t *vlen)
{
	size_t valuelen;
	uint16_t min_len = sizeof(pdu[0]) + sizeof(*start) +
						sizeof(*end) + sizeof(uint16_t);

	if (pdu == NULL)
		return 0;

	if (len < min_len)
		return 0;

	if (pdu[0] != ATT_OP_FIND_BY_TYPE_REQ)
		return 0;

	/* First requested handle number */
	if (start)
		*start = att_get_u16(&pdu[1]);

	/* Last requested handle number */
	if (end)
		*end = att_get_u16(&pdu[3]);

	/* Always UUID16 */
	if (uuid)
		*uuid = att_get_uuid16(&pdu[5]);

	valuelen = len - min_len;

	/* Attribute value to find */
	if (valuelen > 0 && value)
		memcpy(value, pdu + min_len, valuelen);

	if (vlen)
		*vlen = valuelen;

	return len;
}

uint16_t enc_read_by_type_req(uint16_t start, uint16_t end, bt_uuid_t *uuid,
						uint8_t *pdu, size_t len)
{
	uint16_t min_len = sizeof(pdu[0]) + sizeof(start) + sizeof(end);
	uint16_t length;

	if (!uuid)
		return 0;

	if (uuid->type == BT_UUID16)
		length = 2;
	else if (uuid->type == BT_UUID128)
		length = 16;
	else
		return 0;

	if (len < min_len + length)
		return 0;

	pdu[0] = ATT_OP_READ_BY_TYPE_REQ;
	att_put_u16(start, &pdu[1]);
	att_put_u16(end, &pdu[3]);

	att_put_uuid(*uuid, &pdu[5]);

	return min_len + length;
}

uint16_t dec_read_by_type_req(const uint8_t *pdu, size_t len, uint16_t *start,
						uint16_t *end, bt_uuid_t *uuid)
{
	const size_t min_len = sizeof(pdu[0]) + sizeof(*start) + sizeof(*end);

	if (pdu == NULL)
		return 0;

	if (start == NULL || end == NULL || uuid == NULL)
		return 0;

	if (len < min_len + 2)
		return 0;

	if (pdu[0] != ATT_OP_READ_BY_TYPE_REQ)
		return 0;

	*start = att_get_u16(&pdu[1]);
	*end = att_get_u16(&pdu[3]);

	if (len == min_len + 2)
		*uuid = att_get_uuid16(&pdu[5]);
	else
		*uuid = att_get_uuid128(&pdu[5]);

	return len;
}

uint16_t enc_read_by_type_resp(struct att_data_list *list, uint8_t *pdu,
								size_t len)
{
	uint8_t *ptr;
	size_t i, w, l;

	if (list == NULL)
		return 0;

	if (pdu == NULL)
		return 0;

	l = MIN(len - 2, list->len);

	pdu[0] = ATT_OP_READ_BY_TYPE_RESP;
	pdu[1] = l;
	ptr = &pdu[2];

	for (i = 0, w = 2; i < list->num && w + l <= len; i++) {
		memcpy(ptr, list->data[i], l);
		ptr += l;
		w += l;
	}

	return w;
}

struct att_data_list *dec_read_by_type_resp(const uint8_t *pdu, size_t len)
{
	struct att_data_list *list;
	const uint8_t *ptr;
	uint16_t elen, num;
	int i;

	if (pdu[0] != ATT_OP_READ_BY_TYPE_RESP)
		return NULL;

	elen = pdu[1];
	num = (len - 2) / elen;
	list = att_data_list_alloc(num, elen);
	if (list == NULL)
		return NULL;

	ptr = &pdu[2];

	for (i = 0; i < num; i++) {
		memcpy(list->data[i], ptr, list->len);
		ptr += list->len;
	}

	return list;
}

uint16_t enc_write_cmd(uint16_t handle, const uint8_t *value, size_t vlen,
						uint8_t *pdu, size_t len)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(handle);

	if (pdu == NULL)
		return 0;

	if (len < min_len)
		return 0;

	if (vlen > len - min_len)
		vlen = len - min_len;

	pdu[0] = ATT_OP_WRITE_CMD;
	att_put_u16(handle, &pdu[1]);

	if (vlen > 0) {
		memcpy(&pdu[3], value, vlen);
		return min_len + vlen;
	}

	return min_len;
}

uint16_t dec_write_cmd(const uint8_t *pdu, size_t len, uint16_t *handle,
						uint8_t *value, size_t *vlen)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(*handle);

	if (pdu == NULL)
		return 0;

	if (value == NULL || vlen == NULL || handle == NULL)
		return 0;

	if (len < min_len)
		return 0;

	if (pdu[0] != ATT_OP_WRITE_CMD)
		return 0;

	*handle = att_get_u16(&pdu[1]);
	memcpy(value, pdu + min_len, len - min_len);
	*vlen = len - min_len;

	return len;
}

uint16_t enc_write_req(uint16_t handle, const uint8_t *value, size_t vlen,
						uint8_t *pdu, size_t len)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(handle);

	if (pdu == NULL)
		return 0;

	if (len < min_len)
		return 0;

	if (vlen > len - min_len)
		vlen = len - min_len;

	pdu[0] = ATT_OP_WRITE_REQ;
	att_put_u16(handle, &pdu[1]);

	if (vlen > 0) {
		memcpy(&pdu[3], value, vlen);
		return min_len + vlen;
	}

	return min_len;
}

uint16_t dec_write_req(const uint8_t *pdu, size_t len, uint16_t *handle,
						uint8_t *value, size_t *vlen)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(*handle);

	if (pdu == NULL)
		return 0;

	if (value == NULL || vlen == NULL || handle == NULL)
		return 0;

	if (len < min_len)
		return 0;

	if (pdu[0] != ATT_OP_WRITE_REQ)
		return 0;

	*handle = att_get_u16(&pdu[1]);
	*vlen = len - min_len;
	if (*vlen > 0)
		memcpy(value, pdu + min_len, *vlen);

	return len;
}

uint16_t enc_write_resp(uint8_t *pdu)
{
	if (pdu == NULL)
		return 0;

	pdu[0] = ATT_OP_WRITE_RESP;

	return sizeof(pdu[0]);
}

uint16_t dec_write_resp(const uint8_t *pdu, size_t len)
{
	if (pdu == NULL)
		return 0;

	if (pdu[0] != ATT_OP_WRITE_RESP)
		return 0;

	return len;
}

uint16_t enc_read_req(uint16_t handle, uint8_t *pdu, size_t len)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(handle);

	if (pdu == NULL)
		return 0;

	if (len < min_len)
		return 0;

	pdu[0] = ATT_OP_READ_REQ;
	att_put_u16(handle, &pdu[1]);

	return min_len;
}

uint16_t enc_read_blob_req(uint16_t handle, uint16_t offset, uint8_t *pdu,
								size_t len)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(handle) +
							sizeof(offset);

	if (pdu == NULL)
		return 0;

	if (len < min_len)
		return 0;

	pdu[0] = ATT_OP_READ_BLOB_REQ;
	att_put_u16(handle, &pdu[1]);
	att_put_u16(offset, &pdu[3]);

	return min_len;
}

uint16_t dec_read_req(const uint8_t *pdu, size_t len, uint16_t *handle)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(*handle);

	if (pdu == NULL)
		return 0;

	if (handle == NULL)
		return 0;

	if (len < min_len)
		return 0;

	if (pdu[0] != ATT_OP_READ_REQ)
		return 0;

	*handle = att_get_u16(&pdu[1]);

	return min_len;
}

uint16_t dec_read_blob_req(const uint8_t *pdu, size_t len, uint16_t *handle,
							uint16_t *offset)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(*handle) +
							sizeof(*offset);

	if (pdu == NULL)
		return 0;

	if (handle == NULL)
		return 0;

	if (offset == NULL)
		return 0;

	if (len < min_len)
		return 0;

	if (pdu[0] != ATT_OP_READ_BLOB_REQ)
		return 0;

	*handle = att_get_u16(&pdu[1]);
	*offset = att_get_u16(&pdu[3]);

	return min_len;
}

uint16_t enc_read_resp(uint8_t *value, size_t vlen, uint8_t *pdu, size_t len)
{
	if (pdu == NULL)
		return 0;

	/* If the attribute value length is longer than the allowed PDU size,
	 * send only the octets that fit on the PDU. The remaining octets can
	 * be requested using the Read Blob Request. */
	if (vlen > len - 1)
		vlen = len - 1;

	pdu[0] = ATT_OP_READ_RESP;

	memcpy(pdu + 1, value, vlen);

	return vlen + 1;
}

uint16_t enc_read_blob_resp(uint8_t *value, size_t vlen, uint16_t offset,
						uint8_t *pdu, size_t len)
{
	if (pdu == NULL)
		return 0;

	vlen -= offset;
	if (vlen > len - 1)
		vlen = len - 1;

	pdu[0] = ATT_OP_READ_BLOB_RESP;

	memcpy(pdu + 1, &value[offset], vlen);

	return vlen + 1;
}

ssize_t dec_read_resp(const uint8_t *pdu, size_t len, uint8_t *value,
								size_t vlen)
{
	if (pdu == NULL)
		return -EINVAL;

	if (pdu[0] != ATT_OP_READ_RESP)
		return -EINVAL;

	if (value == NULL)
		return len - 1;

	if (vlen < (len - 1))
		return -ENOBUFS;

	memcpy(value, pdu + 1, len - 1);

	return len - 1;
}

uint16_t enc_error_resp(uint8_t opcode, uint16_t handle, uint8_t status,
						uint8_t *pdu, size_t len)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(opcode) +
						sizeof(handle) + sizeof(status);
	uint16_t u16;

	if (len < min_len)
		return 0;

	u16 = htobs(handle);
	pdu[0] = ATT_OP_ERROR;
	pdu[1] = opcode;
	memcpy(&pdu[2], &u16, sizeof(u16));
	pdu[4] = status;

	return min_len;
}

uint16_t enc_find_info_req(uint16_t start, uint16_t end, uint8_t *pdu,
								size_t len)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(start) + sizeof(end);

	if (pdu == NULL)
		return 0;

	if (len < min_len)
		return 0;

	pdu[0] = ATT_OP_FIND_INFO_REQ;
	att_put_u16(start, &pdu[1]);
	att_put_u16(end, &pdu[3]);

	return min_len;
}

uint16_t dec_find_info_req(const uint8_t *pdu, size_t len, uint16_t *start,
								uint16_t *end)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(*start) + sizeof(*end);

	if (pdu == NULL)
		return 0;

	if (len < min_len)
		return 0;

	if (start == NULL || end == NULL)
		return 0;

	if (pdu[0] != ATT_OP_FIND_INFO_REQ)
		return 0;

	*start = att_get_u16(&pdu[1]);
	*end = att_get_u16(&pdu[3]);

	return min_len;
}

uint16_t enc_find_info_resp(uint8_t format, struct att_data_list *list,
						uint8_t *pdu, size_t len)
{
	uint8_t *ptr;
	size_t i, w;

	if (pdu == NULL)
		return 0;

	if (list == NULL)
		return 0;

	if (len < list->len + sizeof(uint8_t) * 2)
		return 0;

	pdu[0] = ATT_OP_FIND_INFO_RESP;
	pdu[1] = format;
	ptr = (void *) &pdu[2];

	for (i = 0, w = 2; i < list->num && w + list->len <= len; i++) {
		memcpy(ptr, list->data[i], list->len);
		ptr += list->len;
		w += list->len;
	}

	return w;
}

struct att_data_list *dec_find_info_resp(const uint8_t *pdu, size_t len,
							uint8_t *format)
{
	struct att_data_list *list;
	uint8_t *ptr;
	uint16_t elen, num;
	int i;

	if (pdu == NULL)
		return 0;

	if (format == NULL)
		return 0;

	if (pdu[0] != ATT_OP_FIND_INFO_RESP)
		return 0;

	*format = pdu[1];
	elen = sizeof(pdu[0]) + sizeof(*format);
	if (*format == 0x01)
		elen += 2;
	else if (*format == 0x02)
		elen += 16;

	num = (len - 2) / elen;

	ptr = (void *) &pdu[2];

	list = att_data_list_alloc(num, elen);
	if (list == NULL)
		return NULL;

	for (i = 0; i < num; i++) {
		memcpy(list->data[i], ptr, list->len);
		ptr += list->len;
	}

	return list;
}

uint16_t enc_notification(uint16_t handle, uint8_t *value, size_t vlen,
						uint8_t *pdu, size_t len)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(uint16_t);

	if (pdu == NULL)
		return 0;

	if (len < (vlen + min_len))
		return 0;

	pdu[0] = ATT_OP_HANDLE_NOTIFY;
	att_put_u16(handle, &pdu[1]);
	memcpy(&pdu[3], value, vlen);

	return vlen + min_len;
}

uint16_t enc_indication(uint16_t handle, uint8_t *value, size_t vlen,
						uint8_t *pdu, size_t len)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(uint16_t);

	if (pdu == NULL)
		return 0;

	if (len < (vlen + min_len))
		return 0;

	pdu[0] = ATT_OP_HANDLE_IND;
	att_put_u16(handle, &pdu[1]);
	memcpy(&pdu[3], value, vlen);

	return vlen + min_len;
}

uint16_t dec_indication(const uint8_t *pdu, size_t len, uint16_t *handle,
						uint8_t *value, size_t vlen)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(uint16_t);
	uint16_t dlen;

	if (pdu == NULL)
		return 0;

	if (pdu[0] != ATT_OP_HANDLE_IND)
		return 0;

	if (len < min_len)
		return 0;

	dlen = MIN(len - min_len, vlen);

	if (handle)
		*handle = att_get_u16(&pdu[1]);

	memcpy(value, &pdu[3], dlen);

	return dlen;
}

uint16_t enc_confirmation(uint8_t *pdu, size_t len)
{
	const uint16_t min_len = sizeof(pdu[0]);

	if (pdu == NULL)
		return 0;

	if (len < min_len)
		return 0;

	pdu[0] = ATT_OP_HANDLE_CNF;

	return min_len;
}

uint16_t enc_mtu_req(uint16_t mtu, uint8_t *pdu, size_t len)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(mtu);

	if (pdu == NULL)
		return 0;

	if (len < min_len)
		return 0;

	pdu[0] = ATT_OP_MTU_REQ;
	att_put_u16(mtu, &pdu[1]);

	return min_len;
}

uint16_t dec_mtu_req(const uint8_t *pdu, size_t len, uint16_t *mtu)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(*mtu);

	if (pdu == NULL)
		return 0;

	if (mtu == NULL)
		return 0;

	if (len < min_len)
		return 0;

	if (pdu[0] != ATT_OP_MTU_REQ)
		return 0;

	*mtu = att_get_u16(&pdu[1]);

	return min_len;
}

uint16_t enc_mtu_resp(uint16_t mtu, uint8_t *pdu, size_t len)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(mtu);

	if (pdu == NULL)
		return 0;

	if (len < min_len)
		return 0;

	pdu[0] = ATT_OP_MTU_RESP;
	att_put_u16(mtu, &pdu[1]);

	return min_len;
}

uint16_t dec_mtu_resp(const uint8_t *pdu, size_t len, uint16_t *mtu)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(*mtu);

	if (pdu == NULL)
		return 0;

	if (mtu == NULL)
		return 0;

	if (len < min_len)
		return 0;

	if (pdu[0] != ATT_OP_MTU_RESP)
		return 0;

	*mtu = att_get_u16(&pdu[1]);

	return min_len;
}

uint16_t enc_prep_write_req(uint16_t handle, uint16_t offset,
					const uint8_t *value, size_t vlen,
					uint8_t *pdu, size_t len)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(handle) +
								sizeof(offset);

	if (pdu == NULL)
		return 0;

	if (len < min_len)
		return 0;

	if (vlen > len - min_len)
		vlen = len - min_len;

	pdu[0] = ATT_OP_PREP_WRITE_REQ;
	att_put_u16(handle, &pdu[1]);
	att_put_u16(offset, &pdu[3]);

	if (vlen > 0) {
		memcpy(&pdu[5], value, vlen);
		return min_len + vlen;
	}

	return min_len;
}

uint16_t dec_prep_write_req(const uint8_t *pdu, size_t len, uint16_t *handle,
				uint16_t *offset, uint8_t *value, size_t *vlen)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(*handle) +
							sizeof(*offset);

	if (pdu == NULL)
		return 0;

	if (handle == NULL || offset == NULL || value == NULL || vlen == NULL)
		return 0;

	if (len < min_len)
		return 0;

	if (pdu[0] != ATT_OP_PREP_WRITE_REQ)
		return 0;

	*handle = att_get_u16(&pdu[1]);
	*offset = att_get_u16(&pdu[3]);

	*vlen = len - min_len;
	if (*vlen > 0)
		memcpy(value, pdu + min_len, *vlen);

	return len;
}

uint16_t enc_prep_write_resp(uint16_t handle, uint16_t offset,
					const uint8_t *value, size_t vlen,
					uint8_t *pdu, size_t len)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(handle) +
								sizeof(offset);

	if (pdu == NULL)
		return 0;

	if (len < min_len)
		return 0;

	if (vlen > len - min_len)
		vlen = len - min_len;

	pdu[0] = ATT_OP_PREP_WRITE_RESP;
	att_put_u16(handle, &pdu[1]);
	att_put_u16(offset, &pdu[3]);

	if (vlen > 0) {
		memcpy(&pdu[5], value, vlen);
		return min_len + vlen;
	}

	return min_len;
}

uint16_t dec_prep_write_resp(const uint8_t *pdu, size_t len, uint16_t *handle,
				uint16_t *offset, uint8_t *value, size_t *vlen)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(*handle) +
								sizeof(*offset);

	if (pdu == NULL)
		return 0;

	if (handle == NULL || offset == NULL || value == NULL || vlen == NULL)
		return 0;

	if (len < min_len)
		return 0;

	if (pdu[0] != ATT_OP_PREP_WRITE_REQ)
		return 0;

	*handle = att_get_u16(&pdu[1]);
	*offset = att_get_u16(&pdu[3]);
	*vlen = len - min_len;
	if (*vlen > 0)
		memcpy(value, pdu + min_len, *vlen);

	return len;
}

uint16_t enc_exec_write_req(uint8_t flags, uint8_t *pdu, size_t len)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(flags);

	if (pdu == NULL)
		return 0;

	if (len < min_len)
		return 0;

	if (flags > 1)
		return 0;

	pdu[0] = ATT_OP_EXEC_WRITE_REQ;
	pdu[1] = flags;

	return min_len;
}

uint16_t dec_exec_write_req(const uint8_t *pdu, size_t len, uint8_t *flags)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(*flags);

	if (pdu == NULL)
		return 0;

	if (flags == NULL)
		return 0;

	if (len < min_len)
		return 0;

	if (pdu[0] != ATT_OP_EXEC_WRITE_REQ)
		return 0;

	*flags = pdu[1];

	return min_len;
}

uint16_t enc_exec_write_resp(uint8_t *pdu)
{
	if (pdu == NULL)
		return 0;

	pdu[0] = ATT_OP_EXEC_WRITE_RESP;

	return sizeof(pdu[0]);
}

uint16_t dec_exec_write_resp(const uint8_t *pdu, size_t len)
{
	const uint16_t min_len = sizeof(pdu[0]);

	if (pdu == NULL)
		return 0;

	if (len < min_len)
		return 0;

	if (pdu[0] != ATT_OP_EXEC_WRITE_RESP)
		return 0;

	return len;
}
//---------------------------- UUID ----------------------------------------
#if __BYTE_ORDER == __BIG_ENDIAN
static uint128_t bluetooth_base_uuid = {
	.data = {	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
			0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB }
};

#define BASE_UUID16_OFFSET	2
#define BASE_UUID32_OFFSET	0

#else
static uint128_t bluetooth_base_uuid = {
	.data = {	0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
			0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};

#define BASE_UUID16_OFFSET	12
#define BASE_UUID32_OFFSET	BASE_UUID16_OFFSET

#endif

static void bt_uuid16_to_uuid128(const bt_uuid_t *src, bt_uuid_t *dst)
{
	dst->value.u128 = bluetooth_base_uuid;
	dst->type = BT_UUID128;

	memcpy(&dst->value.u128.data[BASE_UUID16_OFFSET],
			&src->value.u16, sizeof(src->value.u16));
}

static void bt_uuid32_to_uuid128(const bt_uuid_t *src, bt_uuid_t *dst)
{
	dst->value.u128 = bluetooth_base_uuid;
	dst->type = BT_UUID128;

	memcpy(&dst->value.u128.data[BASE_UUID32_OFFSET],
				&src->value.u32, sizeof(src->value.u32));
}

void bt_uuid_to_uuid128(const bt_uuid_t *src, bt_uuid_t *dst)
{
	switch (src->type) {
	case BT_UUID128:
		*dst = *src;
		break;
	case BT_UUID32:
		bt_uuid32_to_uuid128(src, dst);
		break;
	case BT_UUID16:
		bt_uuid16_to_uuid128(src, dst);
		break;
	default:
		break;
	}
}

static int bt_uuid128_cmp(const bt_uuid_t *u1, const bt_uuid_t *u2)
{
	return memcmp(&u1->value.u128, &u2->value.u128, sizeof(uint128_t));
}

int bt_uuid16_create(bt_uuid_t *btuuid, uint16_t value)
{
	memset(btuuid, 0, sizeof(bt_uuid_t));
	btuuid->type = BT_UUID16;
	btuuid->value.u16 = value;

	return 0;
}

int bt_uuid32_create(bt_uuid_t *btuuid, uint32_t value)
{
	memset(btuuid, 0, sizeof(bt_uuid_t));
	btuuid->type = BT_UUID32;
	btuuid->value.u32 = value;

	return 0;
}

int bt_uuid128_create(bt_uuid_t *btuuid, uint128_t value)
{
	memset(btuuid, 0, sizeof(bt_uuid_t));
	btuuid->type = BT_UUID128;
	btuuid->value.u128 = value;

	return 0;
}

int bt_uuid_cmp(const bt_uuid_t *uuid1, const bt_uuid_t *uuid2)
{
	bt_uuid_t u1, u2;

	bt_uuid_to_uuid128(uuid1, &u1);
	bt_uuid_to_uuid128(uuid2, &u2);

	return bt_uuid128_cmp(&u1, &u2);
}

/*
 * convert the UUID to string, copying a maximum of n characters.
 */
int bt_uuid_to_string(const bt_uuid_t *uuid, char *str, size_t n)
{
	if (!uuid) {
		snprintf(str, n, "NULL");
		return -EINVAL;
	}

	switch (uuid->type) {
	case BT_UUID16:
		snprintf(str, n, "%.4x", uuid->value.u16);
		break;
	case BT_UUID32:
		snprintf(str, n, "%.8x", uuid->value.u32);
		break;
	case BT_UUID128: {
		unsigned int   data0;
		unsigned short data1;
		unsigned short data2;
		unsigned short data3;
		unsigned int   data4;
		unsigned short data5;

		uint128_t nvalue;
		const uint8_t *data = (uint8_t *) &nvalue;

		hton128(&uuid->value.u128, &nvalue);

		memcpy(&data0, &data[0], 4);
		memcpy(&data1, &data[4], 2);
		memcpy(&data2, &data[6], 2);
		memcpy(&data3, &data[8], 2);
		memcpy(&data4, &data[10], 4);
		memcpy(&data5, &data[14], 2);

		snprintf(str, n, "%.8x-%.4x-%.4x-%.4x-%.8x%.4x",
				ntohl(data0), ntohs(data1),
				ntohs(data2), ntohs(data3),
				ntohl(data4), ntohs(data5));
		}
		break;
	default:
		snprintf(str, n, "Type of UUID (%x) unknown.", uuid->type);
		return -EINVAL;	/* Enum type of UUID not set */
	}

	return 0;
}

static inline int is_uuid128(const char *string)
{
	return (strlen(string) == 36 &&
			string[8] == '-' &&
			string[13] == '-' &&
			string[18] == '-' &&
			string[23] == '-');
}

static inline int is_uuid32(const char *string)
{
	return (strlen(string) == 8 || strlen(string) == 10);
}

static inline int is_uuid16(const char *string)
{
	return (strlen(string) == 4 || strlen(string) == 6);
}

static int bt_string_to_uuid16(bt_uuid_t *uuid, const char *string)
{
	uint16_t u16;
	char *endptr = NULL;

	u16 = strtol(string, &endptr, 16);
	if (endptr && *endptr == '\0') {
		bt_uuid16_create(uuid, u16);
		return 0;
	}

	return -EINVAL;
}

static int bt_string_to_uuid32(bt_uuid_t *uuid, const char *string)
{
	uint32_t u32;
	char *endptr = NULL;

	u32 = strtol(string, &endptr, 16);
	if (endptr && *endptr == '\0') {
		bt_uuid32_create(uuid, u32);
		return 0;
	}

	return -EINVAL;
}

static int bt_string_to_uuid128(bt_uuid_t *uuid, const char *string)
{
	uint32_t data0, data4;
	uint16_t data1, data2, data3, data5;
	uint128_t n128, u128;
	uint8_t *val = (uint8_t *) &n128;

	if (sscanf(string, "%08x-%04hx-%04hx-%04hx-%08x%04hx",
				&data0, &data1, &data2,
				&data3, &data4, &data5) != 6)
		return -EINVAL;

	data0 = htonl(data0);
	data1 = htons(data1);
	data2 = htons(data2);
	data3 = htons(data3);
	data4 = htonl(data4);
	data5 = htons(data5);

	memcpy(&val[0], &data0, 4);
	memcpy(&val[4], &data1, 2);
	memcpy(&val[6], &data2, 2);
	memcpy(&val[8], &data3, 2);
	memcpy(&val[10], &data4, 4);
	memcpy(&val[14], &data5, 2);

	ntoh128(&n128, &u128);

	bt_uuid128_create(uuid, u128);

	return 0;
}

int bt_string_to_uuid(bt_uuid_t *uuid, const char *string)
{
	if (is_uuid128(string))
		return bt_string_to_uuid128(uuid, string);
	else if (is_uuid32(string))
		return bt_string_to_uuid32(uuid, string);
	else if (is_uuid16(string))
		return bt_string_to_uuid16(uuid, string);

	return -EINVAL;
}

int bt_uuid_strcmp(const void *a, const void *b)
{
	return strcasecmp(a, b);
}
