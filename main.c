#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "LPC11Uxx.h"

#include "usart.h"
#include "usb_cdc.h"
#include "util.h"

int main(void) {
	const char *p = (const char *)(0x000000c0);
	char buf[11];
	uint32_t start, duration;

	buf[8] = '\r';
	buf[9] = '\n';
	buf[10] = '\0';
	SystemInit();
	SystemCoreClockUpdate();
	SysTick_Config(SystemCoreClock/1000);

	usart_init(USART_BOOTLOADER);

	usart_send("\r\n\r\nReset!\r\n", 12);

	usb_init();

	usart_print("Waiting for DTR...");
	while(!usb_usart_dtr());
	usart_print("Done\r\n");

	while(1) {
		int i = 1024;
		start = msTicks;
		while (i--)
			usb_usart_send(p, 1024);
		duration = msTicks - start;
		usart_print("Transferred 1M in ");
		print_u32(duration);

		delay_ms(10000);
	}

	return 0;
}

