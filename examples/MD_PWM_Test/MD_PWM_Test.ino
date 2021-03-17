#include <MD_PWM.h>

/*
* Simple program to test MD_PWM functions.
* 
* Pot connected to A0 can be used to vary the PWM 
* output of class instance pins specified in the pwm array.
* Pot value is echoed to the Serial monitor.
* 
* NOTE: PWM array is declared one larger than maximum pins 
* allowed to test library functionality, so failed 
* initialisation on the last element is intentional!
*/

MD_PWM pwm[] = { MD_PWM(5), MD_PWM(7), MD_PWM(8), MD_PWM(9), MD_PWM(10) };
const uint16_t PWM_FREQ = 50;    // in Hz

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

void setup(void)
{
  Serial.begin(57600);
  Serial.println("---");
  for (uint8_t i = 0; i < ARRAY_SIZE(pwm); i++)
  {
    if (!pwm[i].begin(PWM_FREQ))
    {
      Serial.print("\nUnable to initialize pwm #");
      Serial.print(i);
    }
  }
}

void loop(void)
{
  uint16_t pot = analogRead(A0) >> 2; // divided by 4;
  static uint16_t lastPot = 0;

  if (pot != lastPot)
  {
    Serial.print("\nPot: "); 
    Serial.print(pot);
    for (uint8_t i=0; i < ARRAY_SIZE(pwm); i++)
      pwm[i].write(pot);
    lastPot = pot;
  }
}