const char * version = "2020-02-27";

//----------------------------------------------------------------------------------------
//
//	STOC DMX BOX Firmware
//						
//		Target MCU: Teensy 4.0
//		Copyright:	2020 Michael Egger, me@anyma.ch
//		License: 	This is FREE software (as in free speech, not necessarily free beer)
//					published under gnu GPL v.3
//
//		DMX Library: https://github.com/ssilverman/TeensyDMX
//
//----------------------------------------------------------------------------------------

#include <TeensyDMX.h>


//========================================================================================
//----------------------------------------------------------------------------------------
//																				DEFINES

namespace teensydmx = ::qindesign::teensydmx;


// .............................................................................Pins 

const int 	PIN_PIXELS = 20;
const int 	DMX_BUF_SIZE	= 512;

//========================================================================================
//----------------------------------------------------------------------------------------
//																				GLOBALS
teensydmx::Sender universe_1{Serial1};
teensydmx::Sender universe_2{Serial2};
teensydmx::Sender universe_3{Serial3};
teensydmx::Sender universe_4{Serial4};
teensydmx::Receiver dmxRx{Serial5};


uint8_t		rx_buffer[DMX_BUF_SIZE];


//========================================================================================
//----------------------------------------------------------------------------------------
//																				IMPLEMENTATION



//========================================================================================
//----------------------------------------------------------------------------------------
//																				SETUP

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 4000) {
    // Wait for initialization to complete or a time limit
  }
  Serial.println("STOCBOX Setting up");

  dmxRx.begin();

  universe_1.begin();
  universe_2.begin();
  universe_3.begin();
  universe_4.begin();

}


//========================================================================================
//----------------------------------------------------------------------------------------
//																				loop

void loop() {
	for (int i = 0; i < DMX_BUF_SIZE; i++) {
  		uint8_t v = dmxRx.get(i);
  		if (v != rx_buffer[i]) {
  			universe_4.set(i, v);
  			rx_buffer[i] = v;
  		}
	}
	
	if (usbMIDI.read()) {
		// get the USB MIDI message, defined by these 5 numbers (except SysEX)
		uint8_t type = usbMIDI.getType();
		uint8_t channel = usbMIDI.getChannel();
		uint8_t data1 = usbMIDI.getData1();
		uint8_t data2 = usbMIDI.getData2();
		uint8_t cable = usbMIDI.getCable();

		switch (type) {
			case usbMIDI.ControlChange: // 0xB0
				switch(cable) {
					case 0:
						break;
				}
   				break;
   			 
			case usbMIDI.SystemExclusive:
    			 // SysEx messages are special.  The message length is given in data1 & data2
    			//  unsigned int SysExLength = data1 + data2 * 256;
    			//usbMIDI.getSysExArray()
	   			break;
   			 
			default:
   			 	break;
		
		}
	}
}