/**
 * \file
 *
 * \brief USB Device Stack HID Generic Function Implementation.
 *
 * Copyright (c) 2015-2018 Microchip Technology Inc. and its subsidiaries.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip
 * software and any derivatives exclusively with Microchip products.
 * It is your responsibility to comply with third party license terms applicable
 * to your use of third party software (including open source software) that
 * may accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
 * INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
 * LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
 * SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
 * POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
 * RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * \asf_license_stop
 *
 */

#include "hiddf_generic.h"
#include <string.h>

#define HIDDF_GENERIC_VERSION 0x00000001u

/** USB Device HID Generic Function Specific Data */
struct hiddf_generic_func_data {
	/** HID Descriptor */
	uint8_t *hid_desc;
	/** HID Device Generic Report Descriptor */
	const uint8_t *report_desc;
	/** HID Device Generic Report Descriptor Length */
	uint32_t report_desc_len;
	/** HID Device Generic Interface information */
	uint8_t func_iface;
	/** HID Device Generic IN Endpoint */
	uint8_t func_ep_in;
	/** HID Device Generic OUT Endpoint */
	uint8_t func_ep_out;
	/** HID Device Generic protocol */
	uint8_t protocol;
	/** HID Device Generic Enable Flag */
	bool enabled;
};

/* USB Device HID Generic Function Instance */
static struct usbdf_driver _hiddf_generic;

/* USB Device HID Generic Function Data Instance */
static struct hiddf_generic_func_data _hiddf_generic_funcd;

/* USB Device HID Generic Set Report Function Callback */
static hiddf_generic_set_report_t hiddf_generic_set_report = NULL;

/* USB Device HID Generic Set Report Function Callback */
static hiddf_generic_get_ctrl_report_t hiddf_generic_get_ctrl_report = NULL;

/* USB Device HID Generic Set Report Function Callback */
static hiddf_generic_set_ctrl_report_t hiddf_generic_set_ctrl_report = NULL;

/**
 * \brief Enable HID Generic Function
 * \param[in] drv Pointer to USB device function driver
 * \param[in] desc Pointer to USB interface descriptor
 * \return Operation status.
 */
static int32_t hid_generic_enable(struct usbdf_driver *drv, struct usbd_descriptors *desc)
{
	uint8_t *        ifc, *ep, i;
	usb_iface_desc_t ifc_desc;
	usb_ep_desc_t    ep_desc;

	struct hiddf_generic_func_data *func_data = (struct hiddf_generic_func_data *)(drv->func_data);

	ifc = desc->sod;
	if (NULL == ifc) {
		return ERR_NOT_FOUND;
	}

	ifc_desc.bInterfaceNumber = ifc[2];
	ifc_desc.bInterfaceClass  = ifc[5];

	if (HID_CLASS == ifc_desc.bInterfaceClass) {
		if (func_data->func_iface == ifc_desc.bInterfaceNumber) { // Initialized
			return ERR_ALREADY_INITIALIZED;
		} else if (func_data->func_iface != 0xFF) { // Occupied
			return ERR_NO_RESOURCE;
		} else {
			func_data->func_iface = ifc_desc.bInterfaceNumber;
		}
	} else { // Not supported by this function driver
		return ERR_NOT_FOUND;
	}

	// Install HID descriptor
	_hiddf_generic_funcd.hid_desc = usb_find_desc(usb_desc_next(desc->sod), desc->eod, USB_DT_HID);

	// Install endpoints
	for (i = 0; i < 2; i++) {
		ep        = usb_find_ep_desc(usb_desc_next(desc->sod), desc->eod);
		desc->sod = ep;
		if (NULL != ep) {
			ep_desc.bEndpointAddress = ep[2];
			ep_desc.bmAttributes     = ep[3];
			ep_desc.wMaxPacketSize   = usb_get_u16(ep + 4);
			if (usb_d_ep_init(ep_desc.bEndpointAddress, ep_desc.bmAttributes, ep_desc.wMaxPacketSize)) {
				return ERR_NOT_INITIALIZED;
			}
			if (ep_desc.bEndpointAddress & USB_EP_DIR_IN) {
				func_data->func_ep_in = ep_desc.bEndpointAddress;
				usb_d_ep_enable(func_data->func_ep_in);
			} else {
				func_data->func_ep_out = ep_desc.bEndpointAddress;
				usb_d_ep_enable(func_data->func_ep_out);
			}
		} else {
			return ERR_NOT_FOUND;
		}
	}

	// Installed
	_hiddf_generic_funcd.protocol = 1;
	_hiddf_generic_funcd.enabled  = true;
	return ERR_NONE;
}

/**
 * \brief Disable HID Generic Function
 * \param[in] drv Pointer to USB device function driver
 * \param[in] desc Pointer to USB device descriptor
 * \return Operation status.
 */
static int32_t hid_generic_disable(struct usbdf_driver *drv, struct usbd_descriptors *desc)
{
	struct hiddf_generic_func_data *func_data = (struct hiddf_generic_func_data *)(drv->func_data);

	usb_iface_desc_t ifc_desc;

	if (desc) {
		ifc_desc.bInterfaceClass = desc->sod[5];
		if (ifc_desc.bInterfaceClass != HID_CLASS) {
			return ERR_NOT_FOUND;
		}
	}

	if (func_data->func_iface != 0xFF) {
		func_data->func_iface = 0xFF;
	}

	if (func_data->func_ep_in != 0xFF) {
		usb_d_ep_deinit(func_data->func_ep_in);
		func_data->func_ep_in = 0xFF;
	}

	if (func_data->func_ep_out != 0xFF) {
		usb_d_ep_deinit(func_data->func_ep_out);
		func_data->func_ep_out = 0xFF;
	}

	_hiddf_generic_funcd.enabled = false;
	return ERR_NONE;
}

/**
 * \brief HID Generic Control Function
 * \param[in] drv Pointer to USB device function driver
 * \param[in] ctrl USB device general function control type
 * \param[in] param Parameter pointer
 * \return Operation status.
 */
static int32_t hid_generic_ctrl(struct usbdf_driver *drv, enum usbdf_control ctrl, void *param)
{
	switch (ctrl) {
	case USBDF_ENABLE:
		return hid_generic_enable(drv, (struct usbd_descriptors *)param);

	case USBDF_DISABLE:
		return hid_generic_disable(drv, (struct usbd_descriptors *)param);

	case USBDF_GET_IFACE:
		return ERR_UNSUPPORTED_OP;

	default:
		return ERR_INVALID_ARG;
	}
}

/**
 * \brief Process the HID class get descriptor
 * \param[in] ep Endpoint address.
 * \param[in] req Pointer to the request.
 * \return Operation status.
 */
static int32_t hid_generic_get_desc(uint8_t ep, struct usb_req *req)
{
	switch (req->wValue >> 8) {
	case USB_DT_HID:
		return usbdc_xfer(ep, _hiddf_generic_funcd.hid_desc, _hiddf_generic_funcd.hid_desc[0], false);
	case USB_DT_HID_REPORT:
		return usbdc_xfer(ep, (uint8_t *)_hiddf_generic_funcd.report_desc, _hiddf_generic_funcd.report_desc_len, false);
	default:
		return ERR_INVALID_ARG;
	}
}

/**
 * \brief Process the HID class request
 * \param[in] ep Endpoint address.
 * \param[in] req Pointer to the request.
 * \return Operation status.
 */
static int32_t hid_generic_req(uint8_t ep, struct usb_req *req, enum usb_ctrl_stage stage)
{
	uint8_t *ctrl_buf = usbdc_get_ctrl_buffer();
	uint16_t len      = req->wLength;
	uint8_t descriptor_type = req->wValueBytes.h;

	if (req->bmRequestType == (HOST_GET | CLASS_COMMUNICATION | INTERFACE_RECIPIENT) && req->bRequest == HID_GET_REPORT && descriptor_type == HID_GET_REPORT_INPUT)
	{
		if (stage == USB_SETUP_STAGE)
		{
			hiddf_generic_set_ctrl_report(ctrl_buf, len);	// invoke callback from setup stage.
			return usbdc_xfer(ep, ctrl_buf, len, false);	//send control transfer buffer
		}
		if (stage == USB_DATA_STAGE && hiddf_generic_set_ctrl_report)
		{
			// buffer data sent from here (data stage) is not sent to PC for whatever reason but instead gets sent from setup stage.
			//hiddf_generic_set_ctrl_report(ctrl_buf, len);	// invoke callback
			//return usbdc_xfer(ep, ctrl_buf, len, false);	//send control transfer buffer
		}
	}
	else if (req->bmRequestType == (HOST_SET | CLASS_COMMUNICATION | INTERFACE_RECIPIENT) && req->bRequest == HID_SET_REPORT && descriptor_type == HID_SET_REPORT_OUTPUT)
	{
		if (stage == USB_SETUP_STAGE)
		{
			return usbdc_xfer(ep, ctrl_buf, len, false);
		}
		if (stage == USB_DATA_STAGE && hiddf_generic_get_ctrl_report)
		{
			hiddf_generic_get_ctrl_report(ctrl_buf, len);	// invoke callback
		}
		return ERR_NONE;
	}

	if ((0x81 == req->bmRequestType) && (0x06 == req->bRequest) && (req->wIndex == _hiddf_generic_funcd.func_iface)) {
		return hid_generic_get_desc(ep, req);
	} else {
		if (0x01 != ((req->bmRequestType >> 5) & 0x03)) { // class request
			return ERR_NOT_FOUND;
		}
		if (req->wIndex == _hiddf_generic_funcd.func_iface) {
			if (req->bmRequestType & USB_EP_DIR_IN) {
				return ERR_INVALID_ARG;
			} else {
				switch (req->bRequest) {
				case 0x03: /* Get Protocol */
					return usbdc_xfer(ep, &_hiddf_generic_funcd.protocol, 1, 0);
				case 0x0B: /* Set Protocol */
					_hiddf_generic_funcd.protocol = req->wValue;
					return usbdc_xfer(ep, NULL, 0, 0);
				case USB_REQ_HID_SET_REPORT:
					if (USB_SETUP_STAGE == stage) {
						return usbdc_xfer(ep, ctrl_buf, len, false);
					} else {
						if (NULL != hiddf_generic_set_report) {
							hiddf_generic_set_report(ctrl_buf, len);
						}
						return ERR_NONE;
					}
				default:
					return ERR_INVALID_ARG;
				}
			}
		} else {
			return ERR_NOT_FOUND;
		}
	}
}

/** USB Device HID Generic Handler Struct */
static struct usbdc_handler hid_generic_req_h = {NULL, (FUNC_PTR)hid_generic_req};

/**
 * \brief Initialize the USB HID Generic Function Driver
 */
int32_t hiddf_generic_init(const uint8_t *report_desc, uint32_t len)
{
	if (NULL == report_desc || 0 == len) {
		return ERR_INVALID_ARG;
	}

	if (usbdc_get_state() > USBD_S_POWER) {
		return ERR_DENIED;
	}

	_hiddf_generic_funcd.report_desc     = report_desc;
	_hiddf_generic_funcd.report_desc_len = len;
	_hiddf_generic.ctrl                  = hid_generic_ctrl;
	_hiddf_generic.func_data             = &_hiddf_generic_funcd;

	usbdc_register_function(&_hiddf_generic);
	usbdc_register_handler(USBDC_HDL_REQ, &hid_generic_req_h);

	return ERR_NONE;
}

/**
 * \brief Deinitialize the USB HID Generic Function Driver
 */
int32_t hiddf_generic_deinit(void)
{
	if (usbdc_get_state() > USBD_S_POWER) {
		return ERR_DENIED;
	}

	_hiddf_generic.ctrl      = NULL;
	_hiddf_generic.func_data = NULL;

	usbdc_unregister_function(&_hiddf_generic);
	usbdc_unregister_handler(USBDC_HDL_REQ, &hid_generic_req_h);
	return ERR_NONE;
}

/**
 * \brief Check whether HID Generic Function is enabled
 */
bool hiddf_generic_is_enabled(void)
{
	return _hiddf_generic_funcd.enabled;
}

/**
 * \brief USB HID Generic Function Read Data
 */
int32_t hiddf_generic_read(uint8_t *buf, uint32_t size)
{
	if (!hiddf_generic_is_enabled()) {
		return ERR_DENIED;
	}
	return usbdc_xfer(_hiddf_generic_funcd.func_ep_out, buf, size, false);
}

/**
 * \brief USB HID Generic Function Write Data
 */
int32_t hiddf_generic_write(uint8_t *buf, uint32_t size)
{
	if (!hiddf_generic_is_enabled()) {
		return ERR_DENIED;
	}
	return usbdc_xfer(_hiddf_generic_funcd.func_ep_in, buf, size, false);
}

/**
 * \brief USB HID Generic Function Register Callback
 */
int32_t hiddf_generic_register_callback(enum hiddf_generic_cb_type cb_type, FUNC_PTR func)
{
	if (!hiddf_generic_is_enabled()) {
		return ERR_DENIED;
	}
	switch (cb_type) {
	case HIDDF_GENERIC_CB_READ:
		usb_d_ep_register_callback(_hiddf_generic_funcd.func_ep_out, USB_D_EP_CB_XFER, func);
		break;
	case HIDDF_GENERIC_CB_WRITE:
		usb_d_ep_register_callback(_hiddf_generic_funcd.func_ep_in, USB_D_EP_CB_XFER, func);
		break;
	case HIDDF_GENERIC_CB_SET_REPORT:
		hiddf_generic_set_report = (hiddf_generic_set_report_t)func;
		break;
	case HIDDF_GENERIC_CB_GET_CTRL_REPORT:
		hiddf_generic_get_ctrl_report = (hiddf_generic_get_ctrl_report_t)func;
		break;
	case HIDDF_GENERIC_CB_SET_CTRL_REPORT:
		hiddf_generic_set_ctrl_report = (hiddf_generic_set_ctrl_report_t)func;
		break;
	default:
		return ERR_INVALID_ARG;
	}

	return ERR_NONE;
}

/**
 * \brief Return version
 */
uint32_t hiddf_generic_get_version(void)
{
	return HIDDF_GENERIC_VERSION;
}
