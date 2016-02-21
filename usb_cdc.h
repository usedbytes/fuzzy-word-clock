/* USB CDC UART */

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

/* Return "true" if the USB serial port is connected to a host */
bool usb_usart_dtr(void);
