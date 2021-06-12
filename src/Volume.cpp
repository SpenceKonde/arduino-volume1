/*
  Volume.cpp - Library for tone() with 8-bit volume control.
  Created by Connor Nishijima, May 25th 2016.
  Updated June 14, 2016.
  Released under the GPLv3 license.
  
  2021: Spence Konde - this library was so much worse off for using timer0 instead of the perfectly capable timer2.
  No timing stuff whatsoever needed anymore since timer0 not touched. 
  
  Totally untested, needs someone to compile test it and fix typos and shit like that. 
*/

#include "Volume.h"
bool _toneEnable = false; // making it a bit in a GPIOR would save 4 bytes and 2 clocks, in the ISR, and 4 bytes and 1 clock when accessed if done bitwise
byte _toneVol = 0;
volatile byte _toneCalcVol = 0;
byte _toneAltPin = 255; //will be 0 or 1 once started.

unsigned int _freq = 0;

float _masterVol = 1.00;

bool _fadeOut = false; // making it a bit in a GPIOR would save 4 bytes and 2 clocks, in the ISR, and 4 bytes and 1 clock when accessed if done bitwise
float _fadeVol = 1.00;
float _fadeAmount = 0.01;
byte _fadeCounter = 0;

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  //byte _SPEAKER = 4; //PG5 OC0B
  #define _SPEAKER 9 //PH6 = OC2B
  #define _SPEAKER_ALT 10 //PB4 = OC2A
#eiif defined(__AVR_ATmega168__) || defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168PA__) || defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168PB__) || defined(__AVR_ATmega328PB__)
// Catch most of the mega x8-series
  //byte _SPEAKER = 5; //PD5 OC0B
  #define _SPEAKER 3 //PD3 OC2B
  #define _SPEAKER_ALT 11 // PB2 OC2A
#else 
  #if (__AVR_ARCH__ >= 100) 
    #error "Error - part not supported. This is a modern AVR; this is the kind of job the CCL is made for... use the Logic library... if you can waste a pin you can even use stock tone()!"
  #elif defined(TCCR2A)
    #error "Error - part not supported, but should work if pins are added to library... it is a classic AVR and has a timer2"
  #else 
    #error "Error - part not supported, and does not have a Timer2 either, not looking good for this part being able to do this." 
  #endif
#endif

#define _SPEAKER_PIN (_toneAltPin?_SPEAKER_ALT:_SPEAKER)

void Volume::alternatePin(bool enabled) {
  if (_toneAltPin != 255) { // if pin is set 255, begin has not been called, otherwise we need to set the pin output 
    pinMode((enabled ? _SPEAKER_ALT : _SPEAKER), OUTPUT);
  }
  _toneAltPin = enabled; 

	  /*
  #if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    SPEAKER = 13; //PB7 OC0A
  #endif

  #if defined(__AVR_ATmega32U4__)
    _SPEAKER = 11; //PB7 OC0A, see above rant
  #endif

  #if defined(__AVR_ATmega168__) || defined(__AVR_ATmega328P__)
    _SPEAKER = 6; //PD6 OC0A
  #endif
  */
  // 
}

Volume::Volume()
{
}

/* no ThrowError - what was it here for?! */

void Volume::begin() {
  if (_toneAltPin == 255)
    _toneAltPin = 0;
  pinMode(_SPEAKER_PIN, OUTPUT);
  cli(); // stop interrupts
  TCCR1A = 0; // set entire TCCR1A register to 0
  TCNT1  = 0; // initialize counter value to 0
  // set compare match register for 10000 Hz increments
  OCR1A = F_CPU / (2 * 10000) - 1;
  // turn on CTC mode and set CS12, CS11 and CS10 bits for 1 prescaler, all other bits 0. 
  TCCR1B = (1 << WGM12) | (0 << CS12) | (0 << CS11) | (1 << CS10);
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  sei(); // allow interrupts
  TCCR2A = 0b00000011; // WGM2, no PWM channels enabled.
  TCCR2B = (TCCR2B & 0b11111000) | 0x01;
}

void Volume::end() {
  noTone();
  TIMSK1 &= ~(1 << OCIE1A); //turn off the interrupt
  TCCR2B = (TCCR2B & 0b11111000) | 0x04; //timer2 has a divide-by-32 option in there, so divide-by-64 is 1 higher.
}

void Volume::fadeOut(int duration){
  _fadeAmount = (1/float(_freq*2*(duration/1000.0)))*10;
  _fadeCounter = 0;
  _fadeOut = true; // Only set this after the iother two are staged (or disable interrupts while you do it, but this is better
}

void Volume::tone(int frequency, byte volume)
{
  _freq = frequency;
  _fadeOut = false;
  _fadeVol = 1.00;
  _toneEnable = true;
  long _clk = F_CPU / (1 * frequency * 2) - 1;
  if (_clk >= 65536) {
    _clk = 65536 - 1;
  }
  //cli(); // stop interrupts
  // Uhhhh I don't think you nwwed to disable interrupts here, yes, it is a 16-bit write, 
  // but it's to a 16-bit register, so the actual write happens in a single clock cycle. You 
  // only need interrupts disabled if there's other code that might mess with the temp register for timer1 
  // AND that code could running in an ISR do you have anything reading or writing OCR1x or TCNT1 in an ISR?... 
  // See the accessing 16-bit registers section of the Timer1 chapter of the datasheet.
  OCR1A = _clk;
  // sei(); // allow interrupts
  _toneVol = volume;
  reCalculateVol();
  return;
}

void Volume::noTone()
{
  _toneEnable = false;
  TCCR2A = 0b00000011; // WGM2, no PWM channels enabled. Faster than other ways to achieve this.
  OCR1A = 65535;
  _toneVol = 0;
  return;
}

/*
You don't need these anymore! 
void Volume::delay(unsigned long d) {
  ::delay(d * 64); 
  return;
}

unsigned long Volume::millis() {
  return ::millis() / 64;
}

unsigned long Volume::micros() {
  return ::micros() / 64;
}
*/
/*
And you never needed this, delayMicroseconds doesn't rely on the timers
void Volume::delayMicroseconds(unsigned long du) {
  ::delayMicroseconds(du  * 64);  //So this just made it wrong.
  return ;
}
*/
void Volume::recalculateVol() {
  if (_toneAltPin==1) {
    OCR2A = _toneVol * _masterVol * _fadeVol;
  } else {
    ORC2B = _toneVol * _masterVol * _fadeVol;
  }
}

void Volume::setMasterVolume(float mv) {
  _masterVol = mv;
  reCalculateVol();
  return;
}

ISR(TIMER1_COMPA_vect) {
  if (!toneEnable) return; 
  byte t = TCCR2A  ^ (_toneAltPin ? (1 << COM2A1) : (1 << COM2B1));   TCCR2A = t; 
	if(_fadeOut == true && !(t & (_toneAltPin ? (1 << COM2A1) : (1 << COM2B1)))) {
    // value of (_toneAltPin ? (1 << COM2A1) : (1 << COM2B1)) will still be in a register, compiler will know that, so it's just an AND between two registers if fadeout istrue. 
    // hence this runs only if fadeOut true and we just turned the PWM off. 
    // Not going to wade into this part.
    _fadeCounter++;
		if(_fadeCounter >= 10){
			_fadeCounter = 0;
			if(_fadeVol > 0) {
				_fadeVol-=_fadeAmount;
				if(_fadeVol < 0) {
					_fadeVol = 0;
				}
			} else{
				_fadeOut = false;
				_toneEnable = false;
				_toneVol = 0;
			}
      reCalculateVol(); // Ugh, we still have to do floating point math in an ISR! At least we only have to do it occasionally now during fade. 
		} //counter >=10
	}// fadeout stuff
}
