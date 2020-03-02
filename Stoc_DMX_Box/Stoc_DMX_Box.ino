const char * version = "2020-02-29.0";

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
#include <EEPROM.h>

//========================================================================================
//----------------------------------------------------------------------------------------
//																				DEFINES

namespace teensydmx = ::qindesign::teensydmx;

const int CABLE_MODE_DMX	= 0;
const int CABLE_MODE_CHANGLIER = 1;

const int MERGE_MODE_NONE = 0;
const int MERGE_MODE_LTP = 1;
const int MERGE_MODE_HTP = 2;			// not implemented

#define SERIAL_DEBUG	false
#define TEST_METRICS	true

const int 	DMX_BUF_SIZE		= 512;
const char	NUM_PIXELS			= 6;
const char	NUM_UNIVERSES		= 4;
const int 	DMX_TIMEOUT 		= 1000;

const int PANIC_BUF_SIZE		= 512;
const int PANIC_BUF_ADDRESS = 64; 	// EEPROM Start address

// .............................................................................Pins 

const int 	PIN_PIXELS 						= 23;
const int 	PIN_PANIC[NUM_UNIVERSES]		= {2,3,4,5};
const int 	PIN_PANIC_SET[NUM_UNIVERSES]	= {9,10,11,12};

//========================================================================================
//----------------------------------------------------------------------------------------
//																				GLOBALS
teensydmx::Sender universe_0{Serial1};
teensydmx::Sender universe_1{Serial2};
teensydmx::Sender universe_2{Serial3};
teensydmx::Sender universe_3{Serial4};
teensydmx::Receiver dmxRx{Serial5};

Timer	t;


CRGB                                    pixels[NUM_PIXELS];
CHSV									colors[NUM_PIXELS];

uint8_t		dmx_rx_buffer[DMX_BUF_SIZE];
uint8_t		midi_rx_buffer[NUM_UNIVERSES][16][128];
uint8_t 	cable_mode[NUM_UNIVERSES];
uint8_t 	merge_mode[NUM_UNIVERSES];
int			activity[NUM_PIXELS];

uint8_t		out_buffer[NUM_UNIVERSES][PANIC_BUF_SIZE];
uint8_t		panic_buffer[NUM_UNIVERSES][PANIC_BUF_SIZE];



#if TEST_METRICS
	long avg_midi_time;
	long min_midi_time;
	long max_midi_time;
	long avg_midi_interval;
	long min_midi_interval;
	long max_midi_interval;
	long midi_messages_count_raw;
	long midi_messages_count_handled;
	
	long avg_dmx_time;	
	long min_dmx_time;	
	long max_dmx_time;	
#endif

//========================================================================================
//----------------------------------------------------------------------------------------
//																				IMPLEMENTATION


//----------------------------------------------------------------------------------------
//																	DEFAULT MIDI To DMX

// 	map incoming MIDI (0-127) to DMX (0-255)
//	midi channel 1 controls slots 1-128, channel 2 129-256 and so on


void dmx_set_default(uint8_t cable, uint8_t channel,	uint8_t controller,uint8_t value){
	if (channel < 1) channel = 1;
	uint16_t slot = (channel - 1)  * 127 + controller;
	value = value * 2;
	
	if (slot > 511) slot = 511;
	if (value < 0) value = 0;
	if (value > 253) value = 255; 	// -- 2 * 127 = 254, skip 254 to 255
	

	#if SERIAL_DEBUG
		Serial.print("DMX Universe ");
		Serial.print(cable);
		Serial.print(" slot ");
		Serial.print(slot);
		Serial.print(" val ");
		Serial.print(value);
		Serial.println("DMX Universe ");
	#endif
	
	switch(cable) {
		case 0:
  			universe_0.set(slot, value);
  			out_buffer[0][slot] = value;
  			break;
		case 1:
  			universe_1.set(slot, value);
  			out_buffer[1][slot] = value;
  			break;
		case 2:
  			universe_2.set(slot, value);
  			out_buffer[2][slot] = value;
  			break;
		case 3:
  			universe_3.set(slot, value);
  			out_buffer[3][slot] = value;
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
	if (channel < 1) channel = 1;
	uint16_t slot = (channel - 1) * 20 + controller;
	value = value + 1;
	
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
//																	Panic Buffer

void read_panic_buffer() {
	int address;
	
	for (int cable = 0; cable < NUM_UNIVERSES; cable++) {
		for (int i = 0; i < PANIC_BUF_SIZE; i++) {
			address = cable * PANIC_BUF_SIZE + i;
			address += PANIC_BUF_ADDRESS;
			if(address < EEPROM.length()) {
				panic_buffer[cable][i] = EEPROM.read(address);
			}
		}
	}
}


void write_panic_buffer(uint8_t cable) {
	int address;
	for (int i = 0; i < PANIC_BUF_SIZE; i++) {
		address = cable * PANIC_BUF_SIZE + i;
		address += PANIC_BUF_ADDRESS;
		if(address < EEPROM.length()) {
			switch (cable) {
				case 0:
					panic_buffer[cable][i] = out_buffer[cable][i];
					break;
			}
			EEPROM.write(address,panic_buffer[cable][i]);
		}
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

//----------------------------------------------------------------------------------------
//																	MIDI Control Change

void handle_control_change(byte channel, byte control, byte value) {
	uint8_t cable = usbMIDI.getCable();

	#if TEST_METRICS
		static long entry_time;
		midi_messages_count_raw++;
		
		if (entry_time > 0) {
			long interval = micros()-entry_time;
			if (min_midi_interval == 0) {
				min_midi_interval = interval;
			} else {
				if (interval < min_midi_interval) min_midi_interval = interval;
			}
			if (max_midi_interval == 0) {
				max_midi_interval = interval;
			} else {
				if (interval > max_midi_interval) max_midi_interval = interval;
			}
			if (avg_midi_interval == 0) {
				avg_midi_interval = interval;
			} else {
				avg_midi_interval = (15 * avg_midi_interval + interval) / 16;
			}
		}		
		entry_time = micros();
	#endif
	
	#if SERIAL_DEBUG
		Serial.print("Cable ");
		Serial.print(cable, DEC);
		Serial.print(" : Control Change, ch=");
		Serial.print(channel, DEC);
		Serial.print(", control=");
		Serial.print(control, DEC);
		Serial.print(", value=");
		Serial.println(value, DEC);
	#endif
		
	if (cable < NUM_UNIVERSES) {
	
		if (midi_rx_buffer[cable][channel][control] != value) {

			#if TEST_METRICS
				midi_messages_count_handled++;
			#endif	
			
			midi_rx_buffer[cable][channel][control] = value;
			activity[cable]++;

			switch(cable_mode[cable]) {
				case CABLE_MODE_DMX:
					dmx_set_default(cable,channel,control,value);
					break;
				case CABLE_MODE_CHANGLIER:
					dmx_set_changlier(cable,channel,control,value);
					break;
				default:
					break;
			}
			
			#if TEST_METRICS
				long dur = micros() - entry_time;
				if (min_midi_time == 0) {
					min_midi_time = dur;
				} else {
					if (dur < min_midi_time) min_midi_time = dur;
				}
				if (max_midi_time == 0) {
					max_midi_time = dur;
				} else {
					if (dur > max_midi_time) max_midi_time = dur;
				}
				if (avg_midi_time == 0) {
					avg_midi_time = dur;
				} else {
					avg_midi_time = (15 * avg_midi_time + dur) / 16;
				}
			#endif
		}
	}
}


//----------------------------------------------------------------------------------------
//																	DMX


void check_dmx() {
/*
	if (millis() - dmxRx.lastPacketTimestamp() > DMX_TIMEOUT) {
		activity[4] = 0;
	} else {
		if (activity[4] < 40) activity[4] = 10;			// minimum brightness to show dmx is present
		
		for (int i = 0; i < 127; i++) {
			uint8_t v = dmxRx.get(i);
			if (v != dmx_rx_buffer[i]) {				// output DMX only if received values Change
				if (merge_mode[0] == MERGE_MODE_LTP) {
					universe_0.set(i, v);
					activity[0]++;
				}
				dmx_rx_buffer[i] = v;
				activity[4]++;
			}
		}
	}
	*/
}

//----------------------------------------------------------------------------------------
//																				Metrics

#if TEST_METRICS
void log_metrics() {
	Serial.print(midi_messages_count_raw);
	Serial.print(" ");
	Serial.print(midi_messages_count_handled);
	Serial.print(" ");
	Serial.print(min_midi_interval);
	Serial.print(" ");
	Serial.print(avg_midi_interval);
	Serial.print(" ");
	Serial.print(max_midi_interval);
	Serial.print(" ");
	Serial.print(min_midi_time);
	Serial.print(" ");
	Serial.print(avg_midi_time);
	Serial.print(" ");
	Serial.print(max_midi_time);
	Serial.println();
}
#endif
//========================================================================================
//----------------------------------------------------------------------------------------
//																				SETUP

void setup() {
	#if SERIAL_DEBUG
		Serial.begin(115200);
		while (!Serial && millis() < 4000) {}
		Serial.println("STOCBOX Setting up");
	#endif
	
	
	FastLED.addLeds<1,SK6812, PIN_PIXELS>(pixels, NUM_PIXELS);
	
	for (int hue = 0; hue < 360; hue++) {
    	fill_rainbow( pixels, NUM_PIXELS, hue, 7);
	    delay(3);
    	FastLED.show(); 
  	}


  	
	#if SERIAL_DEBUG
		Serial.println("Reading panic buffer");
	#endif

	read_panic_buffer();
	
	dmxRx.begin();

	universe_0.begin();
	universe_1.begin();
	universe_2.begin();
	universe_3.begin();

	cable_mode[0] = CABLE_MODE_CHANGLIER;
	cable_mode[1] = CABLE_MODE_DMX;
	cable_mode[2] = CABLE_MODE_DMX;
	cable_mode[3] = CABLE_MODE_DMX;

	merge_mode[0] = MERGE_MODE_LTP;
	merge_mode[1] = MERGE_MODE_NONE;
	merge_mode[2] = MERGE_MODE_NONE;
	merge_mode[3] = MERGE_MODE_LTP;

	#if SERIAL_DEBUG
		Serial.println("Done.");
	#endif
	
  	usbMIDI.setHandleControlChange(handle_control_change);

	t.every(40,check_dmx);
	t.every(100,update_leds);
	
	#if TEST_METRICS
		t.every(500,log_metrics);
		Serial.println("--------------------");
		Serial.println("METRICS");
		Serial.println("MIDI: Total messages | Handled messages | Interval Min | Interval Avg | Interval Max | Handle Min | Handle Avg | Handle Max |");
	#endif
}


//========================================================================================
//----------------------------------------------------------------------------------------
//																				loop

void loop() {
	t.update();
	usbMIDI.read();
}