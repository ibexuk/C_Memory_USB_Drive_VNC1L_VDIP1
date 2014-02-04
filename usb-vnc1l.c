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



#include "main.h"
#define USB_HOST_C
#include "usb-vnc1l.h"

#include "ap-main.h"
#include "ap-gen.h"






//************************************
//************************************
//********** SETUP SPI PORT **********
//************************************
//************************************
//Called before any access in case pins are shared with other real SPI bus devices whyich have a different config
void setup_vnc1l_spi_port (void)
{
	//Disable SPI port as we bit bash due to stupid VNC1L SPI 13 bit words
	SpiChnClose(2);				//Disable SPI port

	USB_HOST_MISO_TO_INPUT;
	USB_HOST_MOSI_TO_OUTPUT;
	USB_HOST_CLK_TO_OUTPUT;

	USB_HOST_MOSI(0);
	USB_HOST_CLK(1);
}




//**************************************
//**************************************
//********** PROCESS USB HOST **********
//**************************************
//**************************************
void process_usb_host (void)
{
	static BYTE attempts_count;
	//SIGNED_WORD rx_data;
	//BYTE *p_tx_buffer;
	static BYTE usb_host_state_last = 0xff;
	BYTE just_entered_this_state;
	WORD index;


	just_entered_this_state = 0;
	if (usb_host_state_last != usb_host_state)
	{
		usb_host_state_last = usb_host_state;
		just_entered_this_state = 1;
	}




	switch (usb_host_state)
	{
	case UH_INITIALISE:
		//------------------------------------------------
		//------------------------------------------------
		//----- INITIALISE - WAIT FOR POWERUP STRING -----
		//------------------------------------------------
		//------------------------------------------------
		if (just_entered_this_state)
		{
			usb_host_clear_rx_buffer();
			attempts_count = 20;			//x200mS = 4 secs

			usb_host_1ms_timer = 0;


			//CLEAR THE DEBUG BUFFER
			#ifdef USB_HOST_DEBUG_BUFFER_LENGTH
				for (index = 0; index < USB_HOST_DEBUG_BUFFER_LENGTH; index++)
					usb_host_debug_buffer[index] = 0x0d;
				usb_host_rx_most_recent[USB_HOST_MOST_RECENT_BUFFER_LENGTH] = 0x00;		//Last byte is always null to allow string searches
			#endif
		}

		//After reset the monitor defaults to Extended Command Set.
		//After reset the VNC1L defaults to Binary Mode.
		//We use ASCII mode as its easier for debugging

		//Upon starting the monitor the following string is displayed:
		//	[0x0d]
		//	Ver·xx.xxVcccF·On-Line:[0x0d]
		//This confirms power up but if a drive is present there will be a delay followed by a status mesasge of "D:\>" to tell you a drive insertion has been detected.
		//If you start sening commands while this is pending you can get comms completely screwed up.  We 've had the write byte function stop working compeltly saying the
		//tx buffer is full continuously until a read is tried, at which point the commands are out of sequence and you don't knwo where you are.  The VNC1L is horrible to deal with 
		//has a poor protocol and is badly documented so best to always err on the safe side!
		//The best solution we found to this is to simply wait here to see if this disk present arrives. If it does we move striaght on.  If it doesn't we wait a few seconds just to
		//be sure.  If you've restarted during development but the VNC1L hasn't because you don't reset it via a reset line then you won't get this startup sequence and will instead
		//just have this few second delay before the drive is ready.

		//Don't bother doing comms etc every time
		if (usb_host_1ms_timer)
			break;
		usb_host_1ms_timer = 200;

		if (usb_stick_check_for_events())
		{
			if (find_const_string_in_string_no_case (&usb_host_rx_most_recent[0], (CONSTANT BYTE*)"D:\\>\r"))
			{
				usb_host_state = UH_SET_MODE;
				break;
			}
		}

		attempts_count--;
		if (attempts_count == 0)
			usb_host_state = UH_SET_MODE;			//Assume module has started and we just missed it (maybe we've restarted?)

		break;




	case UH_SET_MODE:
		//-----------------------------
		//-----------------------------
		//----- SET MODE TO ASCII -----
		//-----------------------------
		//-----------------------------

		//Send command
		usb_stick_do_command((BYTE*)"IPA\r", 1);

		//usb_stick_do_command((BYTE*)"\r", 1);			//This seems to be necessary to cause the IPA command to actually complete - you don't get the D:\> without sending something and subsequent commands are then broken

		usb_host_1ms_timer = 3000;			//In case we dont' get the response we expect
		while (!find_const_string_in_string_no_case (&usb_host_rx_most_recent[0], (CONSTANT BYTE*)"D:\\>\r"))
		{
			usb_stick_check_for_events();
			if (usb_host_1ms_timer == 0)
				break;
		}

		usb_host_state = UH_NO_DRIVE_PRESENT;

		break;


		

	case UH_NO_DRIVE_PRESENT:
		//-----------------------------------------
		//-----------------------------------------
		//----- WAIT FOR DRIVE TO BE PRESENT -----
		//-----------------------------------------
		//-----------------------------------------
		if (just_entered_this_state)
		{
			usb_host_clear_rx_buffer();

			//Do an initial get status to trigger drive present message if a drive is already inserted
			usb_stick_do_command((BYTE*)"\r", 0);
		}
		usb_drive_is_present = 0;

		//We don't need to continually poll for a drive inserted message.  If a drive is inserted we receive a string ending in "D:\\>\r"
		if (usb_stick_check_for_events())
		{
			if (find_const_string_in_string_no_case (&usb_host_rx_most_recent[0], (CONSTANT BYTE*)"D:\\>\r"))
			{
				usb_drive_is_present = 1;
				usb_host_file_open = 0;
				usb_host_state = UH_READY;
			}
		}
		break;




	case UH_READY:
		//-----------------
		//-----------------
		//----- READY -----
		//-----------------
		//-----------------
		if (just_entered_this_state)
		{
			usb_host_clear_rx_buffer();
		}

		//----- CHECK FOR DISK REMOVED -----
		//We don't need to poll for a drive removed message.  If the drive is removed we receive a string ending in "\rNo Disk\r"
		if (usb_stick_check_for_events())
		{
			if (find_const_string_in_string_no_case (&usb_host_rx_most_recent[0], (CONSTANT BYTE*)"\rNo Disk\r"))
				usb_host_state = UH_NO_DRIVE_PRESENT;
		}
		break;

	}

}







//*******************************
//*******************************
//********** OPEN FILE **********
//*******************************
//*******************************
//Returns 1 if sucessful, 0 if not
BYTE usb_host_open_file (CONSTANT BYTE *p_filename)
{
	BYTE *p_tx_buffer;
	BYTE data;
	
	//----- EXIT IF STILL INITIALISING -----
	if (usb_host_state != UH_READY)
		return(0);

	//----- CLOSE FILE IF ONE IS ALREADY OPEN -----
	if (usb_host_file_open)
	{
		usb_host_close_file(usb_host_file_open);
	}
	usb_host_file_open = p_filename;

	usb_stick_check_for_events();

	//----- SEND READ FILE COMMAND -----
	p_tx_buffer = &usb_host_tx_rx_buffer[0];
	*p_tx_buffer++ = 'O';
	*p_tx_buffer++ = 'P';
	*p_tx_buffer++ = 'R';
	*p_tx_buffer++ = ' ';

	data = *p_filename++;
	while ((data != 0x00) && (p_tx_buffer < (&usb_host_tx_rx_buffer[0] + USB_HOST_BUFFER_LENGTH)))
	{
		*p_tx_buffer++ = data;
		data = *p_filename++;
	}
	*p_tx_buffer++ = 0x0d;

	if (usb_stick_do_command (&usb_host_tx_rx_buffer[0], 1))
	{
		//Responses:
		//No Disk 0x0d			No USB Stick
		//D:\> 0x0d				File found
		//Command Failed		File not found
		if (find_const_string_in_string_no_case (&usb_host_rx_most_recent[0], (CONSTANT BYTE*)"D:\\>\r"))		//Should be received to confirm command
		{
			//----- FILE FOUND -----
			return(1);
		}
	}

	usb_stick_check_for_events();		//Get any additional data

	usb_host_file_open = 0;
	return(0);
}





//************************************
//************************************
//********** READ FROM FILE **********
//************************************
//************************************
//Returns 1 if sucessful, 0 if not
//Reads the specified number of bytes from the currently open file.
//If the number of bytes to read exceeds the number of bytes in the file then the entire remaining contents of the file will be transferred, followed by
//padding data (0xFE bytes) to make up the total number of bytes requested in the command parameter.
//Each call reads from the byte following the last byte of the previous call
BYTE usb_host_read_file_bytes (BYTE *p_buffer, WORD length)
{
	SIGNED_WORD rx_data;
	BYTE *p_tx_buffer;
	
	//----- EXIT IF STILL INITIALISING -----
	if (usb_host_state != UH_READY)
		return(0);


	
	//----- SEND READ FILE COMMAND -----
	//When the command has been issued the number of bytes requested in the parameter will be transferred
	p_tx_buffer = &usb_host_tx_rx_buffer[0];
	*p_tx_buffer++ = 'R';
	*p_tx_buffer++ = 'D';
	*p_tx_buffer++ = 'F';
	*p_tx_buffer++ = ' ';
	p_tx_buffer = convert_word_to_ascii((WORD)length, p_tx_buffer);
	*p_tx_buffer++ = 0x0d;

	if (usb_stick_do_command (&usb_host_tx_rx_buffer[0], 0))
	{
		//----- GET THE FILE DATA BYTES -----
		while (length)
		{
			//Check for timeout
			if (usb_host_rx_timeout_1ms_timer == 0)
				return(0);

			rx_data = vnc1l_transfer_byte(VNC1L_DATA_READ, 0, 0);

			if (rx_data != -1)
			{
				length--;
				*p_buffer++ = (BYTE)rx_data;
			}
		}
	}

	//Ensure there is no more data waiting to be sent (you will get "command failed" if you read past the end of the file)
	usb_stick_check_for_events();

	return(1);
}


//********************************
//********************************
//********** CLOSE FILE **********
//********************************
//********************************
void usb_host_close_file (CONSTANT BYTE *p_filename)
{
	BYTE *p_tx_buffer;
	BYTE data;

	//----- EXIT IF STILL INITIALISING -----
	if (usb_host_state != UH_READY)
		return;

	//You don't actually have to close a file opened for reading but its recommended

	//----- SEND CLOSE FILE COMMAND -----
	p_tx_buffer = &usb_host_tx_rx_buffer[0];
	*p_tx_buffer++ = 'C';
	*p_tx_buffer++ = 'L';
	*p_tx_buffer++ = 'F';
	*p_tx_buffer++ = ' ';
	
	data = *p_filename++;
	while ((data != 0x00) && (p_tx_buffer < (&usb_host_tx_rx_buffer[0] + USB_HOST_BUFFER_LENGTH)))
	{
		*p_tx_buffer++ = data;
		data = *p_filename++;
	}
	*p_tx_buffer++ = 0x0d;

	//Send it
	if (usb_stick_do_command (&usb_host_tx_rx_buffer[0], 1))
	{
		//We don't care about the response
		//if (find_const_string_in_string_no_case (&usb_host_rx_most_recent[0], (CONSTANT BYTE*)"D:\\>\r"))
		//	return;
	}

	usb_host_file_open = 0;
}



//********************************************************
//********************************************************
//********** PROCESS USB STICK CHECK CLOSE FILE **********
//********************************************************
//********************************************************
void process_usb_stick_check_close_file (void)
{

	if (usb_stick_file_is_open)
	{
		usb_stick_file_is_open = 0;
		usb_host_close_file(&usb_stick_playback_file_open[0]);
	}
}





//**************************************
//**************************************
//********** CHECK FOR EVENTS **********
//**************************************
//**************************************
//Returns: 1 if data was received
BYTE usb_stick_check_for_events (void)
{
	SIGNED_WORD rx_data;
	BYTE return_value = 0;

	setup_vnc1l_spi_port();

	//We could read the status byte but it isn't clear exactly how it works and seeing as we are checkign for RX we may as well just RX and see if data is set!

	//Read all bytes until end of rx data avaialble
	rx_data = 1;
	while (rx_data != -1)
	{
		rx_data = vnc1l_transfer_byte(VNC1L_DATA_READ, 0, 1);
		if (rx_data != -1)
			return_value = 1;
	}
	return(return_value);
}



//**********************************
//**********************************
//********** SEND COMMAND **********
//**********************************
//**********************************
//Returns: 1 = success, 0 = fail (e.g. timeout)
//wait_for_response
//	0= no
//	1 = until no more data to receive
//	2 = Read up to "\d" received
BYTE usb_stick_do_command (BYTE *tx_data, BYTE wait_for_response)
{
	SIGNED_WORD rx_data;
	BYTE response_received = 0;
	WORD index;

	//Add a '#' to the debug buffer
	#ifdef USB_HOST_DEBUG_BUFFER_LENGTH
		for (index = 0; index < (USB_HOST_DEBUG_BUFFER_LENGTH - 1); index++)
			usb_host_debug_buffer[index] = usb_host_debug_buffer[(index + 1)];
		usb_host_debug_buffer[index] = '#';
		usb_host_debug_buffer[USB_HOST_DEBUG_BUFFER_LENGTH] = 0x00;		//Last byte is always null to allow string searches
	#endif


	usb_host_clear_rx_buffer();

	setup_vnc1l_spi_port();

	//----- SEND THE COMMAND -----
	while (*tx_data != 0x00)
	{
		vnc1l_transfer_byte(VNC1L_DATA_WRITE, *tx_data, 1);
		if (*tx_data == 0x0d)
			break;
		tx_data++;
	}

	if (!wait_for_response)
		return(1);

	//----- GET THE RESPONSE ------

	//Add a '*' to the debug buffer
	#ifdef USB_HOST_DEBUG_BUFFER_LENGTH
		for (index = 0; index < (USB_HOST_DEBUG_BUFFER_LENGTH - 1); index++)
			usb_host_debug_buffer[index] = usb_host_debug_buffer[(index + 1)];
		usb_host_debug_buffer[index] = '*';
		usb_host_debug_buffer[USB_HOST_DEBUG_BUFFER_LENGTH] = 0x00;		//Last byte is always null to allow string searches
	#endif

	//Read all bytes until end of rx data availble
	rx_data = 1;
	while ((rx_data != -1) || (!response_received))
	{
		//Check for timeout
		if (usb_host_rx_timeout_1ms_timer == 0)
		{
			//Add a '%' to the debug buffer
			#ifdef USB_HOST_DEBUG_BUFFER_LENGTH
				for (index = 0; index < (USB_HOST_DEBUG_BUFFER_LENGTH - 1); index++)
					usb_host_debug_buffer[index] = usb_host_debug_buffer[(index + 1)];
				usb_host_debug_buffer[index] = '%';
				usb_host_debug_buffer[USB_HOST_DEBUG_BUFFER_LENGTH] = 0x00;		//Last byte is always null to allow string searches
			#endif
			return(0);
		}

		rx_data = vnc1l_transfer_byte(VNC1L_DATA_READ, 0, 1);

		//CARRIAGE RETURN = END OF RESPONSE (we carry on until no data indication to ensure we've got everything being sent to us)
		if (rx_data == 0x0d)
		{
			response_received = 1;
			if (wait_for_response == 2)
				break;
		}
	}
	return(1);

}



//*************************************
//*************************************
//********** CLEAR RX BUFFER **********
//*************************************
//*************************************
void usb_host_clear_rx_buffer (void)
{
	WORD index;

	for (index = 0; index < USB_HOST_MOST_RECENT_BUFFER_LENGTH; index++)
		usb_host_rx_most_recent[index] = 0x0d;
	usb_host_rx_most_recent[USB_HOST_MOST_RECENT_BUFFER_LENGTH] = 0x00;		//Last byte is always null to allow string searches
}


//***********************************
//***********************************
//********** TRANSFER BYTE **********
//***********************************
//***********************************
//transfer_type		VNC1L_DATA_READ, VNC1L_STATUS_READ or VNC1L_DATA_WRITE
//copy_to_buffer	Can be set of functions which don't need the data copied to the buffer, for maximumspeed.
//Returns: rx_data (-1 for no new data)
WORD vnc1l_transfer_byte (BYTE transfer_type, BYTE tx_data, BYTE copy_to_buffer)
{
	SIGNED_WORD data = 0;
	BYTE bit_data;
	BYTE status = 1;
	WORD timeout_count = 100;
	WORD index;

	USB_HOST_CLK(0);
	USB_HOST_CLK_DELAY;


	while (1)
	{
		// CS goes high to enable SPI communications
		USB_HOST_CS(1);

		// Clock 1 - Start State
		USB_HOST_MOSI(1);

		USB_HOST_CLK_DELAY;
		USB_HOST_CLK(1);
		USB_HOST_CLK_DELAY;
		USB_HOST_CLK(0);

		// Clock 2 - Direction
		if (transfer_type & 0x02)
			USB_HOST_MOSI(1);
		else
			USB_HOST_MOSI(0);


		USB_HOST_CLK_DELAY;
		USB_HOST_CLK(1);
		USB_HOST_CLK_DELAY;
		USB_HOST_CLK(0);

		// Clock 3 - Address
		if (transfer_type & 0x01)
			USB_HOST_MOSI(1);
		else
			USB_HOST_MOSI(0);

		USB_HOST_CLK_DELAY;
		USB_HOST_CLK(1);
		USB_HOST_CLK_DELAY;
		USB_HOST_CLK(0);

		//Clocks 4..11 - Data Phase
		bit_data = 0x80;
		if (transfer_type & 0x02)
		{
			//----- READ OPERATION -----
			USB_HOST_MOSI(0);
			data = 0;

			while (bit_data)
			{
				USB_HOST_CLK_DELAY;
				if (USB_HOST_MISO)
					data |= bit_data;

				USB_HOST_CLK(1);
				USB_HOST_CLK_DELAY;
				USB_HOST_CLK(0);
				bit_data = bit_data >> 1;
			}
		}
		else
		{
			//----- WRITE OPERATION -----
			while (bit_data)
			{
				if (tx_data & bit_data)
					USB_HOST_MOSI(1);
				else
					USB_HOST_MOSI(0);
				USB_HOST_CLK_DELAY;
				USB_HOST_CLK(1);
				USB_HOST_CLK_DELAY;
				USB_HOST_CLK(0);
				bit_data = bit_data >> 1;	
			}
		}

		// Clock 12 - Status bit
		USB_HOST_CLK_DELAY;
		if (USB_HOST_MISO)		//WRITE: 0=data accepted, 1=internal buffer is full; READ 0=new data, 1=old data (no data available)
			status = 1;
		else
			status = 0;
		USB_HOST_CLK(1);
		USB_HOST_CLK_DELAY;
		USB_HOST_CLK(0);


		USB_HOST_CS(0);

		// Clock 13 - CS low
		USB_HOST_CLK_DELAY;
		USB_HOST_CLK(1);
		USB_HOST_CLK_DELAY;
		USB_HOST_CLK(0);

		if ((transfer_type & 0x03) == VNC1L_DATA_READ)
		{
			//-------------------------------
			//----- READ DATA OPERATION -----
			//-------------------------------
			if (status)
			{
				//----- NO DATA AVAILABLE -----
				data = -1;		//Flag that no new data was received
			}
			else
			{
				//----- RX IS NEW DATA BYTE -----
				//We keep a history of the most recent bytes received so functions can use it if they wish
				//Shift string left and add this byte to the end
				for (index = 0; index < (USB_HOST_MOST_RECENT_BUFFER_LENGTH - 1); index++)
					usb_host_rx_most_recent[index] = usb_host_rx_most_recent[(index + 1)];
				usb_host_rx_most_recent[index] = (BYTE)data;
				usb_host_rx_most_recent[USB_HOST_MOST_RECENT_BUFFER_LENGTH] = 0x00;		//Last byte is always null to allow string searches

				usb_host_rx_timeout_1ms_timer = USB_HOST_RX_TIMEOUT_1MS_TIME;			//Reset timeout timer

				//ADD BYTE TO DEBUG BUFFER
				#ifdef USB_HOST_DEBUG_BUFFER_LENGTH
					for (index = 0; index < (USB_HOST_DEBUG_BUFFER_LENGTH - 1); index++)
						usb_host_debug_buffer[index] = usb_host_debug_buffer[(index + 1)];
					usb_host_debug_buffer[index] = (BYTE)data;
					usb_host_debug_buffer[USB_HOST_DEBUG_BUFFER_LENGTH] = 0x00;		//Last byte is always null to allow string searches
				#endif
			}
		}
		if (transfer_type & 0x02)
		{
			//----- DATA OR STATUS READ - EXIT -----
			break;
		}

		//--------------------------------
		//----- WRITE DATA OPERATION -----
		//--------------------------------
		usb_host_rx_timeout_1ms_timer = USB_HOST_RX_TIMEOUT_1MS_TIME;			//Reset timeout timer for next rx
		
		if (status == 0)
		{
			//----- DATA WAS ACCAPTED -----
			//ADD BYTE TO DEBUG BUFFER
			#ifdef USB_HOST_DEBUG_BUFFER_LENGTH
			for (index = 0; index < (USB_HOST_DEBUG_BUFFER_LENGTH - 1); index++)
				usb_host_debug_buffer[index] = usb_host_debug_buffer[(index + 1)];
			usb_host_debug_buffer[index] = (BYTE)tx_data;
			usb_host_debug_buffer[USB_HOST_DEBUG_BUFFER_LENGTH] = 0x00;		//Last byte is always null to allow string searches
			#endif

			data = 1;				//Success
			break;
		}
		else
		{
			//----- DATA REJECTED - INTERNAL BUFFER IS FULL ------

			USB_HOST_CLK_DELAY;
			USB_HOST_CLK(1);
			USB_HOST_CLK_DELAY;
			USB_HOST_CLK(0);

			USB_HOST_CLK_DELAY;
			USB_HOST_CLK(1);
			USB_HOST_CLK_DELAY;
			USB_HOST_CLK(0);



			if ((timeout_count--) == 0)		//Just in case
			{
				//----- WRITE TIMEOUT -----

				#ifdef USB_HOST_DEBUG_BUFFER_LENGTH
				//ADD '$' TO DEBUG BUFFER
				for (index = 0; index < (USB_HOST_DEBUG_BUFFER_LENGTH - 1); index++)
					usb_host_debug_buffer[index] = usb_host_debug_buffer[(index + 1)];
				usb_host_debug_buffer[index] = '$';
				usb_host_debug_buffer[USB_HOST_DEBUG_BUFFER_LENGTH] = 0x00;		//Last byte is always null to allow string searches
				#endif

				data = -1;
				break;
			}
		}
		
	} //while (1)		//For tx byte retry until byte is accepted

	return(data);
}








//***************************************************************************************
//***************************************************************************************
//********** FIND FIRST OCCURANCE OF STRING WITHIN A STRING - CASE INSENSITIVE **********
//***************************************************************************************
//***************************************************************************************
//Looks for the first occurance of a constant string within a variable string
//Returns pointer to first character when found, or 0 if not found
//Compare is case insensitive
BYTE *find_const_string_in_string_no_case (BYTE *examine_string, CONSTANT BYTE *looking_for_string)
{
	BYTE number_of_characters_matched = 0;
	BYTE character1;
	BYTE character2;

	while (*examine_string != 0x00)
	{
		//----- CHECK NEXT CHARACTER -----
		character1 = *examine_string++;
		if ((character1 >= 'a') && (character1 <= 'z'))	//Convert to uppercase if lowercase
			character1 -= 0x20;

		character2 = *looking_for_string;
		if ((character2 >= 'a') && (character2 <= 'z'))	//Convert to uppercase if lowercase
			character2 -= 0x20;

		if (character1 == character2)
		{
			//----- THIS CHARACTER MATCHES -----
			looking_for_string++;
			number_of_characters_matched++;

			if (*looking_for_string == 0x00)
			{
				//----- GOT TO NULL OF STRING TO FIND - SUCCESS - STRING FOUND -----
				return(examine_string - number_of_characters_matched);
			}
		}
		else if (number_of_characters_matched)
		{
			looking_for_string -= number_of_characters_matched;		//Return string being looked for pointer back to start - the match was not completed
			number_of_characters_matched = 0;
		}
	}

	//----- NOT FOUND -----
	return(0);

}



//************************************************
//************************************************
//********** CONVERT WORD TO ASCII TEXT **********
//************************************************
//************************************************
//Returns pointer to the character that is currently the terminating null
BYTE* convert_word_to_ascii (WORD value, BYTE *dest_string)
{
	WORD value_working;
	BYTE string_length = 0;
	BYTE b_count;
	BYTE character;

	if (value == 0)
		*dest_string++ = '0';

	while (value)
	{
		//We have another character to add so shift string right ready for it
		for (b_count = 0; b_count < string_length; b_count++)
		{
			dest_string--;
			character = *dest_string++;
			*dest_string-- = character;
		}
		
		value_working = value / 10;
		*dest_string++ = (value - (value_working * 10)) + 0x30;
		dest_string += string_length;
		
		value = value_working;
		string_length++;
	}
	
	*dest_string = 0x00;		//Add the terminating null
	return(dest_string);
}






