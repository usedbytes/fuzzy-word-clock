#include "iap.h"

#define IAP_LOCATION 0x1fff1ff1

typedef void (*IAP)(uint32_t *command, uint32_t *result);

IAP iap_entry = (IAP)IAP_LOCATION;

uint32_t iap_buf[6];

int iap_read_partid(uint32_t **result)
{
	iap_buf[0] = 54;
	iap_entry(iap_buf, iap_buf);
	*result = &iap_buf[1];

	return (int)iap_buf[0];
}

int iap_read_uid(uint32_t **result)
{
	iap_buf[0] = 58;
	iap_entry(iap_buf, iap_buf);
	*result = &iap_buf[1];

	return (int)iap_buf[0];
}

