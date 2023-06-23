/*
TAMI Reflow Controller
 Created by Zvi Schneider 15-09-2014
 Last update 06-01-2015
 */

// #include <Servo.h>
// Servo myservo; // Initialize the SERVO library

// OLED
#include <U8x8lib.h>
U8X8_SH1106_128X64_NONAME_HW_I2C u8x8(/* reset=*/U8X8_PIN_NONE); // https://github.com/olikraus/u8g2/wiki/u8x8setupcpp

// assign Arduino pins

// connect OLED SCK --> A5
// connectr OLED SDA --> A4

#define LED2 2 // External LED Pin
#define POT A3 // Potentiometer Pin

#define CS 11   // MAX6675 Chip Select
#define SO 10   // MAX6675 Serial data Output a.k.a MISO
#define SCLK 12 // MAX6675 Clock a.k.a SCK, CLK

#define oven_SSR_pin 3 // S.S.R control
// #define door_control 9 //oven door control servo motor

// declare variables
int temp_get_value;
int temp_set_value_old;
int temp_set_value;
int delta;
unsigned long last_time;
boolean toggle = 0;
int servo_pos;
int state;          // There are 5 states: 1. ramp to soak, 2. soak, 3. ramp to peak, 4. cooling and 5. End
int soak_start;     // variable to hold start of soak state time.
int soak_time;
int peak_start;     // variable to hold start of peak state time.
int state_start;    // variable to hold start of state time.
int tick_count = 0; // temperature read event counter. increments every 0.25 sec.
float integral = 0; // I element of the PID
int n_cols;

long manual_mode_millis_value;
long manual_mode_millis_start;
long manual_mode_all_seconds_value;
int manual_mode_minutes_value;
String manual_mode_minutes_str;
int manual_mode_seconds_value;
String manual_mode_seconds_str;

String mode_str, temp_str, state_time_1_str, state_time_2_str;

// define names
#define delta_temperature 12 // Start slowing the oven power "delta_temperature" Degrees before reaching soak temperature. This eliminates temperature overshot.
// adjust "delta_temperature" so that the overshot is between 5 to 10 degrees. high "delta_temperature" yields low overshot.
#define MAX6675_time 250 // The minimum MAX6675 temperature measuring time is 220 mili-sec. We will use 250 mili-sec.
#define closeOvenDoor 1  // Servo angle for closed door
#define openOvenDoor 140 // Servo angle for opened door
#define soak_temp 150    // profile parameter
#define soak_duration 75 // profile parameter
#define peak_temp 230    // profile parameter
// profile parameter

void setup()
{
  // Set timer2 to provide PWM on Pin 3
  //==================================
  TCCR2A = (1 << COM2B1);                           // Clear OC2B on Compare Match when up-counting. Set OC2B on Compare Match when down-counting.
  TCCR2A = TCCR2A | (1 << WGM20);                   // Set PWM, Phase Correct mode with TOP = 0xFF (WGM21 and WGM22 are zero by default).
  TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20); // Select clkT2S/1024 From Prescaler.
  OCR2B = 255;                                      // oven "ON" with full power. OCR2B value controls the oven duty cycle between 0 to 100%. 255 sets oven power to 100%.
  //==================================
  
  Serial.begin(115200);

  pinMode(POT, INPUT);
  pinMode(oven_SSR_pin, OUTPUT);

  // TEMPERATURE SENSOR SETUP
  pinMode(CS, OUTPUT);
  pinMode(SO, INPUT);
  pinMode(SCLK, OUTPUT);

  // led setup
  pinMode(LED2, OUTPUT);

  // OLED setup
  u8x8.begin();
  //u8x8.setFont(u8x8_font_px437wyse700a_2x2_r);
  u8x8.setFont(u8x8_font_8x13B_1x2_f);
  //u8x8.setFont(u8x8_font_pcsenior_f);
  n_cols = u8x8.getCols();

  // Read the temperature for the 1st time
  //======================================
  digitalWrite(CS, LOW);
  digitalWrite(CS, HIGH); // start new MAX6657 temperature conversion
  delay(250);
  last_time = millis();
  temp_get_value = read_MAX6657();
  temp_get_value = temp_get_value / 4;
  //======================================

  state_start = millis() / 1024;

  // if pot is set to 0 on start move to manual mode, stage 99
  temp_set_value = analogRead(POT); // read the potentiometer value
  if (temp_set_value < 5)
  {
    state = 99;
  }
  else
  {
    state = 1;
  }

  manual_mode_millis_start = 0;
}

void loop()
{ 
  if (state == 99)
  {
    mode_str = "Mode:  MANUAL";

    temp_set_value_old = temp_set_value;
    temp_set_value = analogRead(POT) / 4;

    manual_mode_millis_value = millis();

    if (temp_set_value < 5)
    {
      temp_set_value = 0;
      manual_mode_minutes_str = "--";
      manual_mode_seconds_str = "--";
    }
    else
    { 
      if (abs(temp_set_value_old - temp_set_value) > 5)
      {
        manual_mode_millis_start = manual_mode_millis_value;
      }
      
      manual_mode_all_seconds_value = (manual_mode_millis_value - manual_mode_millis_start) / 1000;
      manual_mode_minutes_value = manual_mode_all_seconds_value / 60;
      manual_mode_seconds_value = manual_mode_all_seconds_value % 60;

      manual_mode_minutes_str = String(manual_mode_minutes_value);
      if (manual_mode_minutes_value < 10)
      {
        manual_mode_minutes_str = "0" + manual_mode_minutes_str;
      }

      manual_mode_seconds_str = String(manual_mode_seconds_value);
      if (manual_mode_seconds_value < 10)
      {
        manual_mode_seconds_str = "0" + manual_mode_seconds_str;
      }
    }

    state_time_1_str = "Time:  " + manual_mode_minutes_str + ":" + manual_mode_seconds_str;
    state_time_2_str = "";
  }
  else
  {
    mode_str = "Mode:  AUTO";

    state_time_1_str = "State: " + String(state) + "/5";

    if (state == 1)
    {
      temp_set_value = soak_temp;

      state_time_2_str = "Ramp to Soak...";
      int n_chars = state_time_2_str.length();
      for (int i = 0; i < n_cols - n_chars; i++)
      {
        state_time_2_str = state_time_2_str + " ";
      }
      
    }
    else if (state == 2)
    {
      temp_set_value = soak_temp;

      int soak_left_value = soak_duration - soak_time;
      String soak_left_str = String(soak_left_value);
      if (soak_left_value < 10)
      {
        state_time_1_str += "   (" + soak_left_str + ")";
      }
      else
      {
        state_time_1_str += "  (" + soak_left_str + ")";
      }
      int n_chars = state_time_1_str.length();
      for (int i = 0; i < n_cols - n_chars; i++)
      {
        state_time_1_str = state_time_1_str + " ";
      }

      state_time_2_str = "Soak...";
      n_chars = state_time_2_str.length();
      for (int i = 0; i < n_cols - n_chars; i++)
      {
        state_time_2_str = state_time_2_str + " ";
      }
    }
    else if (state == 3)
    {
      temp_set_value = peak_temp;

      state_time_2_str = "Ramp to Peak...";
      int n_chars = state_time_2_str.length();
      for (int i = 0; i < n_cols - n_chars; i++)
      {
        state_time_2_str = state_time_2_str + " ";
      }
    }
    else if (state == 4)
    {
      temp_set_value = 100;

      state_time_2_str = "Cooling...";
      int n_chars = state_time_2_str.length();
      for (int i = 0; i < n_cols - n_chars; i++)
      {
        state_time_2_str = state_time_2_str + " ";
      }
    }
    else if (state == 5)
    {
      temp_set_value = 0;

      state_time_2_str = "End :)";
      int n_chars = state_time_2_str.length();
      for (int i = 0; i < n_cols - n_chars; i++)
      {
        state_time_2_str = state_time_2_str + " ";
      }
    }
  }
  
  String temp_get_value_str = String(temp_get_value) + "\xb0";
  if (temp_get_value < 10)
  {
    temp_get_value_str = temp_get_value_str + "  ";
  }
  else if (temp_get_value < 100)
  {
    temp_get_value_str = temp_get_value_str + " ";
  }

  String temp_set_value_str = String(temp_set_value) + "\xb0";
  if (temp_set_value < 10)
  {
    temp_set_value_str = "  " + temp_set_value_str;
  }
  else if (temp_set_value < 100)
  {
    temp_set_value_str = " " + temp_set_value_str;
  }

  temp_str = "Temp:  " + temp_get_value_str + ">" + temp_set_value_str;

  int row = 0;
  u8x8.setCursor(0, row);
  u8x8.print(mode_str);
  
  row += 2;
  u8x8.setCursor(0, row);
  u8x8.print(temp_str);

  row += 2;
  u8x8.setCursor(0, row);
  u8x8.print(state_time_1_str);

  row += 2;
  u8x8.setCursor(0, row);
  u8x8.print(state_time_2_str);

  switch (state)
  {

  case 1: // Ramp to Soak
    if (temp_get_value < soak_temp)
    {
      digitalWrite(LED2, HIGH);
    }
    else
    {
      soak_start = millis() / 1024;
      state_start = millis() / 1024;
      state = 2;
    }
    delta = soak_temp - temp_get_value;
    if (delta < delta_temperature)
    { // simple PID control
      integral = integral + 1;
      delta = map(delta, 0, delta_temperature, 128, 255) + integral;
      OCR2B = delta; // oven "ON" with proportional to delta partial power
    }
    else
      OCR2B = 255; // oven "ON" full power

    Serial.print(" ; state time = ");
    Serial.print(millis() / 1024 - state_start);
    Serial.println(" ; State 1, Ramp to Soak");

    break;

  case 2: // Soak
    if (temp_get_value < soak_temp)
    {
      OCR2B = 100; // oven "ON" with partial power
      digitalWrite(LED2, HIGH);
    }
    else
    {
      OCR2B = 0; // oven "OFF"
      digitalWrite(LED2, LOW);
    }
    soak_time = millis() / 1024 - soak_start;
    if (soak_time > soak_duration)
    {
      peak_start = millis() / 1024;
      state_start = millis() / 1024;
      state = 3;
    }
    Serial.print(" ; state time = ");
    Serial.print(millis() / 1024 - state_start);
    Serial.println(" ; State 2, Soak");
    break;

  case 3: // Ramp to Peak
    if (temp_get_value < peak_temp)
    {
      OCR2B = 255; // oven "ON" with  power
      digitalWrite(LED2, HIGH);
    }
    else
    {
      // OCR2B = 0; //oven "OFF"
      // digitalWrite(LED2, LOW);
      state = 4;
    }

    Serial.print(" ; state time = ");
    Serial.print(millis() / 1024 - state_start);
    Serial.println(" ; State 3, Ramp to Peak");
    break;

  case 4: // End (Cooling)
    // myservo.write(openOvenDoor);
    OCR2B = 0;
    digitalWrite(LED2, toggle);
    if (temp_get_value > 100)
    {
      Serial.print(" ; state time = ");
      Serial.print(millis() / 1024 - state_start);
      Serial.println(" ; State 4, End (Cooling)");
    }
    else
    {
      state = 5;
      Serial.println("*** End of Game ***");
    }
    break;

  case 99:
    // Manual mode, get the potentiometer value
    // and set the oveen power
    if (temp_get_value < temp_set_value)
    {
      OCR2B = 255; // oven "ON" with full power
      digitalWrite(LED2, HIGH);
    }
    else
    {
      OCR2B = 0; // oven "OFF"
      digitalWrite(LED2, LOW);
    }
    Serial.println(" ; State 99, manual mode");
    break;

  case 5:
    break;

  default:
    state = 5;
  }

  if (state != 5)
  {
    Serial.print("Temp (degrees): ");
    Serial.print(temp_get_value);
    Serial.print(" / ");
    Serial.print(temp_set_value);
    Serial.print(" (");
    Serial.print(temp_set_value_old);
    Serial.print(")");
    Serial.print("; time base (sec): ");
    Serial.print(manual_mode_millis_start);
    Serial.print("; time (sec): ");
    Serial.print(manual_mode_all_seconds_value);
  }

  toggle = ~toggle;

  while ((millis() - last_time) < MAX6675_time)
    ; // wait for MAX6657 temperature measurement
  last_time = millis();
  temp_get_value = read_MAX6657();
  temp_get_value = temp_get_value / 4;
}

int read_MAX6657()
{
  int temp = 0;
  digitalWrite(CS, LOW);    // Initiate MAX6675 read cycle
  digitalWrite(SCLK, HIGH); // Get read of the MSB
  digitalWrite(SCLK, LOW);
  for (int i = 12; i > 0; i--)
  { // read the 12 relevant bits
    temp = (temp << 1) | digitalRead(SO);
    digitalWrite(SCLK, HIGH);
    digitalWrite(SCLK, LOW);
  }
  digitalWrite(CS, HIGH); // Discard the 3 LSBs and Initiate new MAX6675 temperature measurement
  return temp;
}
