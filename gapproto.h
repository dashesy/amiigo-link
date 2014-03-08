/*
 * GAP protocol
 *
 * @date March 8, 2014
 * @author: dashesy
 */

#ifndef GAPPROTO_H
#define GAPPROTO_H

int exec_read(uint16_t handle);

int exec_write(uint16_t handle, const uint8_t * value, size_t vlen);

int exec_write_req(uint16_t handle, const uint8_t * value, size_t vlen);

int discover_handles(uint16_t start_handle, uint16_t end_handle);

#endif // include guard
