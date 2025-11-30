/**
 * @file drv_status.h
 * @brief Common driver status codes used by peripheral drivers
 *
 * A small, portable set of status codes that drivers in this project can
 * return. Keep the set intentionally small; drivers may provide extended
 * error information via other mechanisms if required.
 */
#ifndef DRV_STATUS_H
#define DRV_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum drv_status {
    DRV_OK = 0,        /**< success */
    DRV_ERROR,         /**< generic failure */
    DRV_TIMEOUT,       /**< operation timed out */
    DRV_INVALID_ARG,   /**< invalid parameter */
    DRV_NO_MEMORY,     /**< allocation failed / out of memory */
    DRV_BUSY           /**< resource busy */
} drv_status_t;

#ifdef __cplusplus
}
#endif

#endif /* DRV_STATUS_H */
