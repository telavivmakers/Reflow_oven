/*
TAMI Reflow Controller
 Created by Zvi Schneider 15-09-2014
 Hardware Version: "TAMI - Reflow Controller V5.sch"
 */
#include <Servo.h>
Servo myservo; // Initialize the SERVO library

int LED2 = 2;               // External LED Pin
int LED13 = 13;             // Arduino onboard LED Pin
int CS = 11;                //MAX6675 Chip Select
int SO = 10;                //MAX6675 Serial data Output
int SCLK = 12;              //MAX6675 Clock
float temperature;
int delta;
unsigned long last_time;
int oven = 3;               //S.S.R control
boolean toggle = 0;
int servo_pos;
#define delta_temperature 5 // 7 Deg before soak start slowing the oven power
#define MAX6675_time 250 // The MAX6675 temperature measuring time is 220 mili-sec. We will give it 250 mili-sec.
#define closeOvenDoor 1 // Servo angle for closed door
#define openOvenDoor 140 // Servo angle for opened door
#define soack_temp 150 // profile parameter
#define soak_duration 100 // profile parameter 
#define peak_temp 215 // profile parameter
int state = 1; // There are 4 states: 1. ramp to soak, 2. soak, 3. ramp to peak, 4. cooling and 5. End
int soak_start; // variable to hold start of soak state time.



void setup(){
  //Set timer2 to provide PWM on Pin 3
  //==================================
  TCCR2A = (1 << COM2B1); //Clear OC2B on Compare Match when up-counting. Set OC2B on Compare Match when down-counting.
  TCCR2A = TCCR2A |(1 << WGM20); // Set PWM, Phase Correct mode with TOP = 0xFF (WGM21 and WGM22 are zero by default).
  TCCR2B = (1 << CS22) |(1 << CS21) |(1 << CS20); //Select clkT2S/1024 From Prescaler.
  OCR2B = 255; // set oven power to 100%
  //==================================

  Serial.begin (19200);
  myservo.attach(9);  // attaches the servo on pin 9 to the servo object
  pinMode (oven, OUTPUT); 
  pinMode(LED2, OUTPUT);
  pinMode(LED13, OUTPUT);
  pinMode (CS, OUTPUT);
  pinMode (SO, INPUT);
  pinMode (SCLK, OUTPUT);


  // Read the temperature for the 1st time
  //======================================
  digitalWrite(CS, LOW);
  digitalWrite(CS, HIGH); // start new MAX6657 temperature conversion
  delay (250);
  last_time = millis();
  temperature = read_MAX6657();
  temperature = temperature/4;
  //======================================

  myservo.write(closeOvenDoor); // Close the oven door

}

void loop (){
  if (state != 5){
    Serial.print(temperature);
    Serial.print (": ");
  }

  switch (state) {

  case 1: // Ramp to Soak
    if (temperature < soack_temp){ 
      digitalWrite(LED2, HIGH);
    }
    else {
      soak_start = millis()/1024;
      state = 2;
    }
    delta = soack_temp - temperature;
    if (delta < delta_temperature){
      map(delta, 0, delta_temperature, 128, 255);
      OCR2B =  delta;
    }
    else OCR2B = 255;
    Serial.println ("Ramp to Soak - ");  
    break;

  case 2: // Soak
    if (temperature < soack_temp){
      OCR2B = 80; 
      digitalWrite(LED2, HIGH);
    }
    else {
      OCR2B=0;//digitalWrite(oven, LOW);
      digitalWrite(LED2, LOW);
    }
    if ((millis()/1024 - soak_start) >  soak_duration){
      state = 3;
    }
    Serial.println ("Soak - ");
    break;

  case 3: // Ramp to Peak
    if (temperature < peak_temp){
      OCR2B=255; 
      digitalWrite(LED2, HIGH);
    }
    else {
      state = 4;
    }
    Serial.println ("Ramp to Peak - ");
    break;

  case 4: // End (Cooling)
    myservo.write(openOvenDoor);
    OCR2B = 0;
    digitalWrite(LED2, toggle);
    if (temperature > 100){
      Serial.println ("End (Cooling) - ");
    }
    else{
      state = 5;
      Serial.println ("End of Game");
    }
    break;

  case 5:
    break;

  default: 
    state = 5;
  }

  digitalWrite(LED13, toggle);
  toggle = ~toggle; 


  /*
  
   
   Enter final code here
   
   
   */
  while ((millis() - last_time) < MAX6675_time); // wait for MAX6657 temperature measurement
  last_time = millis();
  temperature = read_MAX6657();
  temperature = temperature/4;
}

int read_MAX6657(){
  int temp = 0;
  digitalWrite (CS, LOW); // Initiate MAX6675 read cycle
  digitalWrite (SCLK, HIGH); // Get read of the MSB
  digitalWrite (SCLK, LOW); 
  for (int i = 12; i > 0; i--) { // read the 12 relevant bits
    temp = (temp<<1) | digitalRead(SO);
    digitalWrite (SCLK, HIGH);
    digitalWrite (SCLK, LOW);
  }
  digitalWrite (CS, HIGH); // Discard the 3 LSBs and Initiate new MAX6675 temperature measurement
  return temp;
}








