/**
 * @file hex_ascll.h
 * @brief Hexadecimal ASCII conversion utilities
 * 
 * This module provides functions for converting between binary data
 * and hexadecimal ASCII string representations.
 * @author TOPBAND Team
 * @date 2025-11-27
 * @version 1.0
 */

#ifndef HEX_ASCLL_H
#define HEX_ASCLL_H

#include <stdbool.h>  
#include <stdint.h>   
#include <stddef.h>   

/**
 * @brief Check if character is a valid hexadecimal digit
 * 
 * @param ch Character to check
 * @return true Character is a valid hexadecimal digit
 * @return false Character is not a valid hexadecimal digit
 */
bool is_hex_char(char ch);

/**
 * @brief Convert single hexadecimal ASCII character to 4-bit value
 * 
 * @param hex_char Input hexadecimal character ('0'-'9', 'A'-'F')
 * @param value Pointer to store the converted 4-bit value (0-15)
 * @return true Conversion successful
 * @return false Conversion failed (invalid input or NULL pointer)
 */
bool hex_char_to_uint4(uint8_t hex_char, uint8_t *value);

/**
 * @brief Convert two hexadecimal ASCII characters to one byte
 * 
 * @param hex_high High nibble hexadecimal character
 * @param hex_low Low nibble hexadecimal character  
 * @param value Pointer to store the converted byte value
 * @return true Conversion successful
 * @return false Conversion failed (invalid input or NULL pointer)
 */
bool hex_chars_to_uint8(uint8_t hex_high, uint8_t hex_low, uint8_t *value);

/**
 * @brief Convert one byte to two hexadecimal ASCII characters
 * 
 * @param value Input byte value to convert
 * @param hex_high Pointer to store high nibble hexadecimal character
 * @param hex_low Pointer to store low nibble hexadecimal character
 * @return true Conversion successful
 * @return false Conversion failed (NULL pointer)
 */
bool uint8_to_hex_chars(uint8_t value, uint8_t *hex_high, uint8_t *hex_low);

/**
 * @brief Convert hexadecimal ASCII string to byte array
 * 
 * This function converts a string of hexadecimal ASCII characters to 
 * a byte array. Input string length must be even (each byte requires 
 * two hexadecimal characters).
 * 
 * @param hex_str Input hexadecimal ASCII string
 * @param hex_len Length of input hexadecimal string (must be even)
 * @param bytes Output byte array buffer
 * @param bytes_size Size of output buffer in bytes
 * @param bytes_len Pointer to store actual number of bytes converted
 * @return true Conversion successful
 * @return false Conversion failed (invalid parameters, odd length, 
 *              insufficient buffer size, or invalid hex characters)
 * 
 * @note Input hex_len must be even number
 * @note Output buffer must have at least hex_len/2 bytes of space
 */
bool hex_str_to_bytes(const uint8_t *hex_str, uint16_t hex_len, uint8_t *bytes, uint16_t bytes_size, uint16_t *bytes_len);

/**
 * @brief Convert byte array to hexadecimal ASCII string
 * 
 * @param bytes Input byte array to convert
 * @param bytes_len Length of input byte array
 * @param hex_str Output hexadecimal ASCII string buffer
 * @param hex_str_size Size of output string buffer (must be at least bytes_len * 2)
 * @param hex_str_len Pointer to store actual length of output string
 * @return true Conversion successful
 * @return false Conversion failed
 */
bool bytes_to_hex_str(const uint8_t *bytes, uint16_t bytes_len, uint8_t *hex_str, uint16_t hex_str_size, uint16_t *hex_str_len);

#endif
