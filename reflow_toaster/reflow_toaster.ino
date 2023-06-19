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

#define LED2 2   // External LED Pin
#define LED13 13 // Arduino onboard LED Pin
#define POT A3 // Potentiometer Pin

#define CS 11   // MAX6675 Chip Select
#define SO 10   // MAX6675 Serial data Output a.k.a MISO
#define SCLK 12 // MAX6675 Clock a.k.a SCK, CLK

#define oven_SSR_pin 3 // S.S.R control
// #define door_control 9 //oven door control servo motor

// declare variables
float temperature;
int pot_value;
int delta;
unsigned long last_time;
boolean toggle = 0;
int servo_pos;
int state = 1;      // There are 5 states: 1. ramp to soak, 2. soak, 3. ramp to peak, 4. cooling and 5. End
int soak_start;     // variable to hold start of soak state time.
int peak_start;     // variable to hold start of peak state time.
int state_start;    // variable to hold start of state time.
int tick_count = 0; // temperature read event counter. increments every 0.25 sec.
float integral = 0; // I element of the PID

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
  // myservo.attach(door_control); // attaches the door control servo to the servo object
  pinMode(oven_SSR_pin, OUTPUT);
  pinMode(POT, INPUT);

  pinMode(CS, OUTPUT);
  pinMode(SO, INPUT);
  pinMode(SCLK, OUTPUT);

  pinMode(LED2, OUTPUT);
  pinMode(LED13, OUTPUT);

  // OLED setup
  u8x8.begin();
  u8x8.setFont(u8x8_font_px437wyse700a_2x2_r);

  // Read the temperature for the 1st time
  //======================================
  digitalWrite(CS, LOW);
  digitalWrite(CS, HIGH); // start new MAX6657 temperature conversion
  delay(250);
  last_time = millis();
  temperature = read_MAX6657();
  temperature = temperature / 4;
  //======================================

  // myservo.write(closeOvenDoor); // Close the oven door
  state_start = millis() / 1024;
}

void loop()
{
  pot_value = analogRead(POT); // read the potentiometer value
  pot_value = map(pot_value, 0, 1023, 0, 300);
  // convert pot_value to always be 4-letter string
  String pot_value_str = String(pot_value);
  if (pot_value < 10)
  {
    pot_value_str = "  " + pot_value_str;
  }
  else if (pot_value < 100)
  {
    pot_value_str = " " + pot_value_str;
  }

  if (state != 5)
  {
    // Serial.print(tick_count++);
    Serial.print(" ; Oven temperature = ");
    Serial.print(temperature);
    Serial.print(" ");
    Serial.print(pot_value);

    // u8x8.setCursor(0, 1);
    // u8x8.print("STAGE: ");
    // u8x8.setCursor(12, 1);
    // u8x8.print(counter);

    int row = 0;
    u8x8.setCursor(0, row);
    u8x8.print(" ATtami ");

    row += 2;
    u8x8.setCursor(0, row);
    u8x8.print("Set");

    u8x8.setCursor(8, row);
    u8x8.print(pot_value_str);

    row += 2;
    u8x8.setCursor(0, row);
    u8x8.print("Get");

    u8x8.setCursor(11, row);
    u8x8.print(int(temperature));

    row += 2;
    u8x8.setCursor(0, row);
    u8x8.print("State");

    u8x8.setCursor(11, row);
    u8x8.print(state);
  }

  switch (state)
  {

  case 1: // Ramp to Soak
    if (temperature < soak_temp)
    {
      digitalWrite(LED2, HIGH);
    }
    else
    {
      soak_start = millis() / 1024;
      state_start = millis() / 1024;
      state = 2;
    }
    delta = soak_temp - temperature;
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
    if (temperature < soak_temp)
    {
      OCR2B = 100; // oven "ON" with partial power
      digitalWrite(LED2, HIGH);
    }
    else
    {
      OCR2B = 0; // oven "OFF"
      digitalWrite(LED2, LOW);
    }
    if ((millis() / 1024 - soak_start) > soak_duration)
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
    if (temperature < peak_temp)
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
    if (temperature > 100)
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

  case 5:
    break;

  default:
    state = 5;
  }

  digitalWrite(LED13, toggle);
  toggle = ~toggle;

   while ((millis() - last_time) < MAX6675_time)
    ; // wait for MAX6657 temperature measurement
  last_time = millis();
  temperature = read_MAX6657();
  temperature = temperature / 4;

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
