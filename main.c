#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "LPC11Uxx.h"

#include "usart.h"
#include "usb_cdc.h"
#include "util.h"

int main(void) {
	char buf[64];
	const char *p = (const char *)(0x000000c0);
	uint32_t start, duration;

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
		size_t len = usb_usart_recv(buf, sizeof(buf), 1000);
		usart_print("Received: '");
		usart_send(buf, len);
		usart_print("'\r\n");
		print_u32(len);
	}

	return 0;
}

