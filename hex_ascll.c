/**
 * @file hex_ascll.c
 * @brief Hexadecimal ASCII conversion utilities implementation
 * 
 * This module provides functions for converting between binary data
 * and hexadecimal ASCII string representations. It includes utilities
 * for single character conversion, byte conversion, and bulk string
 * conversion operations.
 * 
 * @author TOPBAND Team
 * @date 2025-11-27
 * @version 1.0
 */

#include "hex_ascll.h"

static const char hex_table[16] = {
    '0', '1', '2', '3',
    '4', '5', '6', '7',
    '8', '9', 'A', 'B',
    'C', 'D', 'E', 'F'
};

/**
 * @brief Check if character is a valid hexadecimal digit
 * 
 * This function validates whether the input character is a valid
 * hexadecimal digit. It accepts uppercase hexadecimal characters ('0'-'9', 'A'-'F')
 * but does not accept lowercase letters.
 * 
 * @param ch Character to check
 * @return true Character is a valid hexadecimal digit
 * @return false Character is not a valid hexadecimal digit
 * 
 * @note This function only accepts uppercase hex letters (A-F), not lowercase (a-f)
 * 
 * @example
 * if (is_hex_char('A')) {
 *     // Character 'A' is valid hex digit
 * }
 * if (!is_hex_char('G')) {
 *     // Character 'G' is not valid hex digit
 * }
 */
bool is_hex_char(char ch)
{
    return (((ch >= '0') && (ch <= '9')) ||
            ((ch >= 'A') && (ch <= 'F')));
}

/**
 * @brief Convert single hexadecimal ASCII character to 4-bit value
 * 
 * @param hex_char Input hexadecimal character ('0'-'9', 'A'-'F')
 * @param value Pointer to store the converted 4-bit value (0-15)
 * @return true Conversion successful
 * @return false Conversion failed (invalid input or NULL pointer)
 */
bool hex_char_to_uint4(uint8_t hex_char, uint8_t *value)
{
    bool ret = false;
    
    if (value == NULL)
    {
        ret = false;
    }
    else if ((hex_char >= (uint8_t)'0') && (hex_char <= (uint8_t)'9'))
    {
        *value = hex_char - (uint8_t)'0';
        ret = true;
    }
    else if ((hex_char >= (uint8_t)'A') && (hex_char <= (uint8_t)'F'))
    {
        *value = (hex_char - (uint8_t)'A') + 10U;
        ret = true;
    }
    else
    {
        ret = false;
    }
    
    return ret;
}

/**
 * @brief Convert two hexadecimal ASCII characters to one byte
 * 
 * @param hex_high High nibble hexadecimal character
 * @param hex_low Low nibble hexadecimal character  
 * @param value Pointer to store the converted byte value
 * @return true Conversion successful
 * @return false Conversion failed (invalid input or NULL pointer)
 */
bool hex_chars_to_uint8(uint8_t hex_high, uint8_t hex_low, uint8_t *value)
{
    uint8_t hi_val = 0U;
    uint8_t lo_val = 0U;
    bool ret = false;
    
    if (value == NULL)
    {
        ret = false;
    }
    else if ((hex_char_to_uint4(hex_high, &hi_val) == true) && 
             (hex_char_to_uint4(hex_low, &lo_val) == true))
    {
        *value = (hi_val << 4U) | lo_val;
        ret = true;
    }
    else
    {
        ret = false;
    }
    
    return ret;
}

/**
 * @brief Convert one byte to two hexadecimal ASCII characters
 * 
 * @param value Input byte value to convert
 * @param hex_high Pointer to store high nibble hexadecimal character
 * @param hex_low Pointer to store low nibble hexadecimal character
 * @return true Conversion successful
 * @return false Conversion failed (NULL pointer)
 */
bool uint8_to_hex_chars(uint8_t value, uint8_t *hex_high, uint8_t *hex_low)
{
    bool ret = false;
    if ((hex_high != NULL) && (hex_low != NULL))
    {
        *hex_high = (uint8_t)hex_table[(value >> 4U) & 0x0FU];
        *hex_low = (uint8_t)hex_table[value & 0x0FU];
        ret = true;
    }

    return ret;
}

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
 * 
 * @example
 * uint8_t hex_str[] = "ABCD1234";
 * uint8_t bytes[4];
 * uint16_t bytes_len;
 * if (hex_str_to_bytes(hex_str, 8, bytes, 4, &bytes_len)) {
 *     // bytes[] = {0xAB, 0xCD, 0x12, 0x34}, bytes_len = 4
 * }
 */
bool hex_str_to_bytes(const uint8_t *hex_str, uint16_t hex_len, uint8_t *bytes, uint16_t bytes_size, uint16_t *bytes_len)
{
    uint16_t i = 0;
    uint16_t buf_index = 0;
    uint8_t data = 0;
    bool ret = false;
    uint16_t required_size;
    
    // Parameter validation
    if ((hex_str != NULL) && (bytes != NULL) && (bytes_len != NULL))
    {
        // Check if input length is even (two ASCII chars per byte)
        if ((hex_len > 0) && ((hex_len & 0x01) == 0))
        {
            // Check if output buffer size is sufficient
            required_size = hex_len / 2;
            if (bytes_size >= required_size)
            {
                // Initialize output length
                *bytes_len = 0;
                ret = true; // Assume success, set to false if conversion fails
                
                // Convert ASCII hexadecimal character pairs to binary data
                for (i = 0; (i < hex_len) && (ret == true); i += 2)
                {
                    if (hex_chars_to_uint8(hex_str[i], hex_str[i + 1], &data) == true)
                    {
                        bytes[buf_index++] = data;
                    }
                    else
                    {
                        // Conversion failed, set error flag
                        ret = false;
                        break;
                    }
                }
                
                if (ret == true)
                {
                    *bytes_len = buf_index;
                }
            }
        }
    }
    
    return ret;
}

/**
 * @brief Convert byte array to hexadecimal ASCII string
 * 
 * This function converts a byte array to a string of hexadecimal ASCII 
 * characters. Each byte is represented by two hexadecimal characters.
 * 
 * @param bytes Input byte array to convert
 * @param bytes_len Length of input byte array
 * @param hex_str Output hexadecimal ASCII string buffer
 * @param hex_str_size Size of output string buffer (must be at least bytes_len * 2)
 * @param hex_str_len Pointer to store actual length of output string
 * @return true Conversion successful
 * @return false Conversion failed (invalid parameters or insufficient buffer size)
 * 
 * @note Output buffer must have at least bytes_len * 2 bytes of space
 * @note Output string is not null-terminated
 * 
 * @example
 * uint8_t bytes[] = {0xAB, 0xCD, 0x12, 0x34};
 * uint8_t hex_str[8];
 * uint16_t hex_str_len;
 * if (bytes_to_hex_str(bytes, 4, hex_str, 8, &hex_str_len)) {
 *     // hex_str = "ABCD1234", hex_str_len = 8
 * }
 */
bool bytes_to_hex_str(const uint8_t *bytes, uint16_t bytes_len, uint8_t *hex_str, uint16_t hex_str_size, uint16_t *hex_str_len)
{
    uint16_t i = 0;
    uint16_t str_index = 0;
    uint8_t high = 0;
    uint8_t low = 0;
    bool ret = false;
    uint16_t required_size;
    
    // Parameter validation
    if ((bytes != NULL) && (hex_str != NULL) && (hex_str_len != NULL))
    {
        // Check if bytes_len is valid
        if (bytes_len > 0)
        {
            // Check if output buffer size is sufficient
            required_size = bytes_len * 2;
            if (hex_str_size >= required_size)
            {
                // Initialize output length
                *hex_str_len = 0;
                ret = true; // Assume success, set to false if conversion fails
                
                // Convert each byte to two hexadecimal characters
                for (i = 0; (i < bytes_len) && (ret == true); i++)
                {
                    if (uint8_to_hex_chars(bytes[i], &high, &low) == true)
                    {
                        hex_str[str_index++] = high;
                        hex_str[str_index++] = low;
                    }
                    else
                    {
                        // Conversion failed, set error flag
                        ret = false;
                        break;
                    }
                }
                
                if (ret == true)
                {
                    *hex_str_len = str_index;
                }
            }
        }
    }
    
    return ret;
}


