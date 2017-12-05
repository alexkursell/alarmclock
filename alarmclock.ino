#include <Wire.h>
#include <EEPROM.h>
#include "RTClib.h"
#include "SevSeg.h"

RTC_DS1307 RTC;
SevSeg sevseg;

#define ON 1
#define OFF 0

#define NOSET 0
#define TIMESET 1
#define ALRMSET 2

class Device {
    int _pin;
    int _mode;
    bool _state;
  public:
    Device(int, int);
    void set(bool);
    bool get_digital();
    int get_analog();
    int get_multi_switch();
};

Device::Device(int pin, int mode) {
  _pin = pin;
  _mode = mode;
  _state = false;
  pinMode(_pin, _mode);
}

void Device::set(bool newState) {
  if (_mode == OUTPUT) {
    digitalWrite(_pin, newState);
    _state = newState;
  }
}

bool Device::get_digital() {
  if (_mode == OUTPUT) {
    return _state;
  }
  else {
    return digitalRead(_pin);
  }
}

int Device::get_analog() {
  if (_mode != OUTPUT) {
    return analogRead(_pin);
  }
}

int Device::get_multi_switch() {
  int s1 = 0;
  int s2 = 600;
  int s3 = 900;

  int reading = get_analog();

  s1 = abs(s1 - reading);
  s2 = abs(s2 - reading);
  s3 = abs(s3 - reading);

  if (s2 < s3 && s2 < s1) {
    return 1;
  }
  else if (s3 < s2 && s3 < s1) {
    return 2;
  }
  else {
    return 0;
  }
}

//This function will write a 4 byte (32bit) long to the eeprom at
//the specified address to address + 3.
void EEPROMWritelong(int address, long value)
      {
      //Decomposition from a long to 4 bytes by using bitshift.
      //One = Most significant -> Four = Least significant byte
      byte four = (value & 0xFF);
      byte three = ((value >> 8) & 0xFF);
      byte two = ((value >> 16) & 0xFF);
      byte one = ((value >> 24) & 0xFF);

      //Write the 4 bytes into the eeprom memory.
      EEPROM.write(address, four);
      EEPROM.write(address + 1, three);
      EEPROM.write(address + 2, two);
      EEPROM.write(address + 3, one);
      }

//This function will return a 4 byte (32bit) long from the eeprom
//at the specified address to address + 3.
long EEPROMReadlong(long address)
      {
      //Read the 4 bytes from the eeprom memory.
      long four = EEPROM.read(address);
      long three = EEPROM.read(address + 1);
      long two = EEPROM.read(address + 2);
      long one = EEPROM.read(address + 3);

      //Return the recomposed long by using bitshift.
      return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
      }


Device switchOne(A6, INPUT);
Device switchTwo(A7, INPUT);
Device button(A1, INPUT_PULLUP);

Device relay(A2, OUTPUT);
Device buzzerLow(A3, OUTPUT);
Device buzzerHigh(A0, OUTPUT);

int c = 0;

DateTime alarm(2016, 12, 26, 16, 58, 0);
int buttonFrames = 0;

long alarmStartTime = 0;
bool niceTriggered = false;

byte niceTime = 0;

void setup() {
  Serial.begin(4800);

  //RTC setup
  Wire.begin();
  RTC.begin();
  //RTC.adjust(DateTime(__DATE__, __TIME__));

  byte numDigits = 4;
  byte digitPins[] = {6, 3, 2, 7};
  byte segmentPins[] = {5, 13, 8, 11, 12, 4, 9, 10};
  sevseg.begin(COMMON_CATHODE, numDigits, digitPins, segmentPins);
  sevseg.setNumber(c, 0);

  alarm = DateTime(EEPROMReadlong(0));
  niceTime = (byte)EEPROM.read(5);
  //Serial.println(sizeof(alarm.unixtime()));
}

void loop() {
  // put your main code here, to run repeatedly:
  int state = switchOne.get_multi_switch();
  if(button.get_digital() == ON){
    buttonFrames++;
  }
  else{
    buttonFrames = 0;
  }
  DateTime now = RTC.now();
  
  
  if (state == NOSET) {
    niceTriggered = false;
    sevseg.setNumber(now.hour() * 100 + now.minute(), 2);
    sevseg.refreshDisplay();
  }
  
  else if (state == TIMESET) {
    niceTriggered = false;
    if ((millis() / 100) % 20 > 1) {
      sevseg.setNumber(now.hour() * 100 + now.minute(), 2);
      sevseg.refreshDisplay();
    }
    if (buttonFrames == 1 ||(buttonFrames > 100 && (buttonFrames % 13) == 0)) {
      if (switchTwo.get_multi_switch() == 2) {
        now = DateTime(now.unixtime() + 60);
        if(now.minute() == 0){
          now = DateTime(now.unixtime() - 3600);
        }
        RTC.adjust(now);
        
      }
      else if (switchTwo.get_multi_switch() == 1) {
        RTC.adjust(DateTime(now.unixtime() + 3600));
      }
    }
  }
  
  else if(state == ALRMSET){
    if ((millis() / 100) % 20 > 1) {
      if(!niceTriggered){
        sevseg.setNumber(alarm.hour() * 100 + alarm.minute(), 2);
      }
      else{
        sevseg.setNumber((int)niceTime, 0);
      }
      sevseg.refreshDisplay();
    }

    int whatToSet = switchTwo.get_multi_switch();
    
    if(!niceTriggered && whatToSet == 0 && buttonFrames > 100){
      niceTriggered = true;
      buttonFrames = 0;
    }
    else if(whatToSet != 0){
      niceTriggered = false;
    }
    
    if (buttonFrames == 1 ||(buttonFrames > 100 && (buttonFrames % 13) == 0)) {
      
      if(niceTriggered && whatToSet == 0){
        niceTime = (niceTime + 1) % 61;
        EEPROM.write(5, niceTime);
      }
      else if (whatToSet == 2) {
        alarm = DateTime(alarm.unixtime() + 60);
        if(alarm.minute() == 0){
          alarm = DateTime(alarm.unixtime() - 3600);
        }
        EEPROMWritelong(0, alarm.unixtime());
      }
      else if (whatToSet == 1) {
        alarm = DateTime(alarm.unixtime() + 3600);
        EEPROMWritelong(0, alarm.unixtime());
      }
      
    }
  }

  if(switchTwo.get_multi_switch() == 1 && state == NOSET){
    sevseg.setDigitLight(3, 1);
  }
  else{
    sevseg.setDigitLight(3, 0);
  }

  if(now.hour() == alarm.hour() && now.minute() == alarm.minute() && now.second() == alarm.second() && alarmStartTime == 0 && switchTwo.get_multi_switch() == 1){
    alarmStartTime = now.unixtime();
  }

  if(alarmStartTime != 0 && (buttonFrames != 0 || switchTwo.get_multi_switch() != 1)){
    alarmStartTime = 0;
    buzzerLow.set(OFF);
    buzzerHigh.set(OFF);
    relay.set(OFF);
  }
  if(alarmStartTime == 0){
    if(switchTwo.get_multi_switch() == 2 && switchOne.get_multi_switch() == 0){
      relay.set(ON);
    }
    else{
      relay.set(OFF);
    }
  }
  else if(alarmStartTime + (int)niceTime * 60 > now.unixtime()){
    relay.set(ON);
  }
  else if(alarmStartTime + (int)niceTime * 60 + 300 > now.unixtime()){
    relay.set(ON);
    buzzerLow.set((millis() / 2000) % 2);
  }
  else{
    buzzerLow.set(OFF);
    relay.set((millis() / 2000) % 2);
    buzzerHigh.set((millis() / 500) % 2);
  }
  
  delay(1);
}
