// Imports ---------------------------------------
#include "SD.h"
#include <HardwareSerial.h>
#include "Arduino.h"
#include "FS.h"
#include "Audio.h"
#include <esp_now.h> 
#include <WiFi.h>
#include <WiFiManager.h> 
#include <PinButton.h> // MultiButton (von Martin Poelstra)
#include <ThreeWire.h>  
#include <RtcDS1302.h>
#include "TickTwo.h"
#include "AiEsp32RotaryEncoder.h"
#include "time.h"



// Pins------------------------------------------
// Rotary_Encoder Pins
#define ROTARY_ENCODER_A_PIN 32
#define ROTARY_ENCODER_B_PIN 14
#define ROTARY_ENCODER_BUTTON_PIN 33
#define ROTARY_ENCODER_VCC_PIN -1
#define ROTARY_ENCODER_STEPS 4

// RTC Pins
#define RTC_DAT_PIN 12 
#define RTC_CLK_PIN 13  
#define RTC_RST_PIN 4

// amplifier Pins
#define I2S_DOUT  25
#define I2S_BCLK  27 
#define I2S_LRC   26

// microSD Card Reader connections
#define SD_CS         5
#define SPI_MOSI      23 
#define SPI_MISO      19
#define SPI_SCK       18

// States ---------------------------------------
#define CHECK_WIFI_STATE 0
#define CHECK_RTC_TIME_STATE 1
#define SELECT_OFFLINE_ONLINE_STATE 2
#define WAITING_FOR_WIFI_STATE 3
#define GET_NTP_TIME_STATE 4
#define ENTER_OFFLINE_TIME_STATE 5
// #define ONLINE_MAINTAIN_TIME_STATE 6 //
// #define OFFLINE_MAINTAIN_TIME_STATE 7 //
#define MAINTAIN_TIME_STATE 7 //
#define ENTER_WAKEUP_TIME_STATE 8
#define SHOW_REMAINING_TIME_UNTIL_WAKEUP_STATE 9
#define WAKEUP_DEACTIVATED_STATE 10 
#define PLAY_ALARM_STATE 11

// Events ---------------------------------------
#define START_EVENT  0
#define WIFI_CONFIG_FOUND_EVENT 1
#define NO_WIFI_CONFIG_FOUND_EVENT 2
#define FOUND_SAFED_TIME_RTC 3
#define FOUND_NO_SAFED_TIME_RTC 4
#define ONLINE_SELECTED_EVENT 5
#define OFFLINE_SELECTED_EVENT 6
#define SUCCESSFULLY_CONNECTED_TO_WIFI_EVENT 7
#define CONNECTION_TO_WIFI_FAILED_EVENT 8
#define CURRENT_TIME_SELECTED_EVENT 9
#define RECEIVED_NTP_TIME_EVENT 11
#define ROTARYENCODER_DOUBLECLICK_EVENT 12
#define ROTARYENCODER_CLICK_EVENT 13
#define ROTARYENCODER_LONGCLICK_EVENT 14
#define WAKEUP_TIME_SELECTED_EVENT 15
#define REACHED_ALARM_TIME_EVENT 16
#define KILLSWITCH_CLICK_EVENT 17

#define MAX_PARTS 10


// global variables -----------------------------------
int event;
int state;
bool onlineMode;
bool alarmActive;
int clickNumber;
double remainingTime;
bool didOnce;
// integers: year, month, hour, minute are only used for setting the current time or alarm time (for displaying or comparing => RtcDateTime objects)
int year;
int month; 
int day;
int hour;
int minute;
bool connectOnce;
int alarmHour;
int alarmMin;
uint8_t receiverAddress[] = {0xCC, 0xDB, 0xA7, 0x56, 0x2E, 0xD8};

// global objects -------------------------------------
// below state machine functions because of Ticker Error: "not declared in this scope"

// State Machine---------------------------------------
/*
 * Set current state depending on the occurred event. 
 */
void updateState(){
  switch(event){
    case START_EVENT:
      break;
    case WIFI_CONFIG_FOUND_EVENT:
      state = GET_NTP_TIME_STATE;
      break;
    case NO_WIFI_CONFIG_FOUND_EVENT:
      state = CHECK_RTC_TIME_STATE;
      break;
    case FOUND_SAFED_TIME_RTC:
      // state = OFFLINE_MAINTAIN_TIME_STATE;
      state = MAINTAIN_TIME_STATE;
      break;   
    case FOUND_NO_SAFED_TIME_RTC:
      state =  SELECT_OFFLINE_ONLINE_STATE;
      break;  
    case ONLINE_SELECTED_EVENT:
      state = WAITING_FOR_WIFI_STATE;
      break;
    case OFFLINE_SELECTED_EVENT:
      state = ENTER_OFFLINE_TIME_STATE;
      break;
    case SUCCESSFULLY_CONNECTED_TO_WIFI_EVENT:
      state = GET_NTP_TIME_STATE;
      break;
    case CONNECTION_TO_WIFI_FAILED_EVENT:
      state = WAITING_FOR_WIFI_STATE;
      break;
    case CURRENT_TIME_SELECTED_EVENT:
      //state = OFFLINE_MAINTAIN_TIME_STATE;
      state = MAINTAIN_TIME_STATE;
      break;
    case RECEIVED_NTP_TIME_EVENT: 
      // state = ONLINE_MAINTAIN_TIME_STATE;
      state = MAINTAIN_TIME_STATE;
      break;
    case ROTARYENCODER_DOUBLECLICK_EVENT:
      state = SELECT_OFFLINE_ONLINE_STATE;
      break;
    case ROTARYENCODER_CLICK_EVENT:
      state = ENTER_WAKEUP_TIME_STATE;
      break;
    case ROTARYENCODER_LONGCLICK_EVENT:
      if(state == WAKEUP_DEACTIVATED_STATE){
        state = SHOW_REMAINING_TIME_UNTIL_WAKEUP_STATE;
      }else{
        state = WAKEUP_DEACTIVATED_STATE;
      }
      break; 
    case WAKEUP_TIME_SELECTED_EVENT:
      state = SHOW_REMAINING_TIME_UNTIL_WAKEUP_STATE;
      break;
    case REACHED_ALARM_TIME_EVENT:
      state = PLAY_ALARM_STATE;
      break;
    case KILLSWITCH_CLICK_EVENT:
      if(state == PLAY_ALARM_STATE){
          state = SHOW_REMAINING_TIME_UNTIL_WAKEUP_STATE;
      }
      break;                
  }
}

/*
 * Check the current state and set the event variable depending on which event has occurred
 * Call the required functionality and output something on the serial monitor/display.
 */
void checkForEvents(){
  switch(state){
    case CHECK_WIFI_STATE:
      if(wifiSettingsFound()){
        event = WIFI_CONFIG_FOUND_EVENT;
      }else{
        event = NO_WIFI_CONFIG_FOUND_EVENT;
      }
      break;
    case CHECK_RTC_TIME_STATE: 
      if(rtcTimeFound()){
        event = FOUND_SAFED_TIME_RTC;
      }else{
        event = FOUND_NO_SAFED_TIME_RTC;
      }
      break;
    case SELECT_OFFLINE_ONLINE_STATE:
      // event is set in rotaryInput() function
      break; 
    case WAITING_FOR_WIFI_STATE:
      if(connectedToWifi()){
        event = SUCCESSFULLY_CONNECTED_TO_WIFI_EVENT;
      }else{
        event = CONNECTION_TO_WIFI_FAILED_EVENT;
      }
      break;
    case GET_NTP_TIME_STATE:
      if(getNTPTime()){
        event = RECEIVED_NTP_TIME_EVENT;
      }else{
        event = CONNECTION_TO_WIFI_FAILED_EVENT;
      }
      break;
    case ENTER_OFFLINE_TIME_STATE:
      // event is set in rotaryInput() function
      break;
    case MAINTAIN_TIME_STATE:
      // event is set in rotaryInput() function
      startMaintainTimeTicker();
      break;
    // case ONLINE_MAINTAIN_TIME_STATE:
    //   // event is set in rotaryInput() function
    //   startMaintainTimeTicker();
    //   break;
    // case OFFLINE_MAINTAIN_TIME_STATE:
    //   // event is set in rotaryInput() function
    //   startMaintainTimeTicker();
    //   break;
    case ENTER_WAKEUP_TIME_STATE: 
      // event is set in rotaryInput() function
      break;
    case SHOW_REMAINING_TIME_UNTIL_WAKEUP_STATE:
      break;
    case WAKEUP_DEACTIVATED_STATE:
      break;
    case PLAY_ALARM_STATE:
   
    Serial.println("Alarm_State");
      playAlarmSound();
      break;       
  }
  updateState();
}


// global Objects (must be declared after the state machine function ) --------------------------------------
RtcDateTime currentTime = RtcDateTime(0, 0, 0, 0, 0, 0);
RtcDateTime alarmTime;
TickTwo stateMachineTicker(checkForEvents, 200);
TickTwo rotaryInputTicker(rotaryInput, 20);
TickTwo maintainTimeTicker(maintainTime, 1000); // every 1 seconds
// TickTwo getNTPTimeTicker(getNTPTime, 600000); // every 10 Minutes
//TickTwo updateScreen(updateScreent, 1000);
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);
ThreeWire myWire(RTC_DAT_PIN, RTC_CLK_PIN, RTC_RST_PIN); // DAT, CLK , RST
RtcDS1302<ThreeWire> rtc(myWire);
PinButton rotaryButton(ROTARY_ENCODER_BUTTON_PIN); // to detect double and long clicks
status_t tickerStatus;
typedef struct message {
    char text[64];
    int intVal;
    // float floatVal;
} message;



message myMessage;

Audio audio;
HardwareSerial SerialPort(2); // use UART2
// const int chipSelect = SS; 
// Rotary Encoder Setup
void IRAM_ATTR readEncoderISR()
{
	rotaryEncoder.readEncoder_ISR();
}

// Setup & Loop ---------------------------------------
void setup() {
  // serial port with baud rate
  Serial.begin(115200);
  SerialPort.begin(115200, SERIAL_8N1, 16, 17);

  // Set microSD Card CS as OUTPUT and set HIGH
  pinMode(SD_CS, OUTPUT);      
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  
  // Start microSD Card
  if(!SD.begin(SD_CS))
  {
    Serial.println("Error talking to SD card!");
    while(true);  // end program
  }
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT); // Connect MAX98357 I2S Amplifier Module


  audio.setVolume(40);
  // initialize global variables
  state = CHECK_WIFI_STATE;
  event = START_EVENT;
  alarmActive = false;
  clickNumber = 0;
  didOnce = false;
  connectOnce = false;
  alarmHour = 0;
  alarmMin = 0;
  // initialize Rotary Encoder
  rotaryEncoder.begin();
  

	rotaryEncoder.setup(readEncoderISR);
  

  // initialize external RTC-Module
  rtc.Begin();
  

  rtc.SetIsWriteProtected(false);
 

  // start Ticker / Threads
  stateMachineTicker.start();
 
  rotaryInputTicker.start();
  // Serial.println("Setup complete");
  printDateTime();
  
}

void loop() {
  // Serial.println("Setup Loop");

  // Ticker / Threads
  stateMachineTicker.update();
  rotaryInputTicker.update();
  maintainTimeTicker.update();
  
  if(state == PLAY_ALARM_STATE){
    audio.loop();
  }

}


// Events: ---------------------------------------
// Functions used to determine the current event

/* 
 * Check if wifi credentials were safed and auto connection is possible.
*/
bool wifiSettingsFound(){
  Serial.println("Try to find Wifi settings.");
  WiFiManager wm;
  if(wm.getWiFiIsSaved()){
    Serial.println("Found Wifi Settings.");
    wm.setConnectTimeout(5);
    wm.setConfigPortalTimeout(5);
    bool result = wm.autoConnect();
    if(!result){
      startESPNOW();
      return false;
    }else{
      onlineMode = true; // set device to onlineMode if saved Wifi Settings led to successfull connection
      return true;
    }
  }else{
    Serial.println("Found no Wifi Settings.");
    // turn off wifi & start espnow;
    startESPNOW();
    return false;
  }
}

/* 
 * Check if time is maintained on external rtc module / if external rtc module is running
 * if not, start the rtc module
*/
bool rtcTimeFound(){
  Serial.println("Try to find RTC-Time");
  if(rtc.GetIsRunning()){
    if(rtc.IsDateTimeValid()){
      Serial.println("RTC is running: ");
      currentTime =  rtc.GetDateTime(); 
      printDateTime();
      
      //return false;
      onlineMode = false;
      return true;
    }
  }else{
    rtc.SetIsRunning(true); 
    return false; 
  }
  return false;
}

/*
 * Start an Access Point with the WifiManager library for WiFi Setup with a Web-Interface
 */
bool connectedToWifi(){
  Serial.println("Waiting for Wifi Connection.");
  Serial.println("Please use your Phone or Desktop to connect to the 'Dayrise_Alarm' network and follow the instructions.");  
  WiFiManager wm;
  wm.setTitle("Dayrise Alarm");
  Serial.println("Dayrise AP started.");
  bool result = wm.autoConnect("Dayrise-Alarm", "mydayrisedevice"); // ssid, password
  if(!result){
    Serial.println("Wifi connection failed.");
    return false;
  }

  Serial.println("WiFi connection established.");
  return true;
  
}

/*
 * Get current time from ntp server and set it on the rtc module.
 */
bool getNTPTime(){
  Serial.println("Try to get the current time from an NTP server.");

  const char* ntpServer = "pool.ntp.org"; // public ntp server
  const long gmtOffset_sec = 3600; // gmt offset (Offset Greenwich Mean Time Zone) - DE 1h
  const int daylightOffset_sec = 3600; // offset of summer time: 1h

  struct tm timeinfo; // datastructure for time

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); // get current time from ntp

  // check wifi status and try to safe time data in tm struct
  if(WiFi.status() != WL_CONNECTED || !getLocalTime(&timeinfo)){
    Serial.println("Failed to get NTP time");
    return false;
  }
  int currentYear = timeinfo.tm_year + 1900; // tm_year = years since 1900
  setCurrentTime(currentYear, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  currentTime = rtc.GetDateTime();
  Serial.print("Successfully set the time with NTP: ");
  Serial.println(&timeinfo, "%A, %d %B %Y %H:%M:%S");
  Serial.println("------------------------------------------------------------------------------------------------------------------------------------");

  // turn off wifi & start espnow
  startESPNOW();

  return true;
}

// Functionalities: ------------------------------------

/*
 * Handle the received message from an ESP-NOW transmitter.
 * Copy the incoming data into the 'myMessage' variable using memcpy.
 * Print the MAC address of the transmitter.
 * If the received message has an integer value of 0 and the system is in the PLAY_ALARM_STATE,
 * set the 'event' variable to KILLSWITCH_CLICK_EVENT, update the alarm time for the next day,
 * reset the 'connectOnce' flag, and print the current date and time.
 */
void messageReceived(const uint8_t* macAddr, const uint8_t* incomingData, int len){
    memcpy(&myMessage, incomingData, sizeof(myMessage));
    Serial.printf("Transmitter MAC Address: %02X:%02X:%02X:%02X:%02X:%02X \n\r", 
            macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);    

    if(myMessage.intVal == 0 && state == PLAY_ALARM_STATE){
      event = KILLSWITCH_CLICK_EVENT;
      setAlarmTime(alarmTime.Year(), alarmTime.Month(), alarmTime.Day()+1, alarmTime.Hour(), alarmTime.Minute(), alarmTime.Second());
      Serial.println("Killswiith Nachricht");
      connectOnce = false;
      printDateTime();
    }
}


/*
 * Start the ESP-NOW communication.
 * Disconnect from the current Wi-Fi network, turn off the Wi-Fi mode,
 * and then enable the Wi-Fi station mode.
 * Initialize ESP-NOW and register the messageReceived callback function.
 */
void startESPNOW(){
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() == ESP_OK) {
        Serial.println("ESPNow Init success");
    }
    else {
        Serial.println("ESPNow Init fail");
        return;
    }
    esp_now_register_recv_cb(messageReceived);
}

/* 
 * Set Time on the external RTC-Module.
 */
void setCurrentTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second){
  rtc.SetDateTime(RtcDateTime(year, month, day, hour, minute, second));
}

/*
 * Create a alarmTime Object (as RtcDateTime for comparison with the current RTC Time).
 */
void setAlarmTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second){
  alarmTime = RtcDateTime(year, month, day, hour, minute, second);
}

/*
 * Format Time and Date and print it to the Serial Monitor.
 */
void printDateTime(){

  // date
  Serial.print("Aktuelles Datum: ");
  char dateString[11];
  snprintf_P(dateString, sizeof(dateString), PSTR("%02u.%02u.%04u"), currentTime.Day(), currentTime.Month(), currentTime.Year());  // snprintf_P => format String 
  Serial.println(dateString);

  // time
  Serial.print("Aktuelle Uhrzeit: ");
  char timeString[6];
  snprintf_P(timeString, sizeof(timeString), PSTR("%02u:%02u"), currentTime.Hour(), currentTime.Minute());  // snprintf_P => format String 
  Serial.println(timeString);
  String remainingTimeString = " ";
  if(alarmActive){
    remainingTimeString = calculateRemainingTime();
  }else{
    remainingTimeString = "-";
  }
  Serial.println("0|" + String(timeString) + "|" + remainingTimeString);
  SerialPort.println("0|" + String(timeString) + "|" + remainingTimeString + "");

}

/*
 * Get the current time from the RTC-Module and print it to the Serial Monitor. 
 * Online Mode: Continuously refresh the time on the RTC-Module with the time data from the NTP Server.
 * Offline Mode: recognize Rotary Encoder Double Click to change Mode and reset Time
 */
void maintainTime(){
  if(currentTime.Minute() != rtc.GetDateTime().Minute() && state != ENTER_WAKEUP_TIME_STATE) {
    currentTime =  rtc.GetDateTime(); 
    Serial.println(" ");
    
    printDateTime();

  if(alarmActive){
    if(currentTime >= alarmTime){
      event = REACHED_ALARM_TIME_EVENT;
    }

  }
  }
}


 /*
 * Calculate the remaining time between the current time (currentTime) and the alarm time (alarmTime).
 * The resulting remaining time is returned as a string in the format "HH:MM" using the createRemainingTimeString() function.
 * If the current time is later than or equal to the alarm time, return a dash ("-") to indicate no remaining time.
 */
String calculateRemainingTime(){
  Serial.print("remaining time: ");
  // Calculate time difference from seconds since 1/1/2000 and convert back to minutes.
  remainingTime = (alarmTime.TotalSeconds()-currentTime.TotalSeconds())/60; 
  // convert remaining time to format: hh:mm
  double remainingHour = remainingTime/60;
  int remainingHourInt = (int)remainingHour;
  double remainingDecimalMin = remainingHour - remainingHourInt;
  double remainingFormattedMin = remainingDecimalMin * 60;
  int remainingFormattedMinInt = (int)remainingFormattedMin;
  if(currentTime <= alarmTime){
    return createRemainingTimeString(remainingHourInt, remainingFormattedMinInt );
  }else{
    Serial.println("-");
    String noRemainingTime = "-";
    return noRemainingTime;
  }

  return "-";
}

/*
 * Create a string representation of the remaining time in the format "HH:MM" based on the provided 'remainingHour' and 'remainingMin'.
 * The resulting string is stored in the variable 'remainingTimeString' and returned.
 */
String createRemainingTimeString( int remainingHour, int remainingMin){
  String remainingHourString = String(remainingHour);
  String remainingMinString = String(remainingMin);
  String remainingTimeString = " ";
  if(remainingHour < 10 && remainingMin >= 10){
    Serial.println("0" + remainingHourString + ":" + remainingMinString);
    remainingTimeString = "0" + remainingHourString + ":" + remainingMinString;
    return remainingTimeString;
  }
  if(remainingHour < 10 && remainingMin < 10){
    Serial.println("0" + remainingHourString + ":0" + remainingMinString);
    remainingTimeString = "0" + remainingHourString + ":0" + remainingMinString;
    return remainingTimeString;
  }
  if(remainingHour >= 10 && remainingMin < 10){
    Serial.println(remainingHourString + ":0" + remainingMinString);
    remainingTimeString = remainingHourString + ":0" + remainingMinString;
    return remainingTimeString;
  }
  if(remainingHour >= 10 && remainingMin >= 10){
    Serial.println(remainingHourString + ":" + remainingMinString);
    remainingTimeString = remainingHourString + ":" + remainingMinString;
    return remainingTimeString;
  }
  return " ";
}


/*
 * Play the alarm sound if the alarm is active.
 * If the 'connectOnce' flag is false, establish a connection to the file system (SD) and load the sound file ("/sound.mp3").
 * Set the 'connectOnce' flag to true after establishing the connection.
 */
void playAlarmSound(){
  if(alarmActive){
    if(!connectOnce){
      audio.connecttoFS(SD, "/sound.mp3");

    }
  connectOnce = true;
  } 

}

/*
 * Reads different input values of the rotary encoder depending on the state. 
 * Changes event depending on input and state.
 */
void rotaryInput(){
  // choose between offline and online mode
  if(state == SELECT_OFFLINE_ONLINE_STATE){
    if(!didOnce){
      Serial.println("Please choose Offline or Online Mode by rotating the selection wheel.");
      Serial.println("Confirm the correct mode by pressing the selection wheel.");
      Serial.println("Mode: Online");
      rotaryEncoder.setBoundaries(0, 1, true); //minValue, maxValue, circleValues true|false
      rotaryEncoder.setEncoderValue(0);
      rotaryEncoder.disableAcceleration();
    }
    didOnce = true;

    if(rotaryEncoder.encoderChanged()){
      if(rotaryEncoder.readEncoder() == 0){
        Serial.println("Mode: Online");
      }else{
        Serial.println("Mode: Offline");
      }
    }
    if(rotaryEncoder.isEncoderButtonClicked()){
      if(rotaryEncoder.readEncoder() == 0){
        event = ONLINE_SELECTED_EVENT;
        Serial.println("Selected Mode: Online !");
      }else{
        event = OFFLINE_SELECTED_EVENT;
        Serial.println("Selected Mode: Offline !");
      }
      didOnce = false;
    }
  } 

  // enter time with the rotary encoder for offline time and alarm time
  if(state == ENTER_OFFLINE_TIME_STATE || state == ENTER_WAKEUP_TIME_STATE){

    // skip date configuration when setting the alarm
    if(state == ENTER_WAKEUP_TIME_STATE && clickNumber == 0){
      clickNumber = 3;
    }
    
    switch(clickNumber){
      // select year
      case 0: 
        if(state == ENTER_OFFLINE_TIME_STATE){
          // Allows one time output to the console, inside a continuously called Ticker / Thread. (Variable didOnce need to be set to false after calling the function to get expected behavior)
          if(!didOnce){
            Serial.println("Please select the current year, month, day, hour and minute by rotating the selection wheel.");
            Serial.println("Confirm the correct time/date information by pressing the selection wheel.");
            rotaryEncoder.setBoundaries(0, 9999, true);
            rotaryEncoder.setEncoderValue(2023);
          }
          didOnce = true;
        }
        if(rotaryEncoder.encoderChanged()){
           Serial.print("Year: ");
           Serial.println(rotaryEncoder.readEncoder());
        }
        if(rotaryEncoder.isEncoderButtonClicked()){
          year = rotaryEncoder.readEncoder();
          Serial.print("Selected Year: ");
          Serial.println(year);
          clickNumber = clickNumber + 1;
          didOnce = false;
        }
        break;
      // select month
      case 1:
        if(!didOnce){
          rotaryEncoder.setBoundaries(1, 12, true);
          rotaryEncoder.setEncoderValue(1);
        }
        didOnce = true;
        if(rotaryEncoder.encoderChanged()){
           Serial.print("Month: ");
           Serial.println(rotaryEncoder.readEncoder());
        }
        if(rotaryEncoder.isEncoderButtonClicked()){
          month = rotaryEncoder.readEncoder();
          Serial.print("Selected Month: ");
          Serial.println(month);
          clickNumber = clickNumber + 1;
          didOnce = false;
        }
        break;
      // select day 
      case 2:
        if(!didOnce){
          rotaryEncoder.setBoundaries(1, 31, true);
          rotaryEncoder.setEncoderValue(1);
        }
        didOnce = true;
        if(rotaryEncoder.encoderChanged()){
            Serial.print("Day: ");
            Serial.println(rotaryEncoder.readEncoder());
        }
        if(rotaryEncoder.isEncoderButtonClicked()){
          day = rotaryEncoder.readEncoder();
          Serial.print("Selected Day: ");
          Serial.println(day);
          clickNumber = clickNumber + 1;
          didOnce = false;
        }
        break;
      // select hour 
      case 3:
        if(state == ENTER_WAKEUP_TIME_STATE){
          // Allows one time output to the console, inside a continuously called Ticker / Thread. (Variable didOnce need to be set to false after calling the function to get expected behavior)
          if(!didOnce){
            Serial.println("Please select the alarm time (hour and minute) by rotating the selection wheel.");
            Serial.println("Confirm the correct time information by pressing the selection wheel.");
            printAlarmTime(false, alarmMin, 0);
          }
        } 
        if(!didOnce){
          rotaryEncoder.setBoundaries(0, 23, true);
          // rotaryEncoder.setEncoderValue(0);
          rotaryEncoder.setEncoderValue(alarmHour);
        }
        didOnce = true;
        if(rotaryEncoder.encoderChanged()){
            Serial.print("Hour: ");
            Serial.println(rotaryEncoder.readEncoder());
            alarmHour = rotaryEncoder.readEncoder();
            printAlarmTime(true, rotaryEncoder.readEncoder(), 0);
        }
        if(rotaryEncoder.isEncoderButtonClicked()){
          hour = rotaryEncoder.readEncoder();
          Serial.print("Selected Hour: ");
          Serial.println(hour);
          clickNumber = clickNumber + 1;
          didOnce = false;
        }
        break;
      // select min
      case 4:
        if(!didOnce){
          rotaryEncoder.setBoundaries(0, 59, true);
          rotaryEncoder.setEncoderValue(alarmMin);
        }
        didOnce = true;
        if(rotaryEncoder.encoderChanged()){
            Serial.print("Min: ");
            Serial.println(rotaryEncoder.readEncoder());
            alarmMin = rotaryEncoder.readEncoder();
            printAlarmTime(false, rotaryEncoder.readEncoder(), 0);
        }
        if(rotaryEncoder.isEncoderButtonClicked()){
          minute = rotaryEncoder.readEncoder();
          Serial.print("Selected Minute: ");
          Serial.println(minute);
          clickNumber = clickNumber + 1;
        }
        break;

        
    }

    // time setup complete: reset clicknumber, set RtcDateTime Objects depending on the state, set event
    if(clickNumber > 4){
      clickNumber = 0;
      if(state == ENTER_OFFLINE_TIME_STATE){
        setCurrentTime(year, month, day, hour, minute, 0);
        Serial.println("Successfully set current time.");
        event = CURRENT_TIME_SELECTED_EVENT;
        currentTime =  rtc.GetDateTime(); 
        printDateTime();
      }
      if(state == ENTER_WAKEUP_TIME_STATE){
        RtcDateTime tempAlarm = RtcDateTime(currentTime.Year(), currentTime.Month(), currentTime.Day(), hour, minute, 0);
        if(currentTime > tempAlarm){
          setAlarmTime(currentTime.Year(), currentTime.Month(), currentTime.Day()+1, hour, minute, 0);
        }else{
          setAlarmTime(currentTime.Year(), currentTime.Month(), currentTime.Day(), hour, minute, 0);
        }
        alarmActive = true;
        printAlarmTime(false, alarmMin, 1);
        printDateTime();
        Serial.println("Successfully set alarm time.");
        event = WAKEUP_TIME_SELECTED_EVENT; 
      }
      didOnce = false;
    }

  }

 

  // react to rotary encoder clicks while maintaining the time  
  if(state == MAINTAIN_TIME_STATE || state == SHOW_REMAINING_TIME_UNTIL_WAKEUP_STATE || state == WAKEUP_DEACTIVATED_STATE){
    // Button object (.update() to detect Double Click and Long Click)
    rotaryButton.update();

    if(rotaryButton.isDoubleClick()){
      Serial.println("Double Click");
      event = ROTARYENCODER_DOUBLECLICK_EVENT;
      maintainTimeTicker.pause();
    }

    if(rotaryButton.isLongClick() && state != MAINTAIN_TIME_STATE ){
      Serial.println("Long Click");
      alarmActive = !alarmActive;
      event = ROTARYENCODER_LONGCLICK_EVENT;
      if(alarmActive){
          Serial.println("Alarm active");
      }else{
        Serial.println("Alarm inactive");
      }
      printDateTime();
    }

    if(rotaryButton.isSingleClick()){
      Serial.println("Single Click");
      event = ROTARYENCODER_CLICK_EVENT;
    }

  }


}

/* 
 * Start maintainTimeTicker if not already running. 
*/ 
void startMaintainTimeTicker(){
    if(maintainTimeTicker.state() != RUNNING){
     maintainTimeTicker.resume();
  }
}

/* 
 * Print the alarm time in the format "HH:MM" based on the 'hour' flag and the provided 'number' value.
 * Also, construct a string 'alarmString' in the format "1|HH:MM|status" to represent the alarm time and status.
 */
 void printAlarmTime(bool hour, int number, int status){
   String alarmString = "";
      if(hour){
        if(number < 10 && alarmMin >= 10){
        Serial.println("0" + String(number) + ":" + String(alarmMin));
        alarmString = "1|0" + String(number) + ":" + String(alarmMin) + "|" + status;
        }
        if(number < 10 && alarmMin < 10){
          Serial.println("0" +  String(number) + ":0" + String(alarmMin));
          alarmString = "1|0" + String(number) + ":0" + String(alarmMin) + "|" + status;
        }
        if(number >= 10 && alarmMin < 10){
          Serial.println( String(number) + ":0" + String(alarmMin));
          alarmString = "1|" + String(number) + ":0" + String(alarmMin) + "|" + status;
        }
        if(number >= 10 && alarmMin >= 10){
          Serial.println( String(number) + ":" + String(alarmMin));
          alarmString = "1|" + String(number) + ":" + String(alarmMin) + "|" + status;
        }
      }else{
          if(alarmHour < 10 && number >= 10){
          Serial.println("0" + String(alarmHour) + ":" +  String(number));
          alarmString = "1|0" + String(alarmHour) + ":" + String(number) + "|" + status;
        }
        if(alarmHour < 10 && number < 10){
          Serial.println("0" + String(alarmHour) + ":0" +  String(number));
          alarmString = "1|0" + String(alarmHour) + ":0" + String(number) + "|" + status;
        }
        if(alarmHour >= 10 && number < 10){
          Serial.println(String(alarmHour) + ":0" +  String(number));
          alarmString = "1|" + String(alarmHour) + ":0" + String(number) + "|" + status;
        }
        if(alarmHour >= 10 && number >= 10){
          Serial.println(String(alarmHour) + ":" + String(number));
          alarmString = "1|" + String(alarmHour) + ":" + String(number) + "|" + status;
        }
        
      }
        Serial.println(alarmString);
        SerialPort.println(alarmString + "");
        
        
  }

