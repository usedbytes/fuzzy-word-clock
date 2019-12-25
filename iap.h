#ifndef __IAP_H__
#define __IAP_H__
#include <stdint.h>

#define CMD_SUCCESS         0
#define INVALID_COMMAND     1
#define SRC_ADDR_ERROR      2
#define DST_ADDR_ERROR      3
#define SRC_ADDR_NOT_MAPPED 4
#define DST_ADDR_NOT_MAPPED 5
#define COUNT_ERROR         6
#define INVALID_SECTOR      7
#define SECTOR_NOT_BLANK    8
#define SECTOR_NOT_PREPARED_FOR_WRITE_OPERATION 9
#define COMPARE_ERROR       10
#define BUSY                11
#define PARAM_ERROR         12
#define ADDR_ERROR          13
#define ADDR_NOT_MAPPED     14
#define CMD_LOCKED          15
#define INVALID_CODE        16
#define INVALID_BAUD_RATE   17
#define INVALID_STOP_BIT    18
#define CODE_READ_PROTECTION_ENABLED 19

int iap_read_partid(uint32_t **result);
int iap_read_uid(uint32_t **result);
#endif /* __IAP_H__ */
