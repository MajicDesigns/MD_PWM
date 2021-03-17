#include <MD_PWM.h>

#include <avr/io.h>
#include <avr/interrupt.h>

/**
 * \file
 * \brief Code file for MD_PWM library class.
 */

// Global data
bool MD_PWM::_bInitialised = false;
volatile uint8_t MD_PWM::_pinCount;
MD_PWM* MD_PWM::_cbInstance[MAX_PWM_PIN];

MD_PWM::MD_PWM(uint8_t pin) : _pin(pin)
{ };

MD_PWM::~MD_PWM(void) 
// Last one out the door turns out the lights
{ 
  if (_pinCount == 0)
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
    PWMPRINTS(" ... initialising");
    for (uint8_t i = 0; i < MAX_PWM_PIN; i++)
      _cbInstance[i] = nullptr;

    _pinCount = 0;    // we have no pins globally allocated yet

    setTimerMode();
    setFrequency(freq);
    attachISR();

    _bInitialised = true;
  }

  // Always enable this instance of PWM pin
  pinMode(_pin, OUTPUT);
  return(enable());
}

void MD_PWM::write(uint8_t duty)
{
  _pwmDuty = duty;
}

bool MD_PWM::enable(void)
// Enable the PWM on the pin instance
{
  bool found = false;

  for (uint8_t i = 0; i < MAX_PWM_PIN; i++)
  {
    if (_cbInstance[i] == nullptr)
    {
      found = true;
      _cbInstance[i] = this;  // save ourtselves in this slot
      _pinCount++;        // one less pin to allocate
      _cycleCount = 0;    // initialise the counter for the pin
      write(0);           // initiliase the duty cycle
      break;
    }
  }

  return(found);
}

void MD_PWM::disable(void)
// disable the PWM on the pin instance
{
  for (uint8_t i = 0; i < MAX_PWM_PIN; i++)
  {
    if (_cbInstance[i] == this)
    {
      _cbInstance[i] = nullptr;         // erase ourselves from the slot
      if (_pinCount > 0) _pinCount--;   // one slot ius now free
      break;
    }
  }
}

void MD_PWM::setPin(void)
// Handle the PWM counting and timing digital transitions
{
  if (_cycleCount == 0 && _pwmDuty != 0)
    digitalWrite(_pin, HIGH);
  if (_cycleCount == _pwmDuty && _pwmDuty != 0xff)
    digitalWrite(_pin, LOW);
  _cycleCount++;   // 8 bit unsigned counter will simply roll over at 255
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
  if (MD_PWM::_pinCount)      // only do this if there are pins to process
  {
    for (uint8_t i = 0; i < MD_PWM::MAX_PWM_PIN; i++)
      if (MD_PWM::_cbInstance[i] != nullptr) MD_PWM::_cbInstance[i]->setPin();
  }
}

void MD_PWM::setTimerMode(void)
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

  // The counter runs backwards after TOP, interrupt is at BOTTOM 
  // so divide cycles by 2
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

void MD_PWM::attachISR(void)
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

void MD_PWM::detachISR(void)
// Stop the timer interrupt 
{
  // clears timer overflow interrupt enable bit 
#if USE_TIMER == 1
  TIMSK1 &= ~_BV(TOIE1);

#elif USE_TIMER == 2
  TIMSK2 &= ~_BV(TOIE2);

#endif
}

void MD_PWM::stop(void)
// Stop the timer
{
  // clears all clock selects bits
#if USE_TIMER == 1
  TCCR1B &= ~(_BV(CS10) | _BV(CS11) | _BV(CS12));

#elif USE_TIMER == 2
  TCCR2B &= ~(_BV(CS20) | _BV(CS21) | _BV(CS22));

#endif
}
