/*
* Simple program to test MD_PWM functions.
* 
* Rotary Encoder connected to ENC_* pins can be used to vary the PWM 
* output of class instance pins specified in the pwm array.
* Encoder value is echoed to the Serial monitor.
* Switch between encoder setting and sweep mode by pressing on the 
* encoder switch.
* 
* NOTE: PWM array is declared one larger than maximum pins 
* allowed to test library functionality, so failed 
* initialisation on the last element is intentional!
* 
* MD_REncoder is available at https://github.com/MajicDesigns/MD_REncoder or Arduino Library Manager
* MD_UISwitch is available at https://github.com/MajicDesigns/MD_UISwitch or Arduino Library Manager
*/

#include <MD_REncoder.h>
#include <MD_UISwitch.h>
#include <MD_PWM.h>

const uint8_t ENC_A = 2;
const uint8_t ENC_B = 3;
const uint8_t ENC_SW = 4;

MD_REncoder E(ENC_A, ENC_B);
MD_UISwitch_Digital S(ENC_SW);

MD_PWM pwm[] = { MD_PWM(5), MD_PWM(7), MD_PWM(8), MD_PWM(9), MD_PWM(10) };
const uint16_t PWM_FREQ = 50;    // in Hz
const uint8_t SWEEP_DELAY = 10;  // milliseconds

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

void setup(void)
{
  Serial.begin(57600);
  Serial.println("---");

  E.begin();
  S.begin();

  for (uint8_t i = 0; i < ARRAY_SIZE(pwm); i++)
  {
    if (!pwm[i].begin(PWM_FREQ))
    {
      Serial.print("\nUnable to initialize pwm #");
      Serial.println(i);
    }
  }
}

void readEncoder(uint8_t& v)
{
  uint8_t x = E.read();

  if (x == DIR_CW) v++;
  else if (x == DIR_CCW) v--;
}

void modeEncoder(void)
{
  static uint8_t dutyCur = 127, dutyPrev = 0;

  readEncoder(dutyCur);

  if (dutyCur != dutyPrev)
  {
    Serial.println(dutyCur);
    for (uint8_t i = 0; i < ARRAY_SIZE(pwm); i++)
      pwm[i].write(dutyCur);
    dutyPrev = dutyCur;
  }
}

void modeSweep(int8_t direction)
{
  static uint8_t state = 0, counter = 255;
  static uint32_t timeStart;

  switch (state)
  {
  case 0:
    //Serial.println(counter);
    pwm[0].write(counter);
    counter += direction;
    timeStart = millis();
    state = 1;
    break;

  case 1:
    if (millis() - timeStart >= SWEEP_DELAY)
      state = 0;
    break;
  }
}


void loop(void)
{
  static enum { M_ENC, M_SWP_UP, M_SWP_DWN } mode = M_ENC;
  MD_UISwitch::keyResult_t k = S.read();

  // check the switch and change mode
  if (k == MD_UISwitch::KEY_PRESS)
  {
    switch (mode)
    {
    case M_ENC:    Serial.print("\n-> Swp Up\n");  mode = M_SWP_UP;   break;
    case M_SWP_UP: Serial.print("\n-> Swp Dwn\n"); mode = M_SWP_DWN;  break;
    default:       Serial.print("\n-> Encoder\n"); mode = M_ENC;      break;
    }
  }
  else if (k == MD_UISwitch::KEY_LONGPRESS)
  {
    Serial.print("\nDisabling");
    pwm[0].disable();
  }
  else if (k == MD_UISwitch::KEY_DPRESS)
  {
    Serial.print("\nEnabling");
    if (!pwm[0].enable()) Serial.print(" - error");
  }

  // do according to mode
  switch (mode)
  {
    case M_SWP_UP:  modeSweep(1);   break;
    case M_SWP_DWN: modeSweep(-1);   break;
    case M_ENC:     modeEncoder(); break;
  }
}