/* USB CDC UART */
#ifndef __USB_CDC_H__
#define __USB_CDC_H__

int usb_init(void);

/** Send the buffer over the USB serial port.
 *
 * Blocks until the transfer is complete
 */
void usb_usart_send(const char *buf, size_t len);

/** Receive data from the USB serial port.
 *
 * Receive a maximum of 'len' bytes from the USB serial port into 'buf'
 * Returns either after the given timeout (in milliseconds) or when the
 * requested number of bytes have been received.
 *
 * A negative timeout means wait forever.
 * Returns the number of bytes received.
 */
int usb_usart_recv(char *buf, size_t len, int timeout);

/* Empty the receive buffer, discarding all data. */
void usb_usart_flush_rx(void);

/* Return "true" if the USB serial port is connected to a host */
bool usb_usart_dtr(void);

#endif /* __USB_CDC_H__ */
