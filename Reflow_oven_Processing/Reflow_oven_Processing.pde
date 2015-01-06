//Display reflow-oven temperature
//Created by Zvi Schneider on 04-01-2015
//Last updated 06-01-2015

int lf = 10;    // Linefeed in ASCII
int cr = 13;    // cariage return in ASCII
import processing.serial.*;
Serial port;
String temp_c = "";
String state_time = "";
String tick_count = "";
String data = "";
PFont font;
int x=0;
int y=0;
int i=0;

void setup()
{
  size(900, 600);
  println(Serial.list());
  port = new Serial(this, Serial.list()[2], 19200); // if your arduino COM# is not the 3rd in the list change the parameter in the parantesys.
  port.bufferUntil(cr); 
  font = loadFont("Arial-BoldItalicMT-100.vlw");
  textFont(font, 20);
  background(0, 255, 0);//clear screen at atertup
}

void draw()
{
  fill(0, 255, 0);// eraze the test to the background color
  rect(0, 0, 800, 50);//erazes block around the text line before writing new text
  fill(0, 127, 0); //color for the text
  if (float(tick_count)==0) { // at the beginning of the Arduino run tick_count == 0, 
    background(0, 255, 0); // eraze the screen
    for (i=0; i<=5; i++) { // vertical axis marking
      x=10;
      if (i==0) x=30;
      if (i==1) x=20;
      text(int(i)*50, x, 510-i*50);
    }
    text("[°C]", x, 530-i*50);
    for (i=0; i<=14; i++) { // horizontal axis marking
      x=20;
      if (i==0) x=45;
      if (i==1) x=30;      
      text(int(i)*50, x+i*50, 530);
    }
    text("[Seconds]", 760, 530);
  }

  text(data, 10, 20);// write the line of text at the top of the screen
  point(float(tick_count)/4+50, 500-float(temp_c)/*8*sqrt(float(tick_count)/4)*/); // plot the reflow graph
}

void serialEvent (Serial port)
{
  data = port.readStringUntil(cr); //read the string from the Arduino
  data = data.substring(0, data.length() -1);
  String[] q = splitTokens(data, "; ");//parse the string
  tick_count = q[0];
  temp_c = q[4];
  state_time = q[8];
}

