#include <MD_PWM.h>

/**
 * \file
 * \brief Code file for MD_PWM library class.
 */

// Global data
bool MD_PWM::_bInitialised = false;       
uint8_t MD_PWM::_cycleCount = 0;
MD_PWM* MD_PWM::_cbInstance[MAX_PWM_PIN];

MD_PWM::MD_PWM(uint8_t pin) : _pin(pin)
{ 
#if USE_DIRECT_IO
  _outReg = portOutputRegister(digitalPinToPort(pin)); // output pin register
  _outRegMask = digitalPinToBitMask(pin);              // mask for pin in that register
#endif
};

MD_PWM::~MD_PWM(void) 
// Last one out the door turns out the lights
{ 
  if (disable())
  {
    stop();
    detachISR();
  }
}

bool MD_PWM::begin(uint16_t freq)
{
  PWMPRINT("\nbegin Freq:", freq);

  if (freq > MAX_FREQUENCY)
    return(false);

  // Set up global data and hardware
  if (!_bInitialised)
  {
    // once only initialisation for all instances
    PWMPRINTS(" ... initializing");
    for (uint8_t i = 0; i < MAX_PWM_PIN; i++)
      _cbInstance[i] = nullptr;

    setTimerMode();
    setFrequency(freq);
    attachISR();

    _bInitialised = true;
  }

  // Always enable this instance of PWM pin
  pinMode(_pin, OUTPUT);
  return(enable());
}

bool MD_PWM::enable(void)
// Enable the PWM on the pin instance
{
  bool found = false;

  for (uint8_t i = 0; i < MAX_PWM_PIN; i++)
  {
    if (_cbInstance[i] == nullptr)
    {
      PWMPRINT("\nEnabling [", i);
      PWMPRINT("] for pin ", _pin);
      found = true;
      _cbInstance[i] = this;      // save ourselves in this slot
      _pwmDuty = _pwmDutySP = 0;  // initialize the duty cycle(s)
      break;
    }
  }

  return(found);
}

bool MD_PWM::disable(void)
// Disable the PWM on the pin instance
// Shuffle the instance table entries up to squish out empty slots
{
  for (uint8_t i = 0; i < MAX_PWM_PIN; i++)
  {
    if (_cbInstance[i] == this)
    {
      PWMPRINT("\nDisabling [", i);
      PWMPRINT("] for pin ", _pin);

      noInterrupts();   // stop IRQs that may access this table while reorganizing
      
      for (uint8_t j=i+1; j<MAX_PWM_PIN; j++)
        _cbInstance[i] = _cbInstance[j];      // shuffle along
      _cbInstance[MAX_PWM_PIN-1] = nullptr;   // clear the last one
      
      interrupts();     // IRQs can access again
      break;
    }
  }

  return(_cbInstance[0] == nullptr);    // table is empty, last instance gone
}

void MD_PWM::setPin(void)
// Handle the PWM counting and timing digital transitions
// This is called as part of the ISR instance handling
{
  if (_cycleCount == 0)
  {
    _pwmDuty = _pwmDutySP;    // set up new duty cycle if changed
    if (_pwmDuty != 0)
#if USE_DIRECT_IO
      *_outReg |= _outRegMask;
#else
      digitalWrite(_pin, HIGH);
#endif
  }
  else if (_cycleCount == _pwmDuty && _pwmDutySP != 0xff)
  {
#if USE_DIRECT_IO
    *_outReg &= ~_outRegMask;
#else
    digitalWrite(_pin, LOW);
#endif
  }
}

// -------------------------------------------------------------
// ---- Interrupt and Hardware Management
// -------------------------------------------------------------

// Interrupt service routine that for the timer
#if USE_TIMER == 1
ISR(TIMER1_OVF_vect)
#elif USE_TIMER == 2
ISR(TIMER2_OVF_vect)
#endif
{
  for (uint8_t i = 0; i < MD_PWM::MAX_PWM_PIN; i++)
  {
    if (MD_PWM::_cbInstance[i] == nullptr)
      break;      // None left - just exit
    MD_PWM::_cbInstance[i]->setPin();
  }
  MD_PWM::_cycleCount++;
}

inline void MD_PWM::setTimerMode(void)
{
#if USE_TIMER == 1
  TCCR1B = _BV(WGM13);

#elif USE_TIMER == 2
  TCCR2B = _BV(WGM22);

#endif
}

void MD_PWM::setFrequency(uint32_t freq)
// Set the timer to count closest to the required frequency * 256.
{
  uint8_t scale = 0;

  // The counter runs backwards after TOP, interrupt is at BOTTOM -
  // so multiply frequency by 256 (<<8) divide cycles by 2
  uint32_t cycles = (F_CPU / (freq << 8))/2;

#if USE_TIMER == 1
  // Work out the prescaler for this number of cycles
  if (cycles < TIMER_RESOLUTION) scale = _BV(CS10);              // prescale /1 (full xtal)
  else if ((cycles >>= 3) < TIMER_RESOLUTION) scale = _BV(CS11);              // prescale /8
  else if ((cycles >>= 3) < TIMER_RESOLUTION) scale = _BV(CS11) | _BV(CS10);  // prescale /64
  else if ((cycles >>= 2) < TIMER_RESOLUTION) scale = _BV(CS12);              // prescale /256
  else if ((cycles >>= 2) < TIMER_RESOLUTION) scale = _BV(CS12) | _BV(CS10);  // prescale /1024
  else     // request was out of bounds, set as maximum
  {
    cycles = TIMER_RESOLUTION - 1;
    scale = _BV(CS12) | _BV(CS10);
  }

  // now set up the counts
  TCCR1B &= ~(_BV(CS10) | _BV(CS11) | _BV(CS12));  // clear prescaler value
  TCCR1B |= scale;
  OCR1A = cycles;     // OCR1A is TOP in phase correct pwm mode for Timer1
  TCNT1 = 0;

#elif USE_TIMER == 2
  // Work out the prescaler for this number of cycles
  if      (cycles         < TIMER_RESOLUTION) scale = _BV(CS20);              // prescale /1 (full xtal)
  else if ((cycles >>= 3) < TIMER_RESOLUTION) scale = _BV(CS21);              // prescale /8
  else if ((cycles >>= 2) < TIMER_RESOLUTION) scale = _BV(CS21) | _BV(CS20);  // prescale /32
  else if ((cycles >>= 1) < TIMER_RESOLUTION) scale = _BV(CS22);              // prescale /64
  else if ((cycles >>= 1) < TIMER_RESOLUTION) scale = _BV(CS22) | _BV(CS20);  // prescale /128
  else if ((cycles >>= 1) < TIMER_RESOLUTION) scale = _BV(CS22) | _BV(CS21);  // prescale /256 
  else if ((cycles >>= 2) < TIMER_RESOLUTION) scale = _BV(CS22) | _BV(CS21) | _BV(CS20); // prescale by /1024
  else     // request was out of bounds, set as maximum
  {
    cycles = TIMER_RESOLUTION - 1;
    scale = _BV(CS22) | _BV(CS21) | _BV(CS20);
  }

  // now set up the counts
  TCCR2B &= ~(_BV(CS20) | _BV(CS21) | _BV(CS22));  // clear prescaler value
  TCCR2B |= scale;
  OCR2A = cycles;     // OCR2A is TOP in phase correct pwm mode for Timer2
  TCNT2 = 0;

#endif
}

inline void MD_PWM::attachISR(void)
// Start the timer and enable interrupt
{
  // set timer overflow interrupt enable bit
#if USE_TIMER == 1
  TIMSK1 = _BV(TOIE1);

#elif USE_TIMER == 2
  TIMSK2 = _BV(TOIE2);

#endif
  sei();                // interrupts globally enabled
}

inline void MD_PWM::detachISR(void)
// Stop the timer interrupt 
{
  // clears timer overflow interrupt enable bit 
#if USE_TIMER == 1
  TIMSK1 &= ~_BV(TOIE1);

#elif USE_TIMER == 2
  TIMSK2 &= ~_BV(TOIE2);

#endif
}

inline void MD_PWM::stop(void)
// Stop the timer
{
  // clears all clock selects bits
#if USE_TIMER == 1
  TCCR1B &= ~(_BV(CS10) | _BV(CS11) | _BV(CS12));

#elif USE_TIMER == 2
  TCCR2B &= ~(_BV(CS20) | _BV(CS21) | _BV(CS22));

#endif
}
