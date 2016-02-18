#include <string.h>
#include <stdbool.h>

#include "LPC11Uxx.h"

#include "usart.h"
#include "util.h"
#include "LPC43XX_USB.h"

#define ALIGN(__x, __to) (((__x) + (__to - 1)) & ~(__to - 1))

#define USB_MAX_IF_NUM  8
#define USB_MAX_PACKET0 64
/* Max In/Out Packet Size */
#define USB_FS_MAX_BULK_PACKET  64
/* Full speed device only */
#define USB_HS_MAX_BULK_PACKET  USB_FS_MAX_BULK_PACKET

#define USB_MEM_BASE 0x20004000
#define USB_MEM_SIZE 0x800
#define USB_NUM_ENDPOINTS 3

#define USB_CDC_CIF_NUM   0
#define USB_CDC_DIF_NUM   1
#define USB_CDC_EP_DIF 2
#define USB_CDC_EP_CIF 1
#define USB_CDC_EP_BULK_IN   USB_ENDPOINT_IN(USB_CDC_EP_DIF)
#define USB_CDC_EP_BULK_OUT  USB_ENDPOINT_OUT(USB_CDC_EP_DIF)
#define USB_CDC_EP_INT_IN    USB_ENDPOINT_IN(USB_CDC_EP_CIF)

#define CDC_NO_CALL_MGMT_FUNC_DESC

struct usb_ctx {
	USBD_HANDLE_T core_hnd;
	USBD_HANDLE_T cdc_hnd;
	volatile bool dtr;

	volatile size_t tx_len;
	volatile uint8_t *volatile tx_buf;

	volatile uint8_t rx_buf[128];
	volatile uint8_t head, tail;
	volatile bool overrun;
};

struct usb_ctx usb_ctx;

const USB_DEVICE_DESCRIPTOR device_descriptor = {
	.bLength = USB_DEVICE_DESC_SIZE,
	.bDescriptorType = USB_DEVICE_DESCRIPTOR_TYPE,
	.bcdUSB = 0x200,
	.bDeviceClass = USB_DEVICE_CLASS_COMMUNICATIONS,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = USB_MAX_PACKET0,
	/* pid.codes test code: http://pid.codes/1209/0001/ */
	.idVendor = 0x1209,
	.idProduct = 0x0001,
	.bcdDevice = 0x0001,
	.iManufacturer = 0x01,
	.iProduct = 0x02,
	.iSerialNumber = 0x03,
	.bNumConfigurations = 0x01,
};

const uint8_t string_descriptors[] = {
	/* Index 0: LANGID */
		/* .bLength = */ 0x04,
		/* bDescriptorType = */ USB_STRING_DESCRIPTOR_TYPE,
		/* bString = */ 0x08, 0x09,
	/* Index 1: Manufacturer */
		/* .bLength = */ 0x14,
		/* bDescriptorType = */ USB_STRING_DESCRIPTOR_TYPE,
		/* bString = */ 'u', 0, 's', 0, 'e', 0, 'd', 0,
				'b', 0, 'y', 0, 't', 0, 'e', 0, 's', 0,
	/* Index 2: Product */
		/* .bLength = */ 0x16,
		/* bDescriptorType = */ USB_STRING_DESCRIPTOR_TYPE,
		/* bString = */ 'f', 0, 'u', 0, 'z', 0, 'z', 0, 'y', 0,
				'c', 0, 'l', 0, 'o', 0, 'c', 0, 'k', 0,
	/* Index 3: Serial */
		/* .bLength = */ 0xa,
		/* bDescriptorType = */ USB_STRING_DESCRIPTOR_TYPE,
		/* bString = */ '0', 0, '0', 0, '0', 0, '0', 0,
};

/* USB Configuration Descriptor */
struct __attribute__((packed)) cdc_cif_descriptor {
	USB_INTERFACE_DESCRIPTOR intf;
	USB_CDC_HEADER_FUNC_DESC func_hdr;
#ifndef CDC_NO_CALL_MGMT_FUNC_DESC
	/*
	 * Seems like the ROM driver can't reliably handle a
	 * config descriptor larger than 64 bytes (1 packet).
	 * The Call Management Functional Descriptor seems like
	 * the least useful one, so we can opt to omit it to
	 * reduce the descriptor size
	 *
	 * More info: https://www.lpcware.com/content/forum/usbd-get-device-configuration-descriptor-truncated-packets
	 *
	 * TODO: It should be possible to override the GetDescriptor
	 * Config request, and possibly work-around this limitation.
	 */
	USB_CDC_CM_FUNC_DESC func_cm;
#endif
	USB_CDC_ACM_FUNC_DESC func_acm;
	USB_CDC_UNION_FUNC_DESC func_union;
	USB_ENDPOINT_DESCRIPTOR ep0;
};

struct __attribute__((packed)) cdc_dif_descriptor {
	USB_INTERFACE_DESCRIPTOR intf;
	USB_ENDPOINT_DESCRIPTOR ep_out;
	USB_ENDPOINT_DESCRIPTOR ep_in;
};

struct __attribute__((packed)) cdc_descriptor_array {
	USB_CONFIGURATION_DESCRIPTOR cfg;
	struct cdc_cif_descriptor cif;
	struct cdc_dif_descriptor dif;
	uint8_t null;
};

const struct cdc_descriptor_array desc_array = {
	.cfg = {
		.bLength = USB_CONFIGURATION_DESC_SIZE,
		.bDescriptorType = USB_CONFIGURATION_DESCRIPTOR_TYPE,
		.wTotalLength = sizeof(struct cdc_descriptor_array) - 1,
		.bNumInterfaces = 0x02,
		.bConfigurationValue = 0x01,
		.iConfiguration = 0,
		.bmAttributes = USB_CONFIG_BUS_POWERED,
		.bMaxPower = USB_CONFIG_POWER_MA(100),
	},
	.cif = {
		.intf = {
			.bLength = USB_INTERFACE_DESC_SIZE,
			.bDescriptorType = USB_INTERFACE_DESCRIPTOR_TYPE,
			.bInterfaceNumber = 0,
			.bAlternateSetting = 0,
			.bNumEndpoints = 0x1,
			.bInterfaceClass = CDC_COMMUNICATION_INTERFACE_CLASS,
			.bInterfaceSubClass = CDC_ABSTRACT_CONTROL_MODEL,
			.bInterfaceProtocol = 0,
			.iInterface = 0,
		},
		.func_hdr = {
			.bFunctionLength = sizeof(USB_CDC_HEADER_FUNC_DESC),
			.bDescriptorType = CDC_CS_INTERFACE,
			.bDescriptorSubType = CDC_HEADER,
			.bcdCDC = 0x0110,
		},
#ifndef CDC_NO_CALL_MGMT_FUNC_DESC
		.func_cm = {
			.bFunctionLength = sizeof(USB_CDC_CM_FUNC_DESC),
			.bDescriptorType = CDC_CS_INTERFACE,
			.bDescriptorSubType = CDC_CALL_MANAGEMENT,
			.bmCapabilities = 0x01,
			.bDataInterface = 0x01,
		},
#endif
		.func_acm = {
			.bFunctionLength = sizeof(USB_CDC_ACM_FUNC_DESC),
			.bDescriptorType = CDC_CS_INTERFACE,
			.bDescriptorSubType = CDC_ABSTRACT_CONTROL_MANAGEMENT,
			.bmCapabilities = 0x02,
		},
		.func_union = {
			.bFunctionLength = sizeof(USB_CDC_UNION_FUNC_DESC),
			.bDescriptorType = CDC_CS_INTERFACE,
			.bDescriptorSubType = CDC_UNION,
			.bMasterInterface = USB_CDC_CIF_NUM,
			.bSlaveInterface0 = USB_CDC_DIF_NUM,
		},
		.ep0 = {
			.bLength = USB_ENDPOINT_DESC_SIZE,
			.bDescriptorType = USB_ENDPOINT_DESCRIPTOR_TYPE,
			.bEndpointAddress = USB_CDC_EP_INT_IN,
			.bmAttributes = USB_ENDPOINT_TYPE_INTERRUPT,
			.wMaxPacketSize = 0x0010,
			.bInterval = 0x02,
		},
	},
	.dif = {
		.intf = {
			.bLength = USB_INTERFACE_DESC_SIZE,
			.bDescriptorType = USB_INTERFACE_DESCRIPTOR_TYPE,
			.bInterfaceNumber = 1,
			.bAlternateSetting = 0,
			.bNumEndpoints = 0x2,
			.bInterfaceClass = CDC_DATA_INTERFACE_CLASS,
			.bInterfaceSubClass = 0,
			.bInterfaceProtocol = 0,
			.iInterface = 0,
		},
		.ep_out = {
			.bLength = USB_ENDPOINT_DESC_SIZE,
			.bDescriptorType = USB_ENDPOINT_DESCRIPTOR_TYPE,
			.bEndpointAddress = USB_CDC_EP_BULK_OUT,
			.bmAttributes = USB_ENDPOINT_TYPE_BULK,
			.wMaxPacketSize = USB_HS_MAX_BULK_PACKET,
			.bInterval = 0,
		},
		.ep_in = {
			.bLength = USB_ENDPOINT_DESC_SIZE,
			.bDescriptorType = USB_ENDPOINT_DESCRIPTOR_TYPE,
			.bEndpointAddress = USB_CDC_EP_BULK_IN,
			.bmAttributes = USB_ENDPOINT_TYPE_BULK,
			.wMaxPacketSize = USB_HS_MAX_BULK_PACKET,
			.bInterval = 0,
		},
	},
	.null = 0,
};

const USB_CORE_DESCS_T core_desc = {
	.device_desc = (const uint8_t *)&device_descriptor,
	.string_desc = (const uint8_t *)&string_descriptors[0],
	.full_speed_desc = (const uint8_t *)&desc_array.cfg,
	.high_speed_desc = (const uint8_t *)&desc_array.cfg,
};

ErrorCode_t CDC_BulkIN_Hdlr(USBD_HANDLE_T hUsb, void* data, uint32_t event)
{
	static bool done = false;

	if ((event == USB_EVT_IN) && usb_ctx.tx_buf) {
		uint32_t send = usb_ctx.tx_len;

		if (done) {
			usb_ctx.tx_buf = NULL;
			done = false;
			return LPC_OK;
		}

		if (send > USB_MAX_PACKET0)
		       send = USB_MAX_PACKET0;

		send = USBD_WriteEP(usb_ctx.core_hnd, USB_CDC_EP_BULK_IN,
				    usb_ctx.tx_buf, send);
		usb_ctx.tx_len -= send;
		usb_ctx.tx_buf += send;

		/*
		 * If the last packet was full (USB_MAX_PACKET0), then
		 * we need to go around one more time to send a zero-length
		 * packet, so don't set 'done'
		 * Otherwise (or after our zero-length-packet), the next
		 * interrupt will mark the end of transmission, so we can
		 * set 'done' now and clear tx_buf at the next IRQ.
		 */
		if (!usb_ctx.tx_len && (send < USB_MAX_PACKET0))
			done = true;
	}

	return LPC_OK;
}

ErrorCode_t CDC_BulkOUT_Hdlr(USBD_HANDLE_T hUsb, void* data,
               uint32_t event)
{
	char buf[USB_HS_MAX_BULK_PACKET];
        int len = USBD_API->hw->ReadEP(hUsb, USB_CDC_EP_BULK_OUT, buf);
	usart_send(buf, len);
	return LPC_OK;
}

ErrorCode_t SetCtrlLineState(USBD_HANDLE_T hCDC, uint16_t state)
{
	usart_print("LineState\r\n");
	dump_mem(&state, 2);
	usb_ctx.dtr = (state & 0x1);

	/* Terminate any ongoing transmission */
	if (!usb_ctx.dtr) {
		usb_ctx.tx_len = 0;
		usb_ctx.tx_buf = NULL;
	}

	return LPC_OK;
}

ErrorCode_t SendBreak(USBD_HANDLE_T hCDC, uint16_t mstime)
{
	usart_print("SendBreak:\r\n");
	dump_mem(&mstime, sizeof(mstime));

	return LPC_OK;
}

void USB_Handler(void)
{
	USBD_API->hw->ISR(usb_ctx.core_hnd);
}

void usb_periph_init()
{
	/* GPIO Clock */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 6);

	/* USB and USB RAM */
	LPC_SYSCON->SYSAHBCLKCTRL |= (0x1 << 14) | (0x1 << 27);

	/* VBUS */
	LPC_IOCON->PIO0_3 &= ~0x1F;
	LPC_IOCON->PIO0_3 |= (0x01 << 0);

	/* Connect */
	LPC_IOCON->PIO0_6 &= ~0x07;
	LPC_IOCON->PIO0_6 |= (0x01 << 0);
}

void usb_periph_exit()
{
	/* Just disable the USB clocks */
	LPC_SYSCON->SYSAHBCLKCTRL &= ~((0x1 << 14) | (0x1 << 27));
}

ErrorCode_t usb_core_init(uint32_t mem_base, uint32_t *mem_size)
{
	USBD_API_INIT_PARAM_T usb_param = { 0 };
	ErrorCode_t ret;
	uint32_t req_size;

	usb_param.usb_reg_base = LPC_USB_BASE;
	usb_param.mem_base = mem_base;
	usb_param.max_num_ep = USB_NUM_ENDPOINTS;

	/*
	 * The in-ROM MemSize calculation is wrong/limited:
	 * https://www.lpcware.com/content/forum/hw-init-and-hw-getmemsize-return-seemingly-incorrect-values
	 */
	req_size = USBD_GetMemSize(&usb_param);
	req_size += usb_param.max_num_ep * 2 * ALIGN(USB_MAX_PACKET0, 64);
	usb_param.mem_size = req_size;

	if (req_size > *mem_size) {
		usart_print("Not enough space for core API\r\n");
		return ERR_FAILED;
	}
	*mem_size = req_size;

	ret = USBD_Init(&usb_ctx.core_hnd, &core_desc, &usb_param);
	if (ret)
		return ret;

	if (usb_param.mem_size) {
		usart_print("Unexpected mem_size. usb_param:\r\n");
		dump_mem(&usb_param, sizeof(usb_param));
		return ERR_FAILED;
	}

	/*
	 * FIXME: Not sure if we need to keep the usb_param around.
	 * It seems like no, but this should hopefully cause a crash if
	 * that turns out not to be the case
	 */
	memset(&usb_param, 0, sizeof(usb_param));

	return LPC_OK;
}

int usb_init()
{
	USBD_CDC_INIT_PARAM_T cdc_param = { 0 };
	ErrorCode_t ret;
	uint32_t mem_size = USB_MEM_SIZE;
	uint32_t ep;
	char buf[11];

	buf[8] = '\r';
	buf[9] = '\n';
	buf[10] = '\0';

	usart_print("Got USB ROM API version: ");
	u32_to_str(USBD_API->version, buf);
	usart_print(buf);

	usb_periph_init();

	ret = usb_core_init(USB_MEM_BASE, &mem_size);
	if (ret != LPC_OK) {
		usart_print("usb_core_init failed: ");
		u32_to_str(ret, buf);
		usart_print(buf);
		goto error;
	}

	/* Init CDC params */
	cdc_param.SendBreak = SendBreak;
	cdc_param.SetCtrlLineState = SetCtrlLineState;
	cdc_param.cif_intf_desc = (uint8_t *)&desc_array.cif;
	cdc_param.dif_intf_desc = (uint8_t *)&desc_array.dif;
	cdc_param.mem_base = USB_MEM_BASE + mem_size;
	cdc_param.mem_size = USBD_CDC_GetMemSize(&cdc_param);

	if ((cdc_param.mem_base + cdc_param.mem_size) >
	    (USB_MEM_BASE + USB_MEM_SIZE)) {
		usart_print("Overflows USB RAM!\r\n");
		goto error;
	}

	/* Initialise CDC */
	ret = USBD_CDC_Init(usb_ctx.core_hnd, &cdc_param, &usb_ctx.cdc_hnd);
	if (ret != LPC_OK) {
		usart_print("CDC Init failed: ");
		u32_to_str(ret, buf);
		usart_print(buf);
		goto error;
	}

	ep = USB_EP_INDEX_IN(USB_CDC_EP_DIF);
	ret = USBD_RegisterEpHandler(usb_ctx.core_hnd, ep, CDC_BulkIN_Hdlr, &usb_ctx);
	if (ret != LPC_OK) {
		usart_print("RegisterEpHandler (in) failed: ");
		u32_to_str(ret, buf);
		usart_print(buf);
		goto error;
	}

	ep = USB_EP_INDEX_OUT(USB_CDC_EP_DIF);
	ret = USBD_RegisterEpHandler(usb_ctx.core_hnd, ep, CDC_BulkOUT_Hdlr, &usb_ctx);
	if (ret != LPC_OK) {
		usart_print("RegisterEpHandler (out) failed: ");
		u32_to_str(ret, buf);
		usart_print(buf);
		goto error;
	}

	/* Enable IRQ */
	NVIC_SetPriority(USB_IRQn, 0);
	NVIC_EnableIRQ(USB_IRQn);

	/* Connect to the bus */
        USBD_Connect(usb_ctx.core_hnd, 1);

	/*
	 * FIXME: Not sure if we need to keep the cdc_param around.
	 * It seems like no, but this should hopefully cause a crash if
	 * that turns out not to be the case
	 */
	memset(&cdc_param, 0, sizeof(cdc_param));

	return 0;

error:
	usb_periph_exit();
	return -1;
}

void usb_usart_send(const char *buf, size_t len)
{
	if (!usb_ctx.dtr)
		return;

	NVIC_DisableIRQ(USB_IRQn);
	usb_ctx.tx_buf = buf;
	usb_ctx.tx_len = len;
	/*
	 * Call the interrupt handler directly to send the first
	 * packet
	 */
	CDC_BulkIN_Hdlr(usb_ctx.core_hnd, &usb_ctx, USB_EVT_IN);
	NVIC_EnableIRQ(USB_IRQn);

	/* Wait for finish */
	while(usb_ctx.tx_buf != NULL);
}

bool usb_usart_dtr(void)
{
	return usb_ctx.dtr;
}
