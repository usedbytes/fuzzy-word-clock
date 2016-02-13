#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "LPC11Uxx.h"

#include "usart.h"
#include "usb_cdc.h"
#include "util.h"

int main(void) {
	SystemInit();
	SystemCoreClockUpdate();
	SysTick_Config(SystemCoreClock/1000);

	usart_init(USART_BOOTLOADER);

	usart_send("\r\n\r\nReset!\r\n", 12);

	usb_init();

	while(1);

	return 0;
}

