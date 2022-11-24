#include "M5CoreInk.h"
#include "Numbers_55x40.h"
#include <Adafruit_NeoPixel.h>

image_t num55[11] = {
  {40, 55, 1, image_num_01}, {40, 55, 1, image_num_02},
  {40, 55, 1, image_num_03}, {40, 55, 1, image_num_04},
  {40, 55, 1, image_num_05}, {40, 55, 1, image_num_06},
  {40, 55, 1, image_num_07}, {40, 55, 1, image_num_08},
  {40, 55, 1, image_num_09}, {40, 55, 1, image_num_10},
  {24, 55, 1, image_num_11},
};

Ink_Sprite TimePageSprite (&M5.M5Ink);
Ink_Sprite TimeSprite     (&M5.M5Ink);

RTC_TimeTypeDef RTCtime, RTCTimeSave;
RTC_TimeTypeDef AlarmTime;
uint8_t second = 0, minutes = 0;

// state constants:
const int STATE_DEFAULT       = 0;          // DEFAULT CONSTANT
const int STATE_ALARM         = 1;          // ALARM   CONSTANT
const int STATE_REST          = 2;          // REST    CONSTANT

// pin constants:
const int input1Pin           = 26;         // top connector pin G26
const int rgbOutputPin        = 32;         // bottom connector pin G32

// program state:
int program_state = STATE_DEFAULT;

// timers:
unsigned long rtc_timer       = 0;          // real-time clock 
unsigned long alarm_timer     = 0;          // timer for Alarm
unsigned long ambient_timer   = 0;          // timer for Ambient Lighting
unsigned long input_timer     = 0;          // timer for Reading Inputs

// variables for the input pin states:
int input1State               = HIGH;
int armLedVal                 = 0;          // Alarm LED brightness value
int ambLed1Val                = 0;          // Ambient LED brightness value
int ambLed2Val                = 0;          // Ambient LED brightness value
int ambLed3Val                = 0;          // Ambient LED brightness value

bool ambientOn                = true;       // Ambient Lighting ON/OFF
bool alarmMax                 = false;      // LED alarm Max
bool led1Max                  = false;      // LED 1 Max
bool led2Max                  = false;      // LED 2 Max
bool led3Max                  = false;      // LED 3 Max
bool led1Start                = true;       // LED 1 ON/OFF
bool led2Start                = false;      // LED 2 ON/OFF
bool led3Start                = false;      // LED 3 ON/OFF



// pixels object to control the RGB LED Unit:
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(
                             3,                   // number of RGB LEDs
                             rgbOutputPin,        // pin the RGB LED Unit is connected to
                             NEO_GRB + NEO_KHZ800 // LED type
                           );

void setup() {
  pinMode(input1Pin, INPUT_PULLUP);

  M5.begin();                                     // initialize the M5 board
  pixels.begin();                                 // initialize the NeoPixel library
  Serial.begin(9600);                             // open Serial port at 9600 bit per second

  M5.rtc.GetTime(&RTCTimeSave);
  AlarmTime = RTCTimeSave;
  AlarmTime.Minutes = AlarmTime.Minutes + 2;      // set alarm 2 minutes ahead
  M5.update();

  M5.M5Ink.clear();
  delay(500);

  checkRTC();

  TimePageSprite.creatSprite(0, 0, 200, 200);
  drawTimePage();
  
}

void loop() {

  if (Serial.available() > 0)
  {
    // OLD input format is "hh:mm"
    // NEW format:
    // t=hh:mm to set time
    // a=hh:mm to set alarm time
    char input[8];
    int charsRead = Serial.readBytes(input, 8);    // read 8 characters
    if (charsRead == 8 && input[0] == 't' && input[4] == ':') {
      int mm = int(input[6] - '0') + int(input[5] - '0') * 10;
      Serial.println();
      Serial.println("set time..");
      Serial.print("minutes: ");
      Serial.println(mm);
      int hr = int(input[3] - '0') + int(input[2] - '0') * 10;
      Serial.print("hours: ");
      Serial.println(hr);
      RTCtime.Minutes = mm;
      RTCtime.Hours = hr;
      M5.rtc.SetTime(&RTCtime);
    }
    else if(charsRead == 8 && input[0] == 'a' && input[4] == ':') {
      int mm = int(input[6] - '0') + int(input[5] - '0') * 10;
      Serial.println();
      Serial.println("set alarm..");
      Serial.print("minutes: ");
      Serial.println(mm);
      int hr = int(input[3] - '0') + int(input[2] - '0') * 10;
      Serial.print("hours: ");
      Serial.println(hr);
      AlarmTime.Minutes = mm;
      AlarmTime.Hours = hr;
      drawTime(&RTCtime);
      TimePageSprite.pushSprite();
      program_state = STATE_DEFAULT;
      Serial.println("program_state => STATE_DEFAULT");
    }
    else {
      Serial.print("received wrong time format.. ");
      Serial.println(input);
    }
  }

  // =======================================================
  // DEFAULT STATE
  // =======================================================
  if ( program_state == STATE_DEFAULT) {
    // state behavior: check and update time every 100ms
    if (millis() > rtc_timer + 100) {
      updateTime();
      
      if(AlarmTime.Hours == RTCtime.Hours 
        && AlarmTime.Minutes == RTCtime.Minutes) {
        resetLED();
        program_state = STATE_ALARM;
        Serial.println("program_state => STATE_ALARM");
      }
      
      rtc_timer = millis();
    }
    
    // state behavior: update led 1 & 2 & 3 every 20ms
    if(millis() > ambient_timer + 20) {
      if(ambientOn) {
        ambientLED();
      } else {
        resetLED();
      }
      
      ambient_timer = millis();
    }
    
  // =======================================================
  // ALARM STATE == 1
  // =======================================================
  } else if (program_state == STATE_ALARM) {
    // state behavior: check and update time every 100ms
    if (millis() > rtc_timer + 1000) {
      M5.Speaker.setBeep(800,500);
      M5.Speaker.beep();
      rtc_timer = millis();
    }
    
    // LED behavior: Blink in all LED to red color using timer:
    if (millis() > alarm_timer + 10) { // 10 milliseconds passed
      alarmLED();
      alarm_timer = millis();
    }
    
    // process the inputs every 100 milliseconds:
    if (millis() > input_timer + 100) {
      updateTime();
      
      int input1Val = digitalRead(input1Pin);
      
      if(input1Val == LOW) {
        resetLED();
        program_state = STATE_REST;
        Serial.println("program_state => STATE_REST");
      }
      
      input1State = input1Val; // update input 1 state
      input_timer = millis();
    }
    
  // =======================================================
  // REST STATE == 2
  // =======================================================
  } else if (program_state == STATE_REST) {
    if (millis() > rtc_timer + 250) {
      updateTime();
      restLED();
      
      rtc_timer = millis();
    }
  }

  M5.update();
}

void drawImageToSprite(int posX, int posY, image_t *imagePtr,
                       Ink_Sprite *sprite) {
  sprite->drawBuff(posX, posY, imagePtr->width, imagePtr->height,
                   imagePtr->ptr);
}

void updateTime() {
  M5.rtc.GetTime(&RTCtime);
  if (minutes != RTCtime.Minutes) {
    M5.rtc.GetTime(&RTCtime);

    if (RTCtime.Minutes % 10 == 0) {
      M5.M5Ink.clear();
      TimePageSprite.clear(CLEAR_DRAWBUFF | CLEAR_LASTBUFF);
    }
    drawTime(&RTCtime);
    TimePageSprite.pushSprite();
    minutes = RTCtime.Minutes;
  }
}

void drawTime(RTC_TimeTypeDef *time) {
  drawImageToSprite(10, 48, &num55[time->Hours / 10], &TimePageSprite);
  drawImageToSprite(50, 48, &num55[time->Hours % 10], &TimePageSprite);
  drawImageToSprite(90, 48, &num55[10], &TimePageSprite);
  drawImageToSprite(110, 48, &num55[time->Minutes / 10], &TimePageSprite);
  drawImageToSprite(150, 48, &num55[time->Minutes % 10], &TimePageSprite);

  char buf[8];
  int hr_diff = AlarmTime.Hours - RTCtime.Hours;
  if ( hr_diff < 0) {
    hr_diff = 23 + hr_diff;
  }
  int min_diff = AlarmTime.Minutes - RTCtime.Minutes;
  if ( min_diff < 0) {
    min_diff = 59 + min_diff;
    if ( hr_diff == 0) {
      hr_diff = 23 + hr_diff;
    }
  }
  sprintf(buf, "%02d:%02d", hr_diff, min_diff);
  TimePageSprite.drawString(45, 130, buf);
  TimePageSprite.drawString(95, 130, "TO ALARM");
}

void drawTimePage() {
  M5.rtc.GetTime(&RTCtime);
  drawTime(&RTCtime);
  minutes = RTCtime.Minutes;
  TimePageSprite.pushSprite();
}


void checkRTC() {
  M5.rtc.GetTime(&RTCtime);
  if (RTCtime.Seconds == RTCTimeSave.Seconds) {
    Serial.println("RTC Error");
    while (1) {
      delay(10);
      M5.update();
    }
  }
}

// ===========================================================
//                  Personal Functions
// ===========================================================

void ambientLED()
{
  if(led1Start) {
    if (led1Max) {
      ambLed1Val -= 2;
      if (ambLed1Val <= 0) {
        ambLed1Val  = 0;
        led1Max     = false;
        led1Start   = false;
      }
    } else {
      ambLed1Val += 2;
      if (ambLed1Val > 75 && !led2Start)   led2Start  = true;
      if (ambLed1Val > 150)                led1Max    = true;
    }
  }
  
  if(led2Start) {
    if (led2Max) {
      ambLed2Val -= 2;
      if (ambLed2Val <= 0) {
        ambLed2Val  = 0;
        led2Max     = false;
        led2Start   = false;
      }
    } else {
      ambLed2Val += 2;
      if (ambLed2Val > 75 && !led3Start)   led3Start  = true;
      if (ambLed2Val > 150)                led2Max    = true;
    }
  }
  
  if(led3Start) {
    if (led3Max) {
      ambLed3Val -= 2;
      if (ambLed3Val <= 0) {
        ambLed3Val  = 0;
        led3Max     = false;
        led3Start   = false;
        led1Start   = true;
      }
    } else {
      ambLed3Val += 2;
      if (ambLed3Val > 150)               led3Max    = true;
    }
  }
  
  pixels.setPixelColor(0, pixels.Color(0, 0, ambLed1Val));
  pixels.setPixelColor(1, pixels.Color(0, 0, ambLed2Val));
  pixels.setPixelColor(2, pixels.Color(0, 0, ambLed3Val));

  pixels.show();
}

void alarmLED()
{
  if (!alarmMax) {
    armLedVal += 10;    // increase LEDs brightness value
    if (armLedVal > 255) alarmMax = true;
  } else {              // decrease LEDs brightness value
    armLedVal -= 10;
    if (armLedVal <= 0)  alarmMax = false;
  }
      
  pixels.setPixelColor(0, pixels.Color(armLedVal, 0, 0));
  pixels.setPixelColor(1, pixels.Color(armLedVal, 0, 0));
  pixels.setPixelColor(2, pixels.Color(armLedVal, 0, 0));
      
  pixels.show();                  // update RGB LED Unit
}

void restLED()
{
  pixels.setPixelColor(0, pixels.Color(0, 100, 0));
  pixels.setPixelColor(1, pixels.Color(0, 100, 0));
  pixels.setPixelColor(2, pixels.Color(0, 100, 0));
  
  pixels.show();                  // update RGB LED Unit
}

// Reset the LED Colors to None
void resetLED()
{
  pixels.setPixelColor(0, pixels.Color(0, 0, 0));
  pixels.setPixelColor(1, pixels.Color(0, 0, 0));
  pixels.setPixelColor(2, pixels.Color(0, 0, 0));
  
  pixels.show();                  // update RGB LED Unit
}
