#include "LPC11Uxx.h"

#include "usart.h"
#include "util.h"
#include "LPC43XX_USB.h"

#include <string.h>

#define ALIGN(__x, __to) (((__x) + (__to - 1)) & ~(__to - 1))

#define USB_MAX_IF_NUM  8
#define USB_MAX_PACKET0 64
/* Max In/Out Packet Size */
#define USB_FS_MAX_BULK_PACKET  64
/* Full speed device only */
#define USB_HS_MAX_BULK_PACKET  USB_FS_MAX_BULK_PACKET
/* IP3511 is full speed only */
#define FULL_SPEED_ONLY

#define CDC_MEM_BASE           0x20000000
#define CDC_MEM_SIZE           0x00001000
#define USB_CDC_CIF_NUM   0
#define USB_CDC_DIF_NUM   1
#define USB_CDC_EP_BULK_IN   USB_ENDPOINT_IN(2)
#define USB_CDC_EP_BULK_OUT  USB_ENDPOINT_OUT(2)
#define USB_CDC_EP_INT_IN    USB_ENDPOINT_IN(1)

#define USB_CONFIG_POWER_MA(mA)                ((mA)/2)
#define USB_CONFIGUARTION_DESC_SIZE (sizeof(USB_CONFIGURATION_DESCRIPTOR))
#define USB_DEVICE_QUALI_SIZE       (sizeof(USB_DEVICE_QUALIFIER_DESCRIPTOR))
#define USB_OTHER_SPEED_CONF_SIZE   (sizeof(USB_OTHER_SPEED_CONFIGURATION))

/* Communication device class specification version 1.10 */
#define CDC_V1_10                               0x0110

/* Communication interface class code */
/* (usbcdc11.pdf, 4.2, Table 15) */
#define CDC_COMMUNICATION_INTERFACE_CLASS       0x02

/* Communication interface class subclass codes */
/* (usbcdc11.pdf, 4.3, Table 16) */
#define CDC_DIRECT_LINE_CONTROL_MODEL           0x01
#define CDC_ABSTRACT_CONTROL_MODEL              0x02
#define CDC_TELEPHONE_CONTROL_MODEL             0x03
#define CDC_MULTI_CHANNEL_CONTROL_MODEL         0x04
#define CDC_CAPI_CONTROL_MODEL                  0x05
#define CDC_ETHERNET_NETWORKING_CONTROL_MODEL   0x06
#define CDC_ATM_NETWORKING_CONTROL_MODEL        0x07

/* Communication interface class control protocol codes */
/* (usbcdc11.pdf, 4.4, Table 17) */
#define CDC_PROTOCOL_COMMON_AT_COMMANDS         0x01

/* Data interface class code */
/* (usbcdc11.pdf, 4.5, Table 18) */
#define CDC_DATA_INTERFACE_CLASS                0x0A

/* Data interface class protocol codes */
/* (usbcdc11.pdf, 4.7, Table 19) */
#define CDC_PROTOCOL_ISDN_BRI                   0x30
#define CDC_PROTOCOL_HDLC                       0x31
#define CDC_PROTOCOL_TRANSPARENT                0x32
#define CDC_PROTOCOL_Q921_MANAGEMENT            0x50
#define CDC_PROTOCOL_Q921_DATA_LINK             0x51
#define CDC_PROTOCOL_Q921_MULTIPLEXOR           0x52
#define CDC_PROTOCOL_V42                        0x90
#define CDC_PROTOCOL_EURO_ISDN                  0x91
#define CDC_PROTOCOL_V24_RATE_ADAPTATION        0x92
#define CDC_PROTOCOL_CAPI                       0x93
#define CDC_PROTOCOL_HOST_BASED_DRIVER          0xFD
#define CDC_PROTOCOL_DESCRIBED_IN_PUFD          0xFE

/* Type values for bDescriptorType field of functional descriptors */
/* (usbcdc11.pdf, 5.2.3, Table 24) */
#define CDC_CS_INTERFACE                        0x24
#define CDC_CS_ENDPOINT                         0x25

/* Type values for bDescriptorSubtype field of functional descriptors */
/* (usbcdc11.pdf, 5.2.3, Table 25) */
#define CDC_HEADER                              0x00
#define CDC_CALL_MANAGEMENT                     0x01
#define CDC_ABSTRACT_CONTROL_MANAGEMENT         0x02
#define CDC_DIRECT_LINE_MANAGEMENT              0x03
#define CDC_TELEPHONE_RINGER                    0x04
#define CDC_REPORTING_CAPABILITIES              0x05
#define CDC_UNION                               0x06
#define CDC_COUNTRY_SELECTION                   0x07
#define CDC_TELEPHONE_OPERATIONAL_MODES         0x08
#define CDC_USB_TERMINAL                        0x09
#define CDC_NETWORK_CHANNEL                     0x0A
#define CDC_PROTOCOL_UNIT                       0x0B
#define CDC_EXTENSION_UNIT                      0x0C
#define CDC_MULTI_CHANNEL_MANAGEMENT            0x0D
#define CDC_CAPI_CONTROL_MANAGEMENT             0x0E
#define CDC_ETHERNET_NETWORKING                 0x0F
#define CDC_ATM_NETWORKING                      0x10

/* CDC class-specific request codes */
/* (usbcdc11.pdf, 6.2, Table 46) */
/* see Table 45 for info about the specific requests. */
#define CDC_SEND_ENCAPSULATED_COMMAND           0x00
#define CDC_GET_ENCAPSULATED_RESPONSE           0x01
#define CDC_SET_COMM_FEATURE                    0x02
#define CDC_GET_COMM_FEATURE                    0x03
#define CDC_CLEAR_COMM_FEATURE                  0x04
#define CDC_SET_AUX_LINE_STATE                  0x10
#define CDC_SET_HOOK_STATE                      0x11
#define CDC_PULSE_SETUP                         0x12
#define CDC_SEND_PULSE                          0x13
#define CDC_SET_PULSE_TIME                      0x14
#define CDC_RING_AUX_JACK                       0x15
#define CDC_SET_LINE_CODING                     0x20
#define CDC_GET_LINE_CODING                     0x21
#define CDC_SET_CONTROL_LINE_STATE              0x22
#define CDC_SEND_BREAK                          0x23
#define CDC_SET_RINGER_PARMS                    0x30
#define CDC_GET_RINGER_PARMS                    0x31
#define CDC_SET_OPERATION_PARMS                 0x32
#define CDC_GET_OPERATION_PARMS                 0x33
#define CDC_SET_LINE_PARMS                      0x34
#define CDC_GET_LINE_PARMS                      0x35
#define CDC_DIAL_DIGITS                         0x36
#define CDC_SET_UNIT_PARAMETER                  0x37
#define CDC_GET_UNIT_PARAMETER                  0x38
#define CDC_CLEAR_UNIT_PARAMETER                0x39
#define CDC_GET_PROFILE                         0x3A
#define CDC_SET_ETHERNET_MULTICAST_FILTERS      0x40
#define CDC_SET_ETHERNET_PMP_FILTER             0x41
#define CDC_GET_ETHERNET_PMP_FILTER             0x42
#define CDC_SET_ETHERNET_PACKET_FILTER          0x43
#define CDC_GET_ETHERNET_STATISTIC              0x44
#define CDC_SET_ATM_DATA_FORMAT                 0x50
#define CDC_GET_ATM_DEVICE_STATISTICS           0x51
#define CDC_SET_ATM_DEFAULT_VC                  0x52
#define CDC_GET_ATM_VC_STATISTICS               0x53

/* Communication feature selector codes */
/* (usbcdc11.pdf, 6.2.2..6.2.4, Table 47) */
#define CDC_ABSTRACT_STATE                      0x01
#define CDC_COUNTRY_SETTING                     0x02

/* Feature Status returned for ABSTRACT_STATE Selector */
/* (usbcdc11.pdf, 6.2.3, Table 48) */
#define CDC_IDLE_SETTING                        (1 << 0)
#define CDC_DATA_MULTPLEXED_STATE               (1 << 1)


/* Control signal bitmap values for the SetControlLineState request */
/* (usbcdc11.pdf, 6.2.14, Table 51) */
#define CDC_DTE_PRESENT                         (1 << 0)
#define CDC_ACTIVATE_CARRIER                    (1 << 1)

/* CDC class-specific notification codes */
/* (usbcdc11.pdf, 6.3, Table 68) */
/* see Table 67 for Info about class-specific notifications */
#define CDC_NOTIFICATION_NETWORK_CONNECTION     0x00
#define CDC_RESPONSE_AVAILABLE                  0x01
#define CDC_AUX_JACK_HOOK_STATE                 0x08
#define CDC_RING_DETECT                         0x09
#define CDC_NOTIFICATION_SERIAL_STATE           0x20
#define CDC_CALL_STATE_CHANGE                   0x28
#define CDC_LINE_STATE_CHANGE                   0x29
#define CDC_CONNECTION_SPEED_CHANGE             0x2A

/* UART state bitmap values (Serial state notification). */
/* (usbcdc11.pdf, 6.3.5, Table 69) */
#define CDC_SERIAL_STATE_OVERRUN                (1 << 6)  /* receive data overrun error has occurred */
#define CDC_SERIAL_STATE_PARITY                 (1 << 5)  /* parity error has occurred */
#define CDC_SERIAL_STATE_FRAMING                (1 << 4)  /* framing error has occurred */
#define CDC_SERIAL_STATE_RING                   (1 << 3)  /* state of ring signal detection */
#define CDC_SERIAL_STATE_BREAK                  (1 << 2)  /* state of break detection */
#define CDC_SERIAL_STATE_TX_CARRIER             (1 << 1)  /* state of transmission carrier */
#define CDC_SERIAL_STATE_RX_CARRIER             (1 << 0)  /* state of receiver carrier */

USBD_HANDLE_T mhUsb, mhCdc;

const USB_DEVICE_DESCRIPTOR device_descriptor = {
	.bLength = USB_DEVICE_DESC_SIZE,
	.bDescriptorType = USB_DEVICE_DESCRIPTOR_TYPE,
	.bcdUSB = 0x210,
	.bDeviceClass = USB_DEVICE_CLASS_COMMUNICATIONS, /* Class code (assigned by the USB-IF) */
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
	USB_CDC_CM_FUNC_DESC func_cm;
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
			.bInterfaceClass = 0x2,
			.bInterfaceSubClass = 0x2,
			.bInterfaceProtocol = 0,
			.iInterface = 0,
		},
		.func_hdr = {
			.bFunctionLength = sizeof(USB_CDC_HEADER_FUNC_DESC),
			.bDescriptorType = CDC_CS_INTERFACE,
			.bDescriptorSubType = CDC_HEADER,
			.bcdCDC = 0x0110,
		},
		.func_cm = {
			.bFunctionLength = sizeof(USB_CDC_CM_FUNC_DESC),
			.bDescriptorType = CDC_CS_INTERFACE,
			.bDescriptorSubType = CDC_CALL_MANAGEMENT,
			.bmCapabilities = 0x01,
			.bDataInterface = 0x01,
		},
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
			.bMasterInterface = 0,
			.bSlaveInterface0 = 1,
		},
		.ep0 = {
			.bLength = USB_ENDPOINT_DESC_SIZE,
			.bDescriptorType = USB_ENDPOINT_DESCRIPTOR_TYPE,
			.bEndpointAddress = USB_ENDPOINT_IN(1),
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
			.bInterfaceClass = 0xa,
			.bInterfaceSubClass = 0,
			.bInterfaceProtocol = 0,
			.iInterface = 0,
		},
		.ep_out = {
			.bLength = USB_ENDPOINT_DESC_SIZE,
			.bDescriptorType = USB_ENDPOINT_DESCRIPTOR_TYPE,
			.bEndpointAddress = USB_ENDPOINT_OUT(2),
			.bmAttributes = USB_ENDPOINT_TYPE_BULK,
			.wMaxPacketSize = USB_HS_MAX_BULK_PACKET,
			.bInterval = 0,
		},
		.ep_in = {
			.bLength = USB_ENDPOINT_DESC_SIZE,
			.bDescriptorType = USB_ENDPOINT_DESCRIPTOR_TYPE,
			.bEndpointAddress = USB_ENDPOINT_IN(2),
			.bmAttributes = USB_ENDPOINT_TYPE_BULK,
			.wMaxPacketSize = USB_HS_MAX_BULK_PACKET,
			.bInterval = 0,
		},
	},
	.null = 0,
};

const USB_CORE_DESCS_T core_desc = {
	.device_desc = (uint8_t *)&device_descriptor,
	.string_desc = (uint8_t *)&string_descriptors[0],
	.full_speed_desc = (uint8_t *)&desc_array.cfg,
	.high_speed_desc = (uint8_t *)&desc_array.cfg,
};

ErrorCode_t SOF_Event(USBD_HANDLE_T hUsb)
{
	return LPC_OK;
}

ErrorCode_t CIC_GetRequest(USBD_HANDLE_T hHid, USBD_SETUP_PACKET* pSetup,
		   uint8_t** pBuffer, uint16_t* length)
{

	return LPC_OK;
}

ErrorCode_t CIC_SetRequest(USBD_HANDLE_T hCdc, USBD_SETUP_PACKET* pSetup,
               uint8_t** pBuffer, uint16_t length)
{

	return LPC_OK;
}

ErrorCode_t CDC_BulkIN_Hdlr(USBD_HANDLE_T hUsb, void* data, uint32_t event)
{

	return LPC_OK;
}

ErrorCode_t CDC_BulkOUT_Hdlr(USBD_HANDLE_T hUsb, void* data,
               uint32_t event)
{

	return LPC_OK;
}

ErrorCode_t SendEncpsCmd(USBD_HANDLE_T hCDC, uint8_t* buffer, uint16_t len)
{

	return LPC_OK;
}

ErrorCode_t GetEncpsResp(USBD_HANDLE_T hCDC, uint8_t** buffer,
             uint16_t* len)
{

	return LPC_OK;
}

ErrorCode_t SetCommFeature(USBD_HANDLE_T hCDC, uint16_t feature,
             uint8_t* buffer, uint16_t len)
{

	return LPC_OK;
}

ErrorCode_t GetCommFeature(USBD_HANDLE_T hCDC, uint16_t feature,
             uint8_t** pBuffer, uint16_t* len)
{

	return LPC_OK;
}

ErrorCode_t ClrCommFeature(USBD_HANDLE_T hCDC, uint16_t feature)
{

	return LPC_OK;
}

ErrorCode_t SetCtrlLineState(USBD_HANDLE_T hCDC, uint16_t state)
{

	return LPC_OK;
}

ErrorCode_t SendBreak(USBD_HANDLE_T hCDC, uint16_t mstime)
{

	return LPC_OK;
}

ErrorCode_t SetLineCode(USBD_HANDLE_T hCDC, CDC_LINE_CODING* line_coding)
{
	USBD_API->hw->EnableEvent(mhUsb, 0, USB_EVT_SOF, 1);

	return LPC_OK;
}

ErrorCode_t VCOM_bulk_in_hdlr(USBD_HANDLE_T hUsb, void* data, uint32_t event) 
{
	usart_print("Bulk in\r\n");
	dump_mem(data, 32);
	return LPC_OK;
}

ErrorCode_t VCOM_bulk_out_hdlr(USBD_HANDLE_T hUsb, void* data, uint32_t event) 
{
	char buf[USB_HS_MAX_BULK_PACKET];
        int len = USBD_API->hw->ReadEP(hUsb, USB_CDC_EP_BULK_OUT, buf);
	usart_send(buf, len);
	return LPC_OK;
}

void USB_Handler(void)
{
	USBD_API->hw->ISR(mhUsb);
}

void usb_periph_init()
{
	/* Enable AHB clock to the GPIO domain. */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 6);

	/* Enable AHB clock to the USB block and USB RAM. */
	LPC_SYSCON->SYSAHBCLKCTRL |= (0x1 << 14) | (0x1 << 27);

	/* Pull-down is needed, or internally, VBUS will be floating. This is to
	   address the wrong status in VBUSDebouncing bit in CmdStatus register. It
	   happens on the NXP Validation Board only that a wrong ESD protection chip is used. */
	LPC_IOCON->PIO0_3 &= ~0x1F;
	//  LPC_IOCON->PIO0_3 |= ((0x1<<3)|(0x01<<0));	/* Secondary function VBUS */
	LPC_IOCON->PIO0_3 |= (0x01 << 0);			/* Secondary function VBUS */
	LPC_IOCON->PIO0_6 &= ~0x07;
	LPC_IOCON->PIO0_6 |= (0x01 << 0);			/* Secondary function SoftConn */
}

#define USB_MEM_BASE 0x20004000
#define USB_MEM_SIZE 0x800

void usb_init()
{
	USBD_API_T *rom_api = USBD_API;
	USBD_API_INIT_PARAM_T usb_param = { 0 };
	USBD_CDC_INIT_PARAM_T cdc_param = { 0 };
	ErrorCode_t ret = LPC_OK;
	uint32_t ep_indx;
	uint32_t mem_size;
	char buf[11];

	buf[8] = '\r';
	buf[9] = '\n';
	buf[10] = '\0';

	usart_print("Got USB ROM API version: ");
	u32_to_str(rom_api->version, buf);
	usart_print(buf);

	usb_periph_init();

	/* Set up USB core */
	usb_param.usb_reg_base = LPC_USB_BASE;
	usb_param.mem_base = USB_MEM_BASE;
	usb_param.max_num_ep = 3;
	/*
	 * The in-ROM MemSize calculation is wrong/limited:
	 * https://www.lpcware.com/content/forum/hw-init-and-hw-getmemsize-return-seemingly-incorrect-values
	 */
	mem_size = rom_api->hw->GetMemSize(&usb_param);
	mem_size += usb_param.max_num_ep * 2 * ALIGN(USB_MAX_PACKET0, 64);
	usb_param.mem_size = mem_size;

	usart_print("hw->Init...\r\n");
	ret = rom_api->hw->Init(&mhUsb, &core_desc, &usb_param);
	usart_print("ret: ");
	u32_to_str(ret, buf);
	usart_print(buf);

	/* Adjust mem_base as-per above forum post */
	usb_param.mem_base = USB_MEM_BASE + (mem_size - usb_param.mem_size);

	/* Init CDC params */
	cdc_param.SetLineCode = SetLineCode;
	cdc_param.SendBreak = SendBreak;
	cdc_param.cif_intf_desc = (uint8_t *)&desc_array.cif;
	cdc_param.dif_intf_desc = (uint8_t *)&desc_array.dif;
	cdc_param.mem_base = USB_MEM_BASE + mem_size;
	cdc_param.mem_size = rom_api->cdc->GetMemSize(&cdc_param);

	if ((cdc_param.mem_base + cdc_param.mem_size) >
	    (USB_MEM_BASE + USB_MEM_SIZE)) {
		usart_print("Overflows USB RAM!\r\n");
		return;
	}

	/* Initialise CDC */
	usart_print("cdc->Init...\r\n");
	ret = rom_api->cdc->init(mhUsb, &cdc_param, &mhCdc);
	usart_print("ret: ");
	u32_to_str(ret, buf);
	usart_print(buf);


	/* Set up endpoint IRQ handlers */
	usart_print("RegisterEpHandler (in)...\r\n");
	ep_indx = USB_EP_INDEX_IN(desc_array.dif.ep_in.bEndpointAddress);
	ret = rom_api->core->RegisterEpHandler(mhUsb, ep_indx, VCOM_bulk_in_hdlr, &mhCdc);
	usart_print("ret: ");
	u32_to_str(ret, buf);
	usart_print(buf);

	usart_print("RegisterEpHandler (out)...\r\n");
	ep_indx = USB_EP_INDEX_OUT(desc_array.dif.ep_out.bEndpointAddress);
	ret = rom_api->core->RegisterEpHandler (mhUsb, ep_indx, VCOM_bulk_out_hdlr, &mhCdc);
	usart_print("ret: ");
	u32_to_str(ret, buf);
	usart_print(buf);


	/* Connect to the bus */
        rom_api->hw->Connect(mhUsb, 1);

	/* Enable IRQ */
	NVIC_EnableIRQ(USB_IRQn);

	memset(&usb_param, 0, sizeof(usb_param));
	memset(&cdc_param, 0, sizeof(cdc_param));

	delay_ms(5000);

	ret = USBD_API->hw->WriteEP(mhUsb, USB_CDC_EP_BULK_IN, "Hello over USB!\r\n",
	                            17);
	usart_print("wrote: ");
	u32_to_str(ret, buf);
	usart_print(buf);
}
