
int usb_init(void);

void usb_usart_send(const char *buf, size_t len);
int usb_usart_recv(char *buf, size_t len, int timeout);
bool usb_usart_dtr(void);
