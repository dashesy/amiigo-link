/*
 * GAP protocol
 *
 * @date March 8, 2014
 * @author: dashesy
 */

#ifndef GAPPROTO_H
#define GAPPROTO_H

int exec_read(int sock, uint16_t handle);

int exec_write(int sock, uint16_t handle, const uint8_t * value, size_t vlen);

int exec_write_req(int sock, uint16_t handle, const uint8_t * value, size_t vlen);

int discover_handles(int sock, uint16_t start_handle, uint16_t end_handle);

int gap_connect(const char * src, const char * dst);

int gap_recv(int sock, void * buf, size_t buflen);

int gap_shutdown(int sock);

#endif // include guard
