#include "LPC11Uxx.h"

#include "iap.h"

#define IAP_LOCATION 0x1fff1ff1

typedef void (* const IAP)(uint32_t *command, uint32_t *result);

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

int iap_eeprom_write(uint32_t eeprom_addr, void *data, uint32_t len)
{
	iap_buf[0] = 61;
	iap_buf[1] = eeprom_addr;
	iap_buf[2] = (uint32_t)((uintptr_t)data);
	iap_buf[3] = len;
	iap_buf[4] = SystemCoreClock / 1000;

	iap_entry(iap_buf, iap_buf);

	return (int)iap_buf[0];
}

int iap_eeprom_read(uint32_t eeprom_addr, void *data, uint32_t len)
{
	iap_buf[0] = 62;
	iap_buf[1] = eeprom_addr;
	iap_buf[2] = (uint32_t)((uintptr_t)data);
	iap_buf[3] = len;
	iap_buf[4] = SystemCoreClock / 1000;

	iap_entry(iap_buf, iap_buf);

	return (int)iap_buf[0];
}
