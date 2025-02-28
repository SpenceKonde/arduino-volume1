/*
  Volume.h - Library for tone() with 8-bit volume control.
  Created by Connor Nishijima, May 25th 2016.
  Updated June 14, 2016.
  Released under the GPLv3 license.
*/
#ifndef volume_h
#define volume_h

#include "Arduino.h"

class Volume
{
  public:
    Volume();
    void begin();
    void end();
    void alternatePin(bool enabled);
    void tone(int frequency, byte volume);
    void fadeOut(int duration);
    void noTone();
    void setMasterVolume(float mv);
};

#endif
