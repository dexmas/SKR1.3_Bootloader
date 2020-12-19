/*****************************************************************************
 *                                                                            *
 * DFU/SD/SDHC Bootloader for LPC17xx                                         *
 *                                                                            *
 * by Triffid Hunter                                                          *
 *                                                                            *
 *                                                                            *
 * This firmware is Copyright (C) 2009-2010 Michael Moon aka Triffid_Hunter   *
 *                                                                            *
 * This program is free software; you can redistribute it and/or modify       *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software                *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

#include "usbcore.h"

#include "usbhw.h"
#include "lpc17xx_usb.h"
#include "dfu.h"
#include "descriptor.h"

#include <stdio.h>

#ifndef NULL
#define NULL ((void *) 0)
#endif

CONTROL_TRANSFER control;

void DFU_EP0in(void);
void DFU_EP0out(void);

uint8_t control_buffer[64];

usbdesc_base *descriptors;

void usb_provideDescriptors(void *d)
{
	descriptors = (usbdesc_base *) d;
}

void requestGetStatus(void)
{
	control_buffer[0] = control_buffer[1] = 0;
	control.bufferlen = 2;
}

void requestGetDescriptor(void)
{
	uint8_t dType = control.setup.wValue >> 8;
	uint8_t dIndex = control.setup.wValue & 0xFF;

	usbdesc_base *d = descriptors;

	int i = 0;

	while (d->bLength > 0)
	{
		if (d->bDescType == dType)
		{
			switch (d->bDescType)
			{
				case DT_DEVICE:
					dIndex = i = 0;
					break;
				case DT_CONFIGURATION:
					dIndex = i = ((usbdesc_configuration *) d)->bConfigurationValue;
					break;
				case DT_INTERFACE:
					i = ((usbdesc_interface *) d)->bInterfaceNumber;
					break;
				case DT_ENDPOINT:
					i = ((usbdesc_endpoint *) d)->bEndpointAddress;
					break;
			}
			if (i == dIndex)
			{
				control.buffer = d;
				if (dType == DT_CONFIGURATION)
				{
					control.bufferlen = ((usbdesc_configuration *) d)->wTotalLength;
				}
				else
				{
					control.bufferlen = d->bLength;
				}
				if (control.bufferlen > control.setup.wLength)
					control.bufferlen = control.setup.wLength;
				return;
			}
			i++;
		}

		d = (usbdesc_base *) (((uint8_t *) d) + d->bLength);
	}
	control.bufferlen = 0;
	control.zlp = 1;
}

void requestSetConfiguration(void)
{
	SIE_ConfigureDevice(1);
}

void requestGetConfiguration(void)
{
	control_buffer[0] = 1;
	control.bufferlen = 1;
}

void EP0Complete(void)
{
	printf(" Complete\n");
	if ((control.setup.bmRequestType & 0x7C) == 0)
	{
	}
	else
	{
		DFU_transferComplete(&control);
	}
}

void EP0setup(void)
{
	int l;

	if ((l = usb_read_packet(EP0OUT, &control.setup, 8)) == 8)
	{
		control.complete = 0;
		control.buffer = control_buffer;
		control.bufferlen = control.setup.wLength;

		printf("S[0x%x 0x%x 0x%x 0x%x 0x%x]: ", control.setup.bmRequestType, control.setup.bRequest, control.setup.wValue, control.setup.wIndex, control.setup.wLength);

		if ((control.setup.bmRequestType & 0x7C) == 0)
		{
			switch(control.setup.bRequest)
			{
				case REQ_GET_STATUS:
 					requestGetStatus();
					break;
				case REQ_CLEAR_FEATURE:
					break;
				case REQ_SET_FEATURE:
					break;
				case REQ_SET_ADDRESS:
					SIE_SetAddress(control.setup.wValue);
					printf("USB: Got USB Address %d\n", control.setup.wValue);
					usb_write_packet(EP0IN, NULL, 0);
					break;
				case REQ_GET_DESCRIPTOR:
					requestGetDescriptor();
					break;
				case REQ_SET_DESCRIPTOR:
					break;
				case REQ_GET_CONFIGURATION:
					requestGetConfiguration();
					break;
				case REQ_SET_CONFIGURATION:
					requestSetConfiguration();
					break;
				default:
					usb_ep0_stall();
					break;
			}
		}
		else
		{
			DFU_controlTransfer(&control);
		}
	}
}

void EP0in(void)
{
	if (control.complete == 0)
	{
		if (control.setup.bmRequestType_Data_Transfer_Direction == DATA_DIRECTION_DEVICE_TO_HOST)
		{
			if (control.bufferlen)
			{
				int l = control.bufferlen;
				if (l > sizeof(control_buffer))
					l = sizeof(control_buffer);
				l = usb_write_packet(EP0IN, control.buffer, l);
				control.bufferlen -= l;
				control.buffer += l;
				printf(":w%d", l);
				if (control.bufferlen == 0)
				{
					if (l == 64)
						control.zlp = 1;
				}
			}
			else if (control.zlp)
			{
				usb_write_packet(EP0IN, NULL, 0);
				control.zlp = 0;
				printf(" sent ZLP,");
			}
		}
		else if (control.bufferlen == 0)
		{
			printf(" Sent ACK,");
			usb_write_packet(EP0IN, NULL, 0);
			control.complete = 1;
			EP0Complete();
		}
	}
}

void EP0out(void)
{
	if (control.complete == 0)
	{
		if (control.setup.bmRequestType_Data_Transfer_Direction == DATA_DIRECTION_HOST_TO_DEVICE)
		{
			if (control.bufferlen)
			{
				int l;
				l = usb_read_packet(EP0OUT, control.buffer, control.bufferlen);

				if (l <= control.bufferlen)
				{
					control.bufferlen -= l;
					control.buffer += l;
					printf(":r%d", l);
					return;
				}
			}
			else
			{
				int l = usb_read_packet(EP0OUT, NULL, 0);
				if (l == 0)
					return;
			}
		}
		else if (control.bufferlen == 0)
		{
			int l = usb_read_packet(EP0OUT, NULL, 0);
			if (l == 0)
			{
				printf(" Recv ACK,");
				control.complete = 1;
				EP0Complete();
				return;
			}
		}
		usb_ep0_stall();
		return;
	}
	usb_read_packet(EP0OUT, control_buffer, sizeof(control_buffer));
}
