#pragma once
/**
\mainpage Any pin PWM Library

This library is designed to provide 'software' PWM output for any 
digital pin.

Important Notes:
- This library uses AVR TIMER1 or TIMER2 to implement the interrupt 
driven clock. TIMER0 is used by the Arduino millis() clock, TIMER1 
is commonly used by the Servo library and TIMER2 by the Tone library. 
Change USE_TIMER (defined at the top of the header file) to select 
which timer is enabled in the library code.

- This library has been tested on Arduino Uno and Nano (ie, 328P processor).
It should work on other AVR chips (eg, MEGA) with slight modifications but 
it will not work on non-AVR architectures without some extensive rework.

# Why use Software PWM?

The Uno and Nano have 6 hardware PWM pins (3, 5, 6, 9, 10, 11). However, of
these pins, the following also have alternative functions:

| Pin| Alternate Use
|----|----------
|  3 | External Interrupt
|  5 | - 
|  6 | - 
|  9 | - 
| 10 | SPI SS (default) 
| 11 | SPI MOSI

So, if the application requires 2 external interrupts and an SPI interface,
there are really only 3 PWM hardware signals available for additional 
control. As an example, for 2 DC motors with a PWM controller, 4 PWM signals 
are required, so we don't have enough hardware PWM pins to get the job done.

One solution is to change hardware to a processor with more PWM pins. 
Another is to create a PWM solution that uses software to drive the pins.

This second option is feasible, especially when the PWM signal needed is 
relatively low frequency. The downside is that the CPU is used process the 
timer interrupt and toggle the PWM digital output, taking processing time 
away from other tasks.

The original use case for this library was for PWM speed control of brushed DC 
motors. The default Arduino Uno/Nano PWM frequency is 490.2 Hz for pins 3, 9,
10, 11 and 976.56 Hz for pins 5 and 6. These frequencies are actually too high 
to properly drive DC motors at low duty cycles, as the current through the 
motor coils does not rise fast enough to provide motive force through the 
motors. See this excellent summary from Adafruit
https://learn.adafruit.com/improve-low-speed-performance-of-brushed-dc-motors/pwm-frequency.

# Implementation

This library implements user defined frequency PWM output for any digital pin 
software limited to MAX_FREQUENCY Hz.

The TIMERn is set for 255 times this frequency (eg, 200Hz becomes 51kHz). This 
causes the TIMERn interrupt routine to be called 255 times for each PWM cycle 
and, depending on where it is in the cycle, allows the software to set the 
digital output pin to LOW or HIGH, thus creating the desired PWM signal. 
This is illustrated below.

![PWM Timing Diagram] (PWM_Timing.png "PWM Timing Diagram")

The duty cycle can be changed very smoothly by changing the set point at which 
the digital transition occurs. The new duty cycle takes effect at the next 
PWM digital transition.

TIMERn is a global resource, so each object instance of class is driven from the
same TIMERn interrupt. The constant MAX_PWM_PIN is used to set limits the
global maximum for instances allowed to be processed by the interrupt.

See Also
- \subpage pageRevisionHistory
- \subpage pageDonation
- \subpage pageCopyright

\page pageDonation Support the Library
If you like and use this library please consider making a small donation 
using [PayPal](https://paypal.me/MajicDesigns/4USD)

\page pageCopyright Copyright
Copyright (C) 2021 Marco Colli. All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

\page pageRevisionHistory Revision History
Sep 2021 ver xxx
- Fixed reported glitch in setPin()

Sep 2021 ver 1.0.3
- disable() now called in destructor

Apr 2021 ver 1.0.2
- cyclecount and duty variables now volatile.

Mar 2021 ver 1.0.1
- Minor tweaks

Mar 2021 ver 1.0.0
- Initial release
 */

#include <Arduino.h>

 /**
 * \file
 * \brief Main header file and class definition for the MD_PWM library.
 */

#ifndef PWMDEBUG
#define PWMDEBUG 0      ///< 1 turns debug output on
#endif

#ifndef USE_TIMER
#define USE_TIMER 2     ///< Set to use hardware TIMER1 or TIMER2 (1 or 2)
#endif

#if PWMDEBUG
#define PWMPRINT(s,v)   do { Serial.print(F(s)); Serial.print(v); } while (false)
#define PWMPRINTX(s,v)  do { Serial.print(F(s)); Serial.print(F("0x")); Serial.print(v, HEX); } while (false)
#define PWMPRINTB(s,v)  do { Serial.print(F(s)); Serial.print(F("0b")); Serial.print(v, BIN); } while (false)
#define PWMPRINTS(s)    do { Serial.print(F(s)); } while (false)
#else
#define PWMPRINT(s,v)
#define PWMPRINTX(s,v)
#define PWMPRINTB(s,v)
#define PWMPRINTS(s)
#endif

/**
 * Core object for the MD_PWM library
 */
class MD_PWM
{
  public:
    static const uint16_t MAX_FREQUENCY = 300;   ///< the maximum PWM frequency allowed
    static const uint8_t MAX_PWM_PIN = 4;        ///< total number of concurrent PWM pins that can be used

  //--------------------------------------------------------------
  /** \name Class constructor and destructor.
   * @{
   */
  /**
   * Class Constructor
   *
   * Instantiate a new instance of the class.
   *
   * The main function for the core object is to set the internal
   * shared variables and timers to default values.
   * 
   * \param pin pin number for this PWM output.
   */
    MD_PWM(uint8_t pin);

  /**
   * Class Destructor.
   *
   * Release any allocated memory and clean up anything else.
   * 
   * If all the instances of the class are closed, then the ISR is
   * disconnected and the timer is stopped.
   */
    ~MD_PWM(void);
  /** @} */

  //--------------------------------------------------------------
  /** \name Methods for core object control.
   * @{
   */
  /**
   * Initialize the object.
   *
   * Initialize the object data. This needs to be called during setup() 
   * to set items that cannot be done during object creation.
   * 
   * If this is the first instance of this class, then the ISR code 
   * is initialized and the frequency of the hardware timer is set.
   * Subsequent instances do not affect the timer frequency.
   * 
   * \sa enable()
   * 
   * \param freq the PWM frequency in Hz [0..MAX_FREQUENCY] (first instance only).
   * \return true if the pin was successfully enabled for PWM.
   */
    bool begin(uint16_t freq = MAX_FREQUENCY);

  /**
   * Write PWM output value.
   *
   * Set the PWM output value for the pin. This works like the 
   * standard analogWrite() for hardware enabled PWM - 0% duty cycle
   * is 0, 50% is 127 and 100% is 255.
   *
   * \param duty the PWM duty cycle [0..255].
   */
    inline void write(uint8_t duty) { _pwmDuty = duty; if(_cycleCount >= _pwmDuty) _cycleCount = _pwmDuty; }

  /**
   * Disable PWM output for this pin.
   *
   * Stops PWM output for the pin related to this class instance.
   * The pin relinquishes its slot allocated for the ISR [0..MAX_PWM_PINS],
   * which can be reused for another pin if needed.
   * 
   * \sa enable()
   */
    void disable(void);

  /**
   * Enables PWM output for this pin.
   *
   * Starts PWM output for the pin related to this class instance.
   * The pin takes the next available slot allocated for the ISR [0..MAX_PWM_PINS].
   * If no slots are available, then the method returns false.
   * 
   * \sa disable()
   * 
   * \return true if the pin was successfully enabled.
   */
    bool enable(void);

  /** @} */
private:
#if USE_TIMER == 1
    static const uint32_t TIMER_RESOLUTION = 65535;    ///< Timer1 is 16 bit

#elif USE_TIMER == 2
    static const uint32_t TIMER_RESOLUTION = 256;    ///< Timer2 is 8 bit

#endif

    uint8_t _pin;         // PWM digital pin
    volatile uint8_t _pwmDuty;     // PWM duty set point
    volatile uint8_t _cycleCount;  // PWM current cycle count

    void setFrequency(uint32_t freq); // set TIMER frequency
    inline void setTimerMode(void);   // set TIMER mode
    inline void attachISR(void);      // attach to TIMER ISR
    inline void detachISR(void);      // detach from TIMER ISR
    inline void stop(void);           // stop the timer

public:
  static bool _bInitialised;          ///< ISR - Global vector initialization flag
  static volatile uint8_t _pinCount;  ///< ISR - Number of pins currently configured
  static MD_PWM* _cbInstance[];       ///< ISR - Callback instance handle per pin slot

  //--------------------------------------------------------------
  /** \name ISR use only - NOT FOR END USERS.
   * @{
   */
   /**
   * Set the output pin (for ISR use only).
   *
   * Set the output PWM pin based on the current counter value. This is
   * called 255 times during each PWM cycle.
   * 
   * This method is for use only by the TIMERn ISR.
   */
   void setPin(void);

    /** @} */
};

