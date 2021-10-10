//ESPboy IceFishing smart rod
//module for www.espboy.com project
//by RomanS 03.10.2021

#include <Servo.h>
#include <ESP_EEPROM.h>
#include "lib/ESPboyInit.h"
#include "lib/ESPboyInit.cpp"

#define PAD_LEFT        0x01
#define PAD_UP          0x02
#define PAD_DOWN        0x04
#define PAD_RIGHT       0x08
#define PAD_ACT         0x10
#define PAD_ESC         0x20
#define PAD_LFT         0x40
#define PAD_RGT         0x80

#define PAD_HOOK         0x8000
#define PAD_LEFT2        0x2000
#define PAD_RIGHT2       0x4000
#define PAD_ACT2         0x1000


//hardware settings
#define SERVO_ATTACH_PIN D8
#define SERVO_ATTACH_START 800
#define SERVO_ATTACH_END 2700
#define SERVO_ARM_LENGTH 34 //servo arm length between axis
#define DISPLAY_FPS 10
#define LED_LIGHT 10
#define DELAY_LOOP 20
#define SAVE_MARK 0xFAFD
#define LONG_PRESS_BUTTON_DELAY 1000


//MAX-MIN, ETC parameters
#define PAUSE_BEFORE_STARTING_FISH_DETECTION_SEC 1
#define MINIMAL_TIME_FOR_FISH_DETECTION_SEC 2
#define MAX_AMPLITUDE_PERCENT 100
#define MIN_AMPLITUDE_PERCENT 10
#define MAX_FISH_DETECTION_PAUSE_SEC 60
#define MIN_FISH_DETECTION_PAUSE_SEC PAUSE_BEFORE_STARTING_FISH_DETECTION_SEC + MINIMAL_TIME_FOR_FISH_DETECTION_SEC
#define MAX_OSCILATIONS_FREQ_PER_MINUTE 240
#define MIN_OSCILATIONS_FREQ_PER_MINUTE 20
#define MAX_OSCILATIONS_TO_PAUSE 30
#define MIN_OSCILATIONS_TO_PAUSE 1
#define MAX_WAY_PER_MINUTE_MM SERVO_ARM_LENGTH*60*4

//DEFAULT SETTINGS
#define DEFAULT_AMPLITUDE_PERCENT 10
#define DEFAULT_ROD_OSCILATIONS_FREQUENCE_MIN 100 //Hz per minute 
#define DEFAULT_OSCILATIONS_TO_PAUSE 5
#define DEFAULT_FISH_DETECTION_TIME_SEC 3


ESPboyInit myESPboy;
Servo servo;


enum FishingModes{
  MODE_STANDBY,
  MODE_FISHING,
  MODE_DETECTING,
  MODE_HOOK,
  MODE_SETTINGS
};


enum FishDetectionResult{
  WAIT_DETECTION,
  DETECTING,
  DETECTED,
  NOT_DETECTED
}fishResut;


struct SaveStruct{
  uint32_t savemark = SAVE_MARK;
  uint16_t amplitudeSetting;
  uint16_t oscilationsRodFrequenceSetting;
  uint16_t oscilationsToPauseSetting;
  uint16_t fishDetectionTimeSetting;
} saveData;


//struct fishing settings
struct FishingRodSettingsStruct{
  uint32_t millisDisplayFPS = 0;
  uint32_t millisStartFishDetection = 0;
  uint8_t UIupdateFlag = 1;
  FishingModes currentFishingMode;
  float currentAngle = 0;
  uint16_t previousAngle = 0;
  float currentAngleStep = 0;
  uint16_t amplitudeSetting = DEFAULT_AMPLITUDE_PERCENT;
  uint16_t oscilationsRodFrequenceSetting = DEFAULT_ROD_OSCILATIONS_FREQUENCE_MIN;
  uint16_t oscilationsToPauseSetting = DEFAULT_OSCILATIONS_TO_PAUSE;
  uint16_t fishDetectionTimeSetting = DEFAULT_FISH_DETECTION_TIME_SEC;
  uint16_t oscilationCount = 0;
  bool oscilationCountZeroFlag = false;
}fishingRodSettings;



uint16_t getKeys() { return ((~myESPboy.mcp.readGPIOAB()) & 0x7039); }
bool getHook() { return (((~myESPboy.mcp.readGPIOAB()) & PAD_HOOK)?1:0); }


void toneUp(){
  myESPboy.playTone(200,100);
  delay(100);
  myESPboy.playTone(500,200);
  delay(200);
}


void toneDown(){
  myESPboy.playTone(500,100);
  delay(100);
  myESPboy.playTone(200,200);
  delay(200);
}


void toneHoocked(){
  myESPboy.playTone(200,200);
  delay(200);
  myESPboy.playTone(500,200);
  delay(200);
  myESPboy.playTone(800,500);
  delay(500);
}


void oscilationStep(){
  uint16_t calc;
    if (fishingRodSettings.currentAngle == 0 && fishingRodSettings.oscilationCountZeroFlag == false){
      fishingRodSettings.oscilationCountZeroFlag = true;
      fishingRodSettings.oscilationCount++;}
    
    if (fishingRodSettings.currentAngle != 0) fishingRodSettings.oscilationCountZeroFlag = false;
    
    fishingRodSettings.currentAngleStep = fishingRodSettings.oscilationsRodFrequenceSetting*DELAY_LOOP/130.0;
    fishingRodSettings.currentAngle += fishingRodSettings.currentAngleStep;
    if (fishingRodSettings.currentAngle >= 360) fishingRodSettings.currentAngle = 0;
    if (fishingRodSettings.previousAngle != floor(fishingRodSettings.currentAngle)){
      calc = (fishingRodSettings.amplitudeSetting/2 * (1 + (cos (radians(fishingRodSettings.currentAngle + 180)))));
      //Serial.println(calc);
      servo.write(calc); 
      myESPboy.myLED.setRGB(0, calc, 0);
      myESPboy.tft.drawFastVLine(126, 0, 127, TFT_BLACK);
      myESPboy.tft.drawFastVLine(127, 0, 127, TFT_BLACK);
      myESPboy.tft.drawFastVLine(126, 0, calc, TFT_WHITE);
      myESPboy.tft.drawFastVLine(127, 0, calc, TFT_WHITE);
      fishingRodSettings.previousAngle = floor(fishingRodSettings.currentAngle);
    } 
};



FishDetectionResult detectingFish(){
  if (fishingRodSettings.millisStartFishDetection == 0) {
    fishingRodSettings.millisStartFishDetection = millis();}

  if((millis() - fishingRodSettings.millisDisplayFPS) > 1000/DISPLAY_FPS){
       fishingRodSettings.millisDisplayFPS = millis();
       myESPboy.tft.drawFastHLine(0, 127, 128, TFT_BLACK);
       myESPboy.tft.drawFastHLine(0, 126, 128, TFT_BLACK);
       uint8_t len;
       len = (128.0/(float)fishingRodSettings.fishDetectionTimeSetting) * (float)(fishingRodSettings.fishDetectionTimeSetting-((millis()-fishingRodSettings.millisStartFishDetection)/1000));
       myESPboy.tft.drawFastHLine(0, 127, len, TFT_WHITE);
       myESPboy.tft.drawFastHLine(0, 126, len, TFT_WHITE);}
  
  if (millis () - fishingRodSettings.millisStartFishDetection < PAUSE_BEFORE_STARTING_FISH_DETECTION_SEC * 1000) 
    return WAIT_DETECTION;

  if (getHook()){
    myESPboy.myLED.setRGB(LED_LIGHT, 0, 0);
    toneUp();
    fishingRodSettings.millisStartFishDetection = 0;
    return(DETECTED);  
  }

  if(millis () - fishingRodSettings.millisStartFishDetection < (fishingRodSettings.fishDetectionTimeSetting) * 1000) 
    return (DETECTING);
    
  fishingRodSettings.millisStartFishDetection = 0;
  return(NOT_DETECTED);
};



void startFishing(){
  fishingRodSettings.oscilationCount = 0;
  fishingRodSettings.oscilationCountZeroFlag = false;
  fishingRodSettings.currentAngle = 0;
  fishingRodSettings.millisStartFishDetection = 0;
  fishingRodSettings.UIupdateFlag++;
  servo.attach(SERVO_ATTACH_PIN, SERVO_ATTACH_START, SERVO_ATTACH_END, SERVO_ATTACH_START);
}


void stopFishing(){
  servo.write(0);
  delay(500);
  myESPboy.myLED.setRGB(0,0,0);
  fishingRodSettings.UIupdateFlag++;
  servo.detach();
}


void hookedStandby(){
  servo.attach(SERVO_ATTACH_PIN, SERVO_ATTACH_START, SERVO_ATTACH_END, SERVO_ATTACH_START);
  myESPboy.myLED.setRGB(0,0,0);
  fishingRodSettings.UIupdateFlag++;
  fishingRodSettings.currentFishingMode = MODE_STANDBY;
  for(uint8_t i=0; i<90; i++){
     servo.write(i);
     delay(20);}
  toneHoocked();
  delay(500);
  servo.detach();
}


void checkKeysStartStop(){
  static uint16_t readingMCP;
  static uint32_t pressedButtonTimer;
  
  readingMCP = getKeys();
  if (!readingMCP) return;

  fishingRodSettings.UIupdateFlag++;
  
  pressedButtonTimer = millis();
  while (getKeys() && millis() - pressedButtonTimer < LONG_PRESS_BUTTON_DELAY) delay(100);
  
  if (millis() - pressedButtonTimer > LONG_PRESS_BUTTON_DELAY ) {
    fishingRodSettings.currentFishingMode = MODE_SETTINGS;
    myESPboy.myLED.setRGB(LED_LIGHT, LED_LIGHT, 0);
    drawUI(1);
    toneUp();
    while(getKeys()) delay(100);
  }
  else{
    if (fishingRodSettings.currentFishingMode == MODE_STANDBY){ 
      fishingRodSettings.currentFishingMode = MODE_FISHING;
      myESPboy.myLED.setRGB(0,0,0);
      drawUI(0);
      toneUp();
      startFishing();}
    else  {
      fishingRodSettings.currentFishingMode = MODE_STANDBY;
      myESPboy.myLED.setRGB(0,0,0);
      drawUI(0);
      toneDown();
      stopFishing();}
    while(getKeys()) delay(100);
  }
}



void drawUI(uint8_t menuItemNo){
  if (millis() - fishingRodSettings.millisDisplayFPS > 1000/DISPLAY_FPS){
    fishingRodSettings.millisDisplayFPS = millis();
    fishingRodSettings.UIupdateFlag = 0;
    myESPboy.tft.fillScreen(TFT_BLACK);

    myESPboy.tft.setTextColor(TFT_GREEN, TFT_BLACK);
    myESPboy.tft.setTextSize(2);

    if (menuItemNo == 1) myESPboy.tft.setTextColor(TFT_RED, TFT_BLACK);
    else myESPboy.tft.setTextColor(TFT_GREEN, TFT_BLACK);
    myESPboy.tft.drawString("Amp:", 0, 0);
    myESPboy.tft.drawString((String)fishingRodSettings.amplitudeSetting, 90, 0);

    if (menuItemNo == 2) myESPboy.tft.setTextColor(TFT_RED, TFT_BLACK);
    else myESPboy.tft.setTextColor(TFT_GREEN, TFT_BLACK);
    myESPboy.tft.drawString("Frq:", 0, 26);
    myESPboy.tft.drawString((String)fishingRodSettings.oscilationsRodFrequenceSetting, 90, 26);

    if (menuItemNo == 3) myESPboy.tft.setTextColor(TFT_RED, TFT_BLACK);
    else myESPboy.tft.setTextColor(TFT_GREEN, TFT_BLACK);
    myESPboy.tft.drawString("Cnt:", 0, 52);
    myESPboy.tft.drawString((String)fishingRodSettings.oscilationsToPauseSetting, 90, 52);

    if (menuItemNo == 4) myESPboy.tft.setTextColor(TFT_RED, TFT_BLACK);
    else myESPboy.tft.setTextColor(TFT_GREEN, TFT_BLACK);
    myESPboy.tft.drawString("Pau:", 0, 78);
    myESPboy.tft.drawString((String)fishingRodSettings.fishDetectionTimeSetting, 90, 78);
    
    String toPrint;

    myESPboy.tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    myESPboy.tft.setTextSize(1);

    myESPboy.tft.drawString("Amplitude %", 0, 16);
    myESPboy.tft.drawString("Oscilations per min", 0, 42);
    myESPboy.tft.drawString("Oscilations count", 0, 68);
    myESPboy.tft.drawString("Pause after oscil.", 0, 94);
    
    switch (fishingRodSettings.currentFishingMode){
      case MODE_STANDBY:
         toPrint = "Standby";
         break;
      case MODE_FISHING:
         toPrint = "Fishing";
         break;
      case MODE_DETECTING:
         toPrint = "Detecting";
         break;
      case MODE_HOOK:
         toPrint = "Hook!";
         break;
      case MODE_SETTINGS:
         toPrint = "Settings";
         break;
    }

    myESPboy.tft.setTextSize(2);
    myESPboy.tft.setTextColor(TFT_WHITE, TFT_BLACK);
    myESPboy.tft.drawString(toPrint, (128-toPrint.length()*12)/2, 107);
  }
}



void loadDataFunc(){
  EEPROM.get(5, saveData);
  if (saveData.savemark != SAVE_MARK){
    saveDataFunc();
  }
  else{
    fishingRodSettings.amplitudeSetting = saveData.amplitudeSetting;
    fishingRodSettings.oscilationsRodFrequenceSetting = saveData.oscilationsRodFrequenceSetting;
    fishingRodSettings.oscilationsToPauseSetting = saveData.oscilationsToPauseSetting;
    fishingRodSettings.fishDetectionTimeSetting = saveData.fishDetectionTimeSetting;
  }
}



void saveDataFunc(){
  saveData.savemark = SAVE_MARK;
  saveData.amplitudeSetting = fishingRodSettings.amplitudeSetting;
  saveData.oscilationsRodFrequenceSetting = fishingRodSettings.oscilationsRodFrequenceSetting;
  saveData.oscilationsToPauseSetting = fishingRodSettings.oscilationsToPauseSetting;
  saveData.fishDetectionTimeSetting = fishingRodSettings.fishDetectionTimeSetting;
  EEPROM.put(5, saveData);
  EEPROM.commit();
}



void setup() {
  Serial.begin(115200);
  EEPROM.begin(sizeof(saveData)+10);
  myESPboy.begin("Ice Fishing"); //Init ESPboy
  
  for (int i=12;i<16;i++){  
    myESPboy.mcp.pinMode(i, INPUT);
    myESPboy.mcp.pullUp(i, HIGH);}

  loadDataFunc();
  startFishing();
  delay(500);
  stopFishing();
}



void loop(){
  static FishDetectionResult fdr;
  uint8_t menuPosition;
  int8_t parameter;
  uint16_t readingMCP;
  
  switch(fishingRodSettings.currentFishingMode){
    
    case MODE_STANDBY:
      if (myESPboy.myLED.getB() != LED_LIGHT || myESPboy.myLED.getR() || myESPboy.myLED.getG()) myESPboy.myLED.setRGB(0, 0, LED_LIGHT);
      if (getHook()){
        myESPboy.myLED.setRGB(LED_LIGHT, 0, 0);
        toneUp();}
      break;
      
    case MODE_FISHING:
      oscilationStep();
      if (fishingRodSettings.oscilationCount > fishingRodSettings.oscilationsToPauseSetting && fishingRodSettings.currentAngle==0) {

       Serial.println();
       Serial.println(fishingRodSettings.oscilationCount);
       Serial.println(fishingRodSettings.oscilationCountZeroFlag);
       Serial.println(fishingRodSettings.currentAngle);
       Serial.println();
        
        stopFishing();
        fishingRodSettings.currentFishingMode = MODE_DETECTING;}
      break;
      
    case MODE_DETECTING:    
      fdr = detectingFish();
      
      if (fdr == DETECTED){
        startFishing();
        fishingRodSettings.currentFishingMode = MODE_HOOK; 
        myESPboy.myLED.setRGB(LED_LIGHT, LED_LIGHT, LED_LIGHT);
        servo.write(90);
        delay(1500);
        for(uint8_t i=90; i>1; i--){
          servo.write(i);
          delay(40);}
        servo.detach();
      }
      if (fdr == NOT_DETECTED){
        startFishing();
        fishingRodSettings.currentFishingMode = MODE_FISHING;}
      break;
      
    case MODE_HOOK:   
      fdr = detectingFish();
      
      if (fdr == DETECTED){
        hookedStandby();}
      if (fdr == NOT_DETECTED){
        fishingRodSettings.currentFishingMode = MODE_FISHING;
        startFishing();}
      break;  
   
   case MODE_SETTINGS:
     menuPosition = 1;
     drawUI(1);
     while (!((readingMCP = getKeys())&PAD_ESC) && menuPosition < 5){
       parameter = 0;
       if(readingMCP) fishingRodSettings.UIupdateFlag++;
       if((readingMCP & PAD_RIGHT) || (readingMCP & PAD_RIGHT2)) {parameter=1; myESPboy.playTone(40,40);}
       if((readingMCP & PAD_LEFT) || (readingMCP & PAD_LEFT2)) {parameter=-1; myESPboy.playTone(40,40);}
       if((readingMCP & PAD_ACT) || (readingMCP & PAD_ACT2)) {
         menuPosition++; 
         myESPboy.playTone(100,100); 
         while(getKeys()) delay(100);}
       
       switch(menuPosition){
        case 1:
          if (fishingRodSettings.amplitudeSetting < MAX_AMPLITUDE_PERCENT && parameter>0)
            fishingRodSettings.amplitudeSetting +=5;
          if (fishingRodSettings.amplitudeSetting > MIN_AMPLITUDE_PERCENT && parameter<0)
            fishingRodSettings.amplitudeSetting -=5;
          break;
        case 2:
          if (fishingRodSettings.oscilationsRodFrequenceSetting < MAX_OSCILATIONS_FREQ_PER_MINUTE && parameter>0)
            fishingRodSettings.oscilationsRodFrequenceSetting += 10;      
          if (fishingRodSettings.oscilationsRodFrequenceSetting > MIN_OSCILATIONS_FREQ_PER_MINUTE && parameter<0)
            fishingRodSettings.oscilationsRodFrequenceSetting -= 10;      
          break;
        case 3:
         if (fishingRodSettings.oscilationsToPauseSetting < MAX_OSCILATIONS_TO_PAUSE && parameter>0)
           fishingRodSettings.oscilationsToPauseSetting += 1;
         if (fishingRodSettings.oscilationsToPauseSetting > MIN_OSCILATIONS_TO_PAUSE && parameter<0)
           fishingRodSettings.oscilationsToPauseSetting -= 1;
         break;
        case 4:
         if (fishingRodSettings.fishDetectionTimeSetting < MAX_FISH_DETECTION_PAUSE_SEC && parameter>0)
           fishingRodSettings.fishDetectionTimeSetting += 1;
         if (fishingRodSettings.fishDetectionTimeSetting > MIN_FISH_DETECTION_PAUSE_SEC && parameter<0)
           fishingRodSettings.fishDetectionTimeSetting -= 1;
         break;
     }

     if(fishingRodSettings.UIupdateFlag) drawUI(menuPosition);
     delay(100);
    }
    
   fishingRodSettings.UIupdateFlag++;
   fishingRodSettings.currentFishingMode = MODE_STANDBY;
   toneDown();
   myESPboy.myLED.setRGB(0,0,0);
   saveDataFunc();
   break;
  } 
   
  checkKeysStartStop();
  if(fishingRodSettings.UIupdateFlag)drawUI(0);
  delay(DELAY_LOOP);
}
