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
#include <Timer.h>
#include <FastLED.h>

//========================================================================================
//----------------------------------------------------------------------------------------
//																				DEFINES

namespace teensydmx = ::qindesign::teensydmx;

const int CABLE_MODE_DMX	= 0;
const int CABLE_MODE_CHANGLIER = 1;

// .............................................................................Pins 

const int 	PIN_PIXELS 			= 20;
const int 	DMX_BUF_SIZE		= 512;
const char	NUM_PIXELS			= 6;
const char	NUM_UNIVERSES		= 4;
const int 	DMX_TIMEOUT 		= 300;


//========================================================================================
//----------------------------------------------------------------------------------------
//																				GLOBALS
teensydmx::Sender universe_1{Serial1};
teensydmx::Sender universe_2{Serial2};
teensydmx::Sender universe_3{Serial3};
teensydmx::Sender universe_0{Serial4};
teensydmx::Receiver dmxRx{Serial5};

Timer	t;

uint8_t		dmx_rx_buffer[DMX_BUF_SIZE];
uint8_t		midi_rx_buffer[NUM_UNIVERSES][16][128];
uint8_t 	cable_mode[NUM_UNIVERSES];
int			activity[NUM_PIXELS];

CRGB                                    pixels[NUM_PIXELS];
CHSV									colors[NUM_PIXELS];

//========================================================================================
//----------------------------------------------------------------------------------------
//																				IMPLEMENTATION


//----------------------------------------------------------------------------------------
//																	DEFAULT MIDI To DMX

// 	map incoming MIDI (0-127) to DMX (0-255)
//	midi channel 1 controls slots 1-128, channel 2 129-256 and so on


void dmx_set_default(uint8_t cable, uint8_t channel,	uint8_t controller,uint8_t value){
	uint16_t slot = channel * 128 + controller;
	value = value * 2;
	
	if (slot < 0) slot = 0;
	if (slot > 511) slot = 511;
	if (value < 0) value = 0;
	if (value > 253) value = 255; 	// -- 2 * 127 = 254, skip 254 to 255
	
	switch(cable) {
		case 0:
  			universe_0.set(slot, value);
  			break;
		case 1:
  			universe_1.set(slot, value);
  			break;
		case 2:
  			universe_2.set(slot, value);
  			break;
		case 3:
  			universe_3.set(slot, value);
  			break;
		default:
			break;
	}
}

//----------------------------------------------------------------------------------------
//																	CHANGLIER MIDI To DMX

// 	map incoming MIDI (0-127) to Changlier Style DMX (1-128)
//	midi channel 1 controls slots 1-20, channel 2 21-40 and so on


void dmx_set_changlier(uint8_t cable, uint8_t channel,	uint8_t controller,uint8_t value){
	uint16_t slot = channel * 20 + controller;
	value = value + 1;
	
	if (slot < 0) slot = 0;
	if (slot > 511) slot = 511;
	if (value < 0) value = 0;

	
	switch(cable) {
		case 0:
  			universe_0.set(slot, value);
  			break;
		case 1:
  			universe_1.set(slot, value);
  			break;
		case 2:
  			universe_2.set(slot, value);
  			break;
		case 3:
  			universe_3.set(slot, value);
  			break;
		default:
			break;
	}
}


//----------------------------------------------------------------------------------------
//																	LEDS

void update_leds() {
	for (int i = 0; i < NUM_PIXELS; i++) {
		if (i < NUM_UNIVERSES) {	colors[i].hue = 42;  }// yellow
		else {	colors[i].hue = 120; }
		colors[i].saturation = 220;
		if (activity[i] > 0) activity[i] += 30;		// basic brightness for low activity
		if (activity[i] > 255) activity[i] = 255;
		colors[i].value =  activity[i];
		activity[i] = 0;
		pixels[i] = colors[i];
	}
	
	FastLED.show();
}


//========================================================================================
//----------------------------------------------------------------------------------------
//																				SETUP

void setup() {
	Serial.begin(115200);
	while (!Serial && millis() < 4000) {}
	Serial.println("STOCBOX Setting up");

	FastLED.addLeds<NEOPIXEL, PIN_PIXELS>(pixels, NUM_PIXELS);

	dmxRx.begin();

	universe_0.begin();
	universe_1.begin();
	universe_2.begin();
	universe_3.begin();
	
	cable_mode[0] = CABLE_MODE_CHANGLIER;
	cable_mode[1] = CABLE_MODE_CHANGLIER;
	cable_mode[2] = CABLE_MODE_DMX;
	cable_mode[3] = CABLE_MODE_DMX;

	t.every(100,update_leds);
}


//========================================================================================
//----------------------------------------------------------------------------------------
//																				loop

void loop() {

	if (millis() - dmxRx.lastPacketTimestamp() > DMX_TIMEOUT) {
		activity[4] = 0;
	} else {
		if (activity[4] < 40) activity[4] = 40;			// minimum brightness to show dmx is present
		
		for (int i = 0; i < DMX_BUF_SIZE; i++) {
			uint8_t v = dmxRx.get(i);
			if (v != dmx_rx_buffer[i]) {				// output DMX only if received values Change
				universe_3.set(i, v);
				dmx_rx_buffer[i] = v;
				activity[4]++;
				activity[3]++;
			}
		}
	}
	
	
	if (usbMIDI.read()) {
		// get the USB MIDI message, defined by these 5 numbers (except SysEX)
		uint8_t type = usbMIDI.getType();
		uint8_t channel = usbMIDI.getChannel();
		uint8_t data1 = usbMIDI.getData1();
		uint8_t data2 = usbMIDI.getData2();
		uint8_t cable = usbMIDI.getCable();
		
		if (cable < NUM_UNIVERSES) {
			switch (type) {
				case usbMIDI.ControlChange: // 0xB0
					if (midi_rx_buffer[cable][channel][data1] == data2) break;
					
					midi_rx_buffer[cable][channel][data1] = data2;
					activity[cable]++;
				
					switch(cable_mode[cable]) {
						case CABLE_MODE_DMX:
							dmx_set_default(cable,channel,data1,data2);
							break;
						case CABLE_MODE_CHANGLIER:
							dmx_set_changlier(cable,channel,data1,data2);
							break;
						default:
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
}