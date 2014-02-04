/*
Provided by IBEX UK LTD http://www.ibexuk.com
Electronic Product Design Specialists
RELEASED SOFTWARE

The MIT License (MIT)

Copyright (c) 2013, IBEX UK Ltd, http://ibexuk.com

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
//Visit http://www.embedded-code.com/source-code/memory/usb-drives/ftdi-vdip1-vnc1l-usb-host for more information
//
//Project Name:	FTDI VDIP1 / VNC1L USB Host



//--------------------------------
//--------------------------------
//----- VDIP1 USING VNC1L IC -----
//--------------------------------
//--------------------------------
//VDIP1 mododule is supplied pre-loaded with Vincinculum VDAP firmware.
//The USB connector is connected to port 2 on the VNC1L.  You can connect a 2nd USB connetor external to port 1.
//VDAP firmware has the monitor port on the combined interface allowing BOMS devices to be connected to Port 2
//and USB Slave Peripherals to Port 1.
//For SPI Mode J3 = link pins 2&3, J4 = link pins 1&2

//The VDIP1 module is supplied pre-loaded with the VDAP firmware.


//The FTDI documentation is pretty awful in places, specifically with regards to the SPI interface and contradicts itself in different documents
//This SPI transfer code was based on their VMusic source code and works correctly.
//Docs:
//	DS_VNC1L-2.pdf						Correct SPI bus depiction
//	DS_VDIP1-1.pdf						Incorrect SPI bus depiction (MISO changes on falling edge not riging edge)
//	UM_VinculumFirmware_V205.pdf		The guide to the firmware, all commands etc
//The modules green PCB LED2 lights when a valid drive is inserted.


/*
##### SPI SETUP #####
Max 12MHz, SDI, SDO and CS clocked on rising edge of CLK
We don't use SPI port as the VNC1L uses a stupid 13 bit WORD per byte transfered.
NOTE THAT THE CS PIN IS THE WRONG WAY ROUND - HIGH=CS ACTIVE


##### ADD TO MAIN LOOP #####
	//----- PROCESS USB HOST -----
	process_usb_host();

##### ADD TO 1mS HEARTBEAT #####
	if (usb_host_1ms_timer)			//(16 bit so protect irq based changes on 8bit platforms)
		usb_host_1ms_timer--;
	if (usb_host_rx_timeout_1ms_timer)			//(16 bit so protect irq based changes on 8bit platforms)
		usb_host_rx_timeout_1ms_timer--;

 ##### USING #####
	if (usb_drive_is_present)


*/



//*****************************
//*****************************
//********** DEFINES **********
//*****************************
//*****************************
#ifndef USB_HOST_C_INIT		//Do only once the first time this file is used
#define	USB_HOST_C_INIT


//----- IO PINS -----
//(Standard SPI port can't be used due to stupid bit length used)

//PIC32:
#define USB_HOST_CS(state)					(state ? mPORTFSetBits(BIT_1) : mPORTFClearBits(BIT_1))		//NOTE THAT THE CS PIN IS THE WRONG WAY ROUND - HIGH=CS ACTIVE

#define USB_HOST_MISO_TO_INPUT				TRISGSET = (unsigned int)(BIT_7)
#define USB_HOST_MISO						(mPORTGReadBits(BIT_7))

#define USB_HOST_MOSI_TO_OUTPUT				TRISGCLR = (unsigned int)(BIT_8)
#define USB_HOST_MOSI(state)				(state ? mPORTGSetBits(BIT_8) : mPORTGClearBits(BIT_8))

#define USB_HOST_CLK_TO_OUTPUT				TRISGCLR = (unsigned int)(BIT_6)
#define USB_HOST_CLK(state)					(state ? mPORTGSetBits(BIT_6) : mPORTGClearBits(BIT_6))

#define USB_HOST_CLK_DELAY					Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop()				//Max 12MHz, so delay for min 83nS



#define	USB_HOST_MOST_RECENT_BUFFER_LENGTH		31

#define	USB_HOST_BUFFER_LENGTH					32				//SET TX / RX BUFFER LENGTH

#define	USB_FILE_READ_BUFFER_LENGTH				30				//Must be <=USB_HOST_BUFFER_LENGTH and <= LED_CONTROLLER_USART_BUFFER_LENGTH
#define	USB_HOST_RX_TIMEOUT_1MS_TIME			1000				//Timeout if we don't get a response we expect

//DEBUG BUFFER
//Handy buffer to see all recent comms when debugging.  '#'=command tx, '*'=response, '$'=Write timeout
#ifdef __DEBUG
	//#define	USB_HOST_DEBUG_BUFFER_LENGTH			127				//<<<<Comment out to not use debug buffer (a buffer showing all TX and RX, disable to increase speed)
#endif

typedef enum _SM_USB_HOST
{
    UH_INITIALISE,
	UH_SET_MODE,
	UH_NO_DRIVE_PRESENT,
	UH_READY,
} SM_USB_HOST;





#define	VNC1L_DATA_READ			0x02
#define	VNC1L_STATUS_READ		0x03
#define	VNC1L_DATA_WRITE		0x00


#endif


//*******************************
//*******************************
//********** FUNCTIONS **********
//*******************************
//*******************************
#ifdef USB_HOST_C
//-----------------------------------
//----- INTERNAL ONLY FUNCTIONS -----
//-----------------------------------
BYTE usb_stick_check_for_events (void);
BYTE usb_stick_do_command (BYTE *tx_data, BYTE wait_for_response);
WORD vnc1l_transfer_byte (BYTE transfer_type, BYTE tx_data, BYTE copy_to_buffer);
void usb_host_clear_rx_buffer (void);
BYTE *find_const_string_in_string_no_case (BYTE *examine_string, CONSTANT BYTE *looking_for_string);
BYTE* convert_word_to_ascii (WORD value, BYTE *dest_string);


//-----------------------------------------
//----- INTERNAL & EXTERNAL FUNCTIONS -----
//-----------------------------------------
//(Also defined below as extern)
void process_usb_host (void);
BYTE usb_host_open_file (CONSTANT BYTE *p_filename);
BYTE usb_host_read_file_bytes (BYTE *p_buffer, WORD length);
void usb_host_close_file (CONSTANT BYTE *p_filename);
void process_usb_stick_check_close_file (void);


#else
//------------------------------
//----- EXTERNAL FUNCTIONS -----
//------------------------------
extern void process_usb_host (void);
extern BYTE usb_host_open_file (CONSTANT BYTE *p_filename);
extern BYTE usb_host_read_file_bytes (BYTE *p_buffer, WORD length);
extern void usb_host_close_file (CONSTANT BYTE *p_filename);
extern void process_usb_stick_check_close_file (void);


#endif




//****************************
//****************************
//********** MEMORY **********
//****************************
//****************************
#ifdef USB_HOST_C
//--------------------------------------------
//----- INTERNAL ONLY MEMORY DEFINITIONS -----
//--------------------------------------------
BYTE usb_host_state = UH_INITIALISE;
BYTE usb_host_rx_most_recent[(USB_HOST_MOST_RECENT_BUFFER_LENGTH + 1)];
CONSTANT BYTE *usb_host_file_open;
BYTE usb_stick_file_is_open = 0;
BYTE usb_stick_read_byte_index;
BYTE usb_stick_read_file_buffer[USB_FILE_READ_BUFFER_LENGTH];
CONSTANT BYTE *usb_stick_playback_file_open;

#ifdef USB_HOST_DEBUG_BUFFER_LENGTH
BYTE usb_host_debug_buffer[(USB_HOST_DEBUG_BUFFER_LENGTH + 1)];
#endif
//--------------------------------------------------
//----- INTERNAL & EXTERNAL MEMORY DEFINITIONS -----
//--------------------------------------------------
//(Also defined below as extern)
volatile WORD usb_host_1ms_timer = 0;
volatile WORD usb_host_rx_timeout_1ms_timer = 0;
BYTE usb_drive_is_present = 0;
BYTE usb_host_tx_rx_buffer[USB_HOST_BUFFER_LENGTH];
BYTE usb_host_rx_no_of_bytes_received;
WORD usb_stick_playback_this_cmd_number = 0;


#else
//---------------------------------------
//----- EXTERNAL MEMORY DEFINITIONS -----
//---------------------------------------
extern volatile WORD usb_host_1ms_timer;
extern volatile WORD usb_host_rx_timeout_1ms_timer;
extern BYTE usb_drive_is_present;
extern BYTE usb_host_tx_rx_buffer[USB_HOST_BUFFER_LENGTH];
extern BYTE usb_host_rx_no_of_bytes_received;
extern WORD usb_stick_playback_this_cmd_number;

#endif

