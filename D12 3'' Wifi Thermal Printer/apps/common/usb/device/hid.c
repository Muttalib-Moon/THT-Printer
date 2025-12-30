#include "os/os_api.h"
#include "usb/device/usb_stack.h"
#include "usb/device/hid.h"
#include "usb_config.h"
#include "app_config.h"
#include "UsbPrinter.h"

#include "AppDriverGeneric.h"




#define LOG_TAG_CONST       USB
#define LOG_TAG             "[USB]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"
static const u8 sHIDDescriptor[] = {
//HID
    //InterfaceDeszcriptor:
    USB_DT_INTERFACE_SIZE,     // Length
    USB_DT_INTERFACE,          // DescriptorType
    /* 0x04,                      // bInterface number */
    0x00,                       // bInterface number
    0x00,                      // AlternateSetting
    0x02,                      // NumEndpoint
    /* 0x02,                        // NumEndpoint */
    USB_CLASS_PRINTER,             // Class = Human Interface Device
    0x01,                      // Subclass, 0 No subclass, 1 Boot Interface subclass
    0x02,                      // Procotol, 0 None, 1 Keyboard, 2 Mouse
    0x00,                      // Interface Name

    //EndpointDescriptor:
    USB_DT_ENDPOINT_SIZE,       // bLength
    USB_DT_ENDPOINT,            // bDescriptorType, Type
    USB_DIR_IN | HID_EP_IN,     // bEndpointAddress
    USB_ENDPOINT_XFER_BULK,      // Interrupt
    LOBYTE(MAXP_SIZE_HIDIN), HIBYTE(MAXP_SIZE_HIDIN),// Maximum packet size
    10,     // Poll every 10msec seconds

//@Endpoint Descriptor:
     USB_DT_ENDPOINT_SIZE,       // bLength
    USB_DT_ENDPOINT,            // bDescriptorType, Type
    USB_DIR_OUT | HID_EP_OUT,   // bEndpointAddress
    USB_ENDPOINT_XFER_BULK,      // Interrupt
    LOBYTE(MAXP_SIZE_HIDOUT), HIBYTE(MAXP_SIZE_HIDOUT),// Maximum packet size
    0x01,                       // bInterval, for high speed 2^(n-1) * 125us, for full/low speed n * 1ms
};

static const u8 sHIDReportDesc[] = {
    USAGE_PAGE(1, CONSUMER_PAGE),
    USAGE(1, CONSUMER_CONTROL),
    COLLECTION(1, APPLICATION),

    LOGICAL_MIN(1, 0x00),
    LOGICAL_MAX(1, 0x01),

    USAGE(1, VOLUME_INC),
    USAGE(1, VOLUME_DEC),
    USAGE(1, MUTE),
    USAGE(1, PLAY_PAUSE),
    USAGE(1, SCAN_NEXT_TRACK),
    USAGE(1, SCAN_PREV_TRACK),
    USAGE(1, FAST_FORWARD),
    USAGE(1, STOP),

    USAGE(1, TRACKING_INC),
    USAGE(1, TRACKING_DEC),
    USAGE(1, STOP_EJECT),
    USAGE(1, VOLUME),
    USAGE(2, BALANCE_LEFT),
    USAGE(2, BALANCE_RIGHT),
    USAGE(1, PLAY),
    USAGE(1, PAUSE),

    REPORT_SIZE(1, 0x01),
    REPORT_COUNT(1, 0x10),
    INPUT(1, 0x42),
    END_COLLECTION,
};

static u32 get_hid_report_desc_len(u32 index)
{
    u32 len = 0;
    len = sizeof(sHIDReportDesc);
    return len;
}
static void *get_hid_report_desc(u32 index)
{
    u8 *ptr  = NULL;
    ptr = (u8 *)sHIDReportDesc;
    return ptr;
}
/*

static u32 hid_tx_data(struct usb_device_t *usb_device, const u8 *buffer, u32 len)
{
    const usb_dev usb_id = usb_device2id(usb_device);
    return usb_g_intr_write(usb_id, HID_EP_IN, buffer, len);
}
*/
#if 0
static void USB_rx_data(struct usb_device_t *usb_device, u32 ep)
{
    /* const usb_dev usb_id = usb_device2id(usb_device); */
    /* u32 rx_len = usb_g_intr_read(usb_id, ep, NULL, 64, 0); */
    /* hid_tx_data(usb_id, hid_ep_out_dma[usb_id], rx_len); */
	 u8 ep_buffer[MAXP_SIZE_HIDOUT];
	u32 rx_len = usb_g_bulk_read(usb_device2id(usb_device), ep, ep_buffer, MAXP_SIZE_HIDOUT,0);
}
#endif
extern void USBPrinterReveiveDataIsr(struct usb_device_t *UsbDevice, u32 Ep);
static void hid_endpoint_init(struct usb_device_t *usb_device, u32 itf)
{
    const usb_dev usb_id = usb_device2id(usb_device);
    printf("hid_endpoint_init----------%d\r\n",itf);
    u8 *ep_buffer = usb_get_ep_buffer(usb_id, HID_EP_IN | USB_DIR_IN);
    usb_g_ep_config(usb_id, HID_EP_IN | USB_DIR_IN, USB_ENDPOINT_XFER_BULK, 0, ep_buffer, MAXP_SIZE_HIDIN);

    ep_buffer = usb_get_ep_buffer(usb_id, HID_EP_OUT);
    //usb_g_set_intr_hander(usb_id, HID_EP_OUT, hid_rx_data);
    //usb_g_set_intr_hander(usb_id, HID_EP_OUT, USB_rx_data);//注册回调接受函数
    usb_g_set_intr_hander(usb_id, HID_EP_OUT, USBPrinterReveiveDataIsr);//注册回调接受函数
    usb_g_ep_config(usb_id, HID_EP_OUT, USB_ENDPOINT_XFER_BULK, 1, ep_buffer, MAXP_SIZE_HIDOUT);
    usb_enable_ep(usb_id, HID_EP_OUT);



}

static void hid_reset(struct usb_device_t *usb_device, u32 itf)
{
    const usb_dev usb_id = usb_device2id(usb_device);

#if USB_ROOT2
    usb_disable_ep(usb_id, HID_EP_OUT);
#else
printf("hid_reset----%d\r\n",itf);

    hid_endpoint_init(usb_device, itf);
#endif
}
static u32 hid_itf_hander(struct usb_device_t *usb_device, struct usb_ctrlrequest *req)
{
    const usb_dev usb_id = usb_device2id(usb_device);
    u32 tx_len;
    u8 *tx_payload = usb_get_setup_buffer(usb_device);
    u32 bRequestType = req->bRequestType & USB_TYPE_MASK;

    switch (bRequestType) {
    case USB_TYPE_STANDARD:
        switch (req->bRequest) {
        case USB_REQ_GET_DESCRIPTOR:
            switch (HIBYTE(req->wValue)) {
            case USB_HID_DT_HID:
				 tx_payload = (u8 *)sHIDDescriptor + USB_DT_INTERFACE_SIZE;
				/*
                tx_payload = (u8 *)sHIDDescriptor + USB_DT_INTERFACE_SIZE;
                tx_len = 9;
                tx_payload = usb_set_data_payload(usb_device, req, tx_payload, tx_len);
                tx_payload[7] = LOBYTE(get_hid_report_desc_len(req->wIndex));
                tx_payload[8] = HIBYTE(get_hid_report_desc_len(req->wIndex));
				*/
                break;
            case USB_HID_DT_REPORT:

                printf("USB_HID_DT_REPORT----%d\r\n",tx_len);
                hid_endpoint_init(usb_device, req->wIndex);
                tx_len = get_hid_report_desc_len(req->wIndex);
                tx_payload = get_hid_report_desc(req->wIndex);
                usb_set_data_payload(usb_device, req, tx_payload, tx_len);
                break;
            }// USB_REQ_GET_DESCRIPTOR
            break;
        case USB_REQ_SET_DESCRIPTOR:
            usb_set_setup_phase(usb_device, USB_EP0_STAGE_SETUP);
            break;
        case USB_REQ_SET_INTERFACE:
            if (usb_device->bDeviceStates == USB_DEFAULT) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_ADDRESS) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_CONFIGURED) {
                //只有一个interface 没有Alternate
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            }
            break;
        case USB_REQ_GET_INTERFACE:
            if (req->wLength) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_DEFAULT) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_ADDRESS) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_CONFIGURED) {
                tx_len = 1;
                tx_payload[0] = 0x00;
                usb_set_data_payload(usb_device, req, tx_payload, tx_len);
            }
            break;
        case USB_REQ_GET_STATUS:
            if (usb_device->bDeviceStates == USB_DEFAULT) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_ADDRESS) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            }
            break;
        }//bRequest @ USB_TYPE_STANDARD
        break;

    case USB_TYPE_CLASS: {
        switch (req->bRequest) {
        case USB_REQ_SET_IDLE:
            usb_set_setup_phase(usb_device, USB_EP0_STAGE_SETUP);
            break;
        case USB_REQ_GET_IDLE:
            tx_len = 1;
            tx_payload[0] = 0;
            usb_set_data_payload(usb_device, req, tx_payload, tx_len);
            break;
        }//bRequest @ USB_TYPE_CLASS
    }
    break;
    }
    return 0;
}

u32 hid_desc_config(const usb_dev usb_id, u8 *ptr, u32 *cur_itf_num)
{
   // printf("hid interface num:%d\n", *cur_itf_num);
    u8 *_ptr = ptr;
    memcpy(ptr, sHIDDescriptor, sizeof(sHIDDescriptor));
    ptr[2] = *cur_itf_num;
    if (usb_set_interface_hander(usb_id, *cur_itf_num, hid_itf_hander) != *cur_itf_num) {

    }
    if (usb_set_reset_hander(usb_id, *cur_itf_num, hid_reset) != *cur_itf_num) {

    }
    *cur_itf_num = *cur_itf_num + 1;
    return sizeof(sHIDDescriptor) ;
}
/*
u32 msd_desc_config(const usb_dev usb_id, u8 *ptr, u32 *cur_itf_num)
{
    log_debug("%s() %d", __func__, *cur_itf_num);
    memcpy(ptr, sMassDescriptor, sizeof(sMassDescriptor));
    ptr[2] = *cur_itf_num;
    if (usb_set_interface_hander(usb_id, *cur_itf_num, msd_itf_hander) != *cur_itf_num) {

    }
    if (usb_set_reset_hander(usb_id, *cur_itf_num, msd_reset_wakeup) != *cur_itf_num) {

    }
    *cur_itf_num = *cur_itf_num + 1;
    return sizeof(sMassDescriptor);
}
*/
void hid_key_handler(struct usb_device_t *usb_device, u32 hid_key)
{
;
}

struct hid_button {
    u8 report_id;
    u8 button1: 1;
    u8 button2: 1;
    u8 button3: 1;
    u8 no_button: 5;
    u8 X_axis;
    u8 Y_axis;
};
struct hid_button hid_key;
void hid_test(struct usb_device_t *usb_device)
{
    static u32 tx_count = 0;

    hid_key_handler(usb_device, BIT(tx_count));
    tx_count ++;
    if (BIT(tx_count) > USB_AUDIO_PAUSE) {
        tx_count = 0;
    }
}
#include "asm/power_interface.h"
////////////////////////////////////////////////////////////////////////////////////////////////////

