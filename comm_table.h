/**
 * @file comm_table.h
 * @brief Communication command table interface
 * 
 * This module provides a command table interface for communication protocol management.
 * It offers lookup functions to query command properties including request/response
 * ID mappings, timeout values, and retry counts. The table-driven approach enables
 * centralized command configuration and efficient runtime queries.
 * 
 * @author TOPBAND Team
 * @date 2025-12-08
 * @version 1.0
 */

#ifndef COMM_TABLE_H
#define COMM_TABLE_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Get response command ID by send command ID
 * @param send_cmd_id Send command ID
 * @param resp_cmd_id Pointer to store response command ID
 * @return true if found, false if not found
 */
bool comm_table_get_resp_by_send(uint8_t send_cmd_id, uint8_t *resp_cmd_id);

/**
 * @brief Get send command ID by response command ID
 * @param resp_cmd_id Response command ID
 * @param send_cmd_id Pointer to store send command ID
 * @return true if found, false if not found
 */
bool comm_table_get_send_by_resp(uint8_t resp_cmd_id, uint8_t *send_cmd_id);

/**
 * @brief Get timeout by send command ID
 * @param send_cmd_id Send command ID
 * @param timeout Pointer to store timeout in milliseconds
 * @return true if found, false if not found
 */
bool comm_table_get_timeout_by_send(uint8_t send_cmd_id, uint16_t *timeout);

/**
 * @brief Get timeout by response command ID
 * @param resp_cmd_id Response command ID
 * @param timeout Pointer to store timeout in milliseconds
 * @return true if found, false if not found
 */
bool comm_table_get_timeout_by_resp(uint8_t resp_cmd_id, uint16_t *timeout);

/**
 * @brief Get retry count by send command ID
 * @param send_cmd_id Send command ID
 * @param retry_count Pointer to store retry count
 * @return true if found, false if not found
 */
bool comm_table_get_retry_by_send(uint8_t send_cmd_id, uint16_t *retry_count);

/**
 * @brief Get retry count by response command ID
 * @param resp_cmd_id Response command ID
 * @param retry_count Pointer to store retry count
 * @return true if found, false if not found
 */
bool comm_table_get_retry_by_resp(uint8_t resp_cmd_id, uint16_t *retry_count);

#endif // COMM_TABLE_H