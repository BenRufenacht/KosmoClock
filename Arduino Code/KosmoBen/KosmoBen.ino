/* 
 *  KosmoBen
 *  
 *  A master clock for Kosmo systems
 *  By: Ben Rufenacht
 *  Adapted From: Quinie.nl
 *  
 */

// Includes needed for running this code.
// TimerOne will handle the timing of the pulses

#include <TimerOne.h>  
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BasicEncoder.h>
#include <Bounce2.h>

#include "LittleBenOutputs.h"

int const maxLBOutput = 8;
// Load the outputs as class
LittleBenOutput LBOutputs[maxLBOutput] = {
  LittleBenOutput(0),
  LittleBenOutput(1),
  LittleBenOutput(2),
  LittleBenOutput(3),
  LittleBenOutput(4),
  LittleBenOutput(5),
  LittleBenOutput(6),
  LittleBenOutput(7)
};

byte selectedLBOutput = 0;

//Clock Settings
byte statusClock = 0; // 0 = stopped, 1 = playing, 2 = pause
byte clockSource = 1; // 0 = internal, 1 = external

long intervalMicroSeconds;  // variable for interval for timer running clock pulses calculted on BPM value

// These are used to output the clock on the 595 chip
// The bit are linked to the ouput as 2,4,6,8,1,3,5,7 <-- note: double check note sure
byte outputClockBits = B00000000; 

float bpm = 126;  // BPM value 
byte ppqn = 24; // Pulse per beat

#define startTriggerPin 2 // Start trigger pin
#define stopTriggerPin 3 // Stop trigger pin

// 595 output pins 
byte outputLatchPin = 5; 
byte outputClockPin = 6; 
byte outputDataPin = 4;  

// Rotary Encoder Inputs pins
#define inputCLK 9
#define inputDT 8
#define inputSW 7

BasicEncoder enc(inputCLK, inputDT);
Bounce2::Button SW = Bounce2::Button();

// Current en previous states of rotary pins
byte currentStateSW; 
byte previousStateSW;
byte currentStateEnc;
byte previousStateEnc; 

// Current menu item
byte menuItem = 0;

//Oled display settings
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64
#define OLED_RESET     4 
#define SCREEN_ADDRESS 0x3C

int name;

bool updateScreen = false;
bool saving = false;
long savingtime;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {

  // Set timer for internal clock
  Timer1.initialize(intervalMicroSeconds);
  Timer1.setPeriod(calculateIntervalMicroSecs());
  Timer1.attachInterrupt(ClockPulseInternal);
  
  attachInterrupt(digitalPinToInterrupt(stopTriggerPin), stopTrigger, FALLING );
  attachInterrupt(digitalPinToInterrupt(startTriggerPin), pauseTrigger, FALLING );

  // Set all the pins of 74HC595 as OUTPUT
  pinMode(outputLatchPin, OUTPUT);
  pinMode(outputDataPin, OUTPUT);  
  pinMode(outputClockPin, OUTPUT);

  // Rotary inputs
  SW.attach(inputSW, INPUT_PULLUP);
  SW.interval(10); 
  SW.setPressedState(LOW);
  
  // set state of rotary
  SW.update();
  previousStateSW = SW.pressed();
  previousStateEnc = enc.get_count();

  // Set Defaults
  SetDefaults();

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    //Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  display.clearDisplay(); 
  //display.dim(true);

  display.setTextSize(2);       
  display.setTextColor(SSD1306_WHITE);

  printScreenDefault();

}

void loop() {
  CheckRotarySwitch(); // Check Rotary Switch
  CheckRotary(); // Check Rotary 

  // Check is screen needs update
  if(updateScreen) { 
    // if menu is not 4 it is not the default menu
    if(menuItem <= 2) {
      printScreenDefault();
    } 
    else {
      printScreenOutputs();
    }
  updateScreen = false;
  }

  if(saving) { 
    long currentMillis = millis();
    if((currentMillis - savingtime) >= 2000) {
      saving = false;
    }
  }
}

void SetDefaults() {
  LBOutputs[6].SetclockDivider(3);
  LBOutputs[5].SetclockDivider(6);
  LBOutputs[4].SetclockDivider(12);
  LBOutputs[3].SetType(1);
  LBOutputs[2].SetType(1);
  LBOutputs[2].SetbeatCountDivider(2);
  LBOutputs[1].SetType(1);
  LBOutputs[1].SetbeatCountDivider(4);
  LBOutputs[0].SetType(1);
  LBOutputs[0].SetbeatCountDivider(8);
}

// Check if rotary switch has been pushed
// Set the menuState
void CheckRotarySwitch() {
   SW.update();
   currentStateSW = SW.pressed();
   if (currentStateSW != previousStateSW && currentStateSW == 0) {
      if(menuItem == 5) {
        menuItem = 0;
      } else {
        menuItem++;
      }
      updateScreen = true;
   }
   previousStateSW = currentStateSW; 
}

// Check if rotary is turned
// Update menu item
void CheckRotary() {
   enc.service();
   int changed = enc.get_change();
   if (changed){  
     currentStateEnc = enc.get_count(); 
    switch(menuItem) {
      case 0:     
        UpdateBPM();
        break;
      case 1:
        UpdateBPMDecimal();
        break;
      case 2:
        UpdateClockSource();
        break;
      case 3:
        UpdateOutputSelector();
        break;
      case 4:
        UpdateOutputType();         
        break;
      case 5:
        UpdateOutputTypeValue();         
        break;
     }
   } 
   previousStateEnc = currentStateEnc;  
}

// Rotary updates BPM 
void UpdateBPM() {
  if(clockSource == 0) { // only update if clock is internal
    if (currentStateEnc > previousStateEnc)
    {
      bpm++;
    }
    else
    {
      bpm--;
    }
    updateTimer();
    updateScreen = true;
  }
}

// Updates BPM Decimal
void UpdateBPMDecimal() {
  if(clockSource == 0) { // only update if clock is internal
    if (currentStateEnc > previousStateEnc)
    {
      bpm += 0.1;
    }
    else
    {
      bpm -= 0.1;
    }
    updateTimer();
    updateScreen = true;
  }
}

// Update the type of clock
void UpdateClockSource() {
   clockSource = !clockSource;
   updateScreen = true;
}

void UpdateOutputSelector() {
  int tmp = selectedLBOutput; // integer temp value for calulations and suppressing bufferoverflow of bytes
   if (currentStateEnc > previousStateEnc) { 
     tmp++; 
   } else {
     tmp--;
   }
   
   if(tmp < 0) {
      selectedLBOutput = 7;
   } else if(tmp >= 8) {
      selectedLBOutput = 0;
   } else {
      selectedLBOutput = tmp;
   }
   updateScreen = true;
}

void UpdateOutputType() {
  if (currentStateEnc > previousStateEnc) { 
     LBOutputs[selectedLBOutput].SetType(1); // add 1
   } else {
     LBOutputs[selectedLBOutput].SetType(-1);
   }
   updateScreen = true;
}

void UpdateOutputTypeValue() {
  if (currentStateEnc > previousStateEnc) { 
     LBOutputs[selectedLBOutput].SetTypeValue(1); // add 1
   } else {
     LBOutputs[selectedLBOutput].SetTypeValue(-1);
   }
   updateScreen = true;
}

void UpdateSettings() {
  saving = true;
  updateScreen = true;
  savingtime = millis();
}

// Update the timer
void updateTimer() {
  Timer1.setPeriod(calculateIntervalMicroSecs());
}

// Clock comes from internal
void ClockPulseInternal() {
  // Check Clock for play state and internal
  if((statusClock == 1) && (clockSource == 0) ){ 
    ClockPulse();
  }
}

// Clock pulse comes from outside
void ClockPulseOuter() {
  ClockPulse();
}

void ClockPulse() {
  // run thru outputs
  for (int i = 0; i < maxLBOutput; i++) {
    LBOutputs[i].Pulse(ppqn); // Call pulse function on outputs
    bitWrite(outputClockBits, i, LBOutputs[i].GetOutputBit()); // set the output byte based on output settings
  }
  
  //output
  outputClock(outputClockBits);
  delay(4);
  outputClock(B00000000);
}


// Output the clock
void outputClock(byte pins)
{
   digitalWrite(outputLatchPin, LOW);
   shiftOut(outputDataPin, outputClockPin, LSBFIRST, pins);
   digitalWrite(outputLatchPin, HIGH);
}

// Stop the clock
void stopTrigger() {
  if(clockSource == 0){ 
    statusClock = 0;
    updateScreen = true;
  }
  // Loop thru outputs
  for (int i = 0; i < maxLBOutput; i++) {
    LBOutputs[i].resetCounters(); // Reset Counters
  }
}

// Input pulse on pause button/jack
// Pause the clock or other function depending on the clock source setting of Littleben
void pauseTrigger() {
  if(clockSource == 0){ 
    togglePauseOrPlay();
  }
  if(clockSource == 1){ 
    ClockPulseOuter();
  }
}

void togglePauseOrPlay(){
  if(statusClock == 1){ 
    statusClock = 2;
  } else {
    statusClock = 1;
  } 
  updateScreen = true;
}

long calculateIntervalMicroSecs() {
  return 60L * 1000 * 1000 / bpm / ppqn;
}

//Prints the clock status to the screen
void printScreenStatus() {
    display.setCursor(40,2);    
    display.setTextSize(2);   
    if (statusClock == 0) {
      display.print("STOP");
    }  
    else if (statusClock == 1) {
      display.print("PLAY");
    }
    else {
      display.print("PAUSE");
    }

    display.setCursor(14, 46);    
    display.setTextSize(2); 
    if (clockSource == 0) {
      display.print("internal");
    }  
    else {
      display.print("external");
    }
}

// prints the BPM to the screen
void printScreenBPM() {
    display.setCursor(4, 24);
    display.setTextSize(2);  
    // Internal BPM
    if(clockSource == 0){ 
      display.print("BPM ");
      if(bpm<100){
          display.print(F(" "));
      } 
      display.print(bpm);
    }
    if(clockSource == 1){ 
      display.print("---.--");
    }
}

void printScreenMenuSelection() {
    switch(menuItem) {
      case 0: //Clock
        display.drawFastHLine(44, 42, 50, WHITE); // BPM
        break;
      case 1:
        display.drawFastHLine(90, 42, 38, WHITE); // BPM Decimal
        break;
      case 2:
        display.drawFastHLine(12, 62, 100, WHITE); // ClockSource
        break;
      case 3:
        display.drawFastHLine(2, 32, 20, WHITE); // OutputName, Output selector
        break;
      case 4:
        display.drawFastHLine(30, 18, 70, WHITE); // OutputType, Clock, Beat, Random
        break;
      case 5:
        display.drawFastHLine(2, 44, 100, WHITE); // OutputType Value
        break;
    }
}

// Print the display 
void printScreenDefault() {
    display.clearDisplay();
  
    printScreenStatus();
    printScreenBPM();
    printScreenMenuSelection();

    display.display();
}

void printScreenOutputs() {
    display.clearDisplay(); 
    printScreenOutputName();
    printScreenOutputType();
    printScreenOutputTypeValue();
    printScreenMenuSelection();
    display.display();
}

void printScreenOutputName() {
    display.setCursor(2, 2);    
    display.setTextSize(4);      
    display.print(LBOutputs[selectedLBOutput].GetName()); 
}

void printScreenOutputType() {
    display.setCursor(30, 2);    
    display.setTextSize(2);      
    display.print(LBOutputs[selectedLBOutput].GetTypeName()); 
}

void printScreenOutputTypeValue() {
    display.setCursor(2, 35);    
    display.setTextSize(1);      
    display.print(LBOutputs[selectedLBOutput].GetTypeValueText()); 
}

