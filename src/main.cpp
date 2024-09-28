#include "core/globals.h"
#include "core/main_menu.h"

#include <iostream>
#include <functional>
#include <vector>
#include <string>
#include "esp32-hal-psram.h"

#if defined(ATOMS3)
#include "core/wifi_common.h"
#include "modules/others/webInterface.h"
#endif

MainMenu mainMenu;
SPIClass sdcardSPI;
#if defined(STICK_C_PLUS) || defined(STICK_C_PLUS2)
SPIClass CC_NRF_SPI;
#endif
// Public Globals Variables
unsigned long previousMillis = millis();
int prog_handler;    // 0 - Flash, 1 - LittleFS, 3 - Download
int rotation;
int IrTx;
int IrRx;
int RfTx;
int RfRx;
int RfModule=0;  // 0 - single-pinned, 1 - CC1101+SPI
float RfFreq=433.92;
int RfidModule=M5_RFID2_MODULE;
String cachedPassword="";
String wigleBasicToken="";
int dimmerSet;
int bright=100;
int tmz=3;
int devMode=0;
int soundEnabled=1;
bool interpreter_start = false;
bool sdcardMounted = false;
bool gpsConnected = false;
bool wifiConnected = false;
String wifiIP = "";
bool BLEConnected = false;
bool returnToMenu;
bool isSleeping = false;
bool isScreenOff = false;
bool dimmer = false;
char timeStr[10];
time_t localTime;
struct tm* timeInfo;
#if defined(HAS_RTC)
  cplus_RTC _rtc;
  bool clock_set = true;
#else
  ESP32Time rtc;
  bool clock_set = false;
#endif
JsonDocument settings;

String wui_usr="admin";
String wui_pwd="bruce";
String ssid;
String pwd;
std::vector<Option> options;
const int bufSize = 1024;
uint8_t buff[1024] = {0};
// Protected global variables
#if defined(HAS_SCREEN)
  #if defined(M5STACK) && !defined(CORE2) && !defined(CORE)
  #define tft M5.Lcd
  M5Canvas sprite(&M5.Lcd);
  M5Canvas draw(&M5.Lcd);
  #else
	TFT_eSPI tft = TFT_eSPI();         // Invoke custom library
	TFT_eSprite sprite = TFT_eSprite(&tft);
	TFT_eSprite draw = TFT_eSprite(&tft);
  #endif
#else
    SerialDisplayClass tft;
    SerialDisplayClass& sprite = tft;
    SerialDisplayClass& draw = tft;
#endif

#if defined(CARDPUTER)
  Keyboard_Class Keyboard = Keyboard_Class();
#elif defined (STICK_C_PLUS)
  AXP192 axp192;
#endif

#include "Wire.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/settings.h"
#include "core/serialcmds.h"
#include "core/eeprom.h"
#include "modules/others/audio.h"  // for playAudioFile
#include "modules/rf/rf.h"  // for initCC1101once
#include "modules/bjs_interpreter/interpreter.h" // for JavaScript interpreter

/*********************************************************************
**  Function: setup_gpio
**  Setup GPIO pins
*********************************************************************/
void setup_gpio() {
  #if  defined(STICK_C_PLUS2)
    pinMode(UP_BTN, INPUT);   // Sets the power btn as an INPUT
    pinMode(SEL_BTN, INPUT);
    pinMode(DW_BTN, INPUT);
    pinMode(4, OUTPUT);     // Keeps the Stick alive after take off the USB cable
    digitalWrite(4,HIGH);   // Keeps the Stick alive after take off the USB cable
  #elif defined(STICK_C_PLUS)
    pinMode(SEL_BTN, INPUT);
    pinMode(DW_BTN, INPUT);
    axp192.begin();           // Start the energy management of AXP192
  #elif defined(CARDPUTER)
    Keyboard.begin();
    pinMode(0, INPUT);
    pinMode(10, INPUT);     // Pin that reads the
  #elif ! defined(HAS_SCREEN)
    // do nothing
  #elif defined(M5STACK) // init must be done after tft, to make SDCard work
    //M5.begin();
  #else
    pinMode(UP_BTN, INPUT);   // Sets the power btn as an INPUT
    pinMode(SEL_BTN, INPUT);
    pinMode(DW_BTN, INPUT);
  #endif

  #if defined(BACKLIGHT)
  pinMode(BACKLIGHT, OUTPUT);
  #endif
  //if(RfModule==1)
  initCC1101once(&sdcardSPI); // Sets GPIO in the CC1101 lib
  for(int i = 0; i < 8; i++)
      press_events[i] = 0;
}


/*********************************************************************
**  Function: begin_tft
**  Config tft
*********************************************************************/
void begin_tft(){
#if defined(HAS_SCREEN) && !defined(M5STACK)
  tft.init();
#elif defined(CORE2)
  M5.begin();
  tft.init();
#elif defined(CORE)
  tft.init();
  M5.begin();
#elif defined(M5STACK)
  M5.begin();

#endif
  rotation = gsetRotation();
  tft.setRotation(rotation);
  resetTftDisplay();
}


/*********************************************************************
**  Function: boot_screen
**  Draw boot screen
*********************************************************************/
void boot_screen() {
  tft.setTextColor(FGCOLOR, TFT_BLACK);
  tft.setTextSize(FM);
  tft.drawPixel(0,0,TFT_BLACK);
  tft.drawCentreString("Bruce", WIDTH / 2, 10, SMOOTH_FONT);
  tft.setTextSize(FP);
  tft.drawCentreString(BRUCE_VERSION, WIDTH / 2, 25, SMOOTH_FONT);
  tft.setTextSize(FM);

  tft.drawCentreString("PREDATORY FIRMWARE", WIDTH / 2, HEIGHT+2, SMOOTH_FONT); // will draw outside the screen on non touch devices

  int i = millis();
  char16_t bgcolor = BGCOLOR;
  // checks for boot.jpg in SD and LittleFS for customization
  bool boot_img=false;
  if(SD.exists("/boot.jpg")) boot_img = true;
  else if(LittleFS.exists("/boot.jpg")) boot_img = true;
  // Start image loop
  while(millis()<i+7000) { // boot image lasts for 5 secs
  #if !defined(LITE_VERSION)
    if((millis()-i>2000) && (millis()-i)<2200){ 
      tft.fillRect(0,45,WIDTH,HEIGHT-45,BGCOLOR);
      if(showJpeg(SD,"/boot.jpg") && (millis()-i>2000) && (millis()-i<2200)) { boot_img=true; Serial.println("Image from SD"); }
      else if (showJpeg(LittleFS,"/boot.jpg") && (millis()-i>2000) && (millis()-i<2100)) { boot_img=true; Serial.println("Image from LittleFS"); }
    } 
    if(!boot_img && (millis()-i>2200) && (millis()-i)<2700) tft.drawRect(2*WIDTH/3,HEIGHT/2,2,2,FGCOLOR);
    if(!boot_img && (millis()-i>2700) && (millis()-i)<2900) tft.fillRect(0,45,WIDTH,HEIGHT-45,BGCOLOR);
    #if defined(M5STACK)
      if(!boot_img && (millis()-i>2900) && (millis()-i)<3400) tft.drawXBitmap(2*WIDTH/3 - 30 ,5+HEIGHT/2,bruce_small_bits, bruce_small_width, bruce_small_height,bgcolor,FGCOLOR);
      if(!boot_img && (millis()-i>3400) && (millis()-i)<3600) tft.fillRect(0,0,WIDTH,HEIGHT,BGCOLOR);
      if(!boot_img && (millis()-i>3600)) tft.drawXBitmap((WIDTH-238)/2,(HEIGHT-133)/2,bits, bits_width, bits_height,bgcolor,FGCOLOR);
    #else
      if(!boot_img && (millis()-i>2900) && (millis()-i)<3400) tft.drawXBitmap(2*WIDTH/3 - 30 ,5+HEIGHT/2,bruce_small_bits, bruce_small_width, bruce_small_height,TFT_BLACK,FGCOLOR);
      if(!boot_img && (millis()-i>3400) && (millis()-i)<3600) tft.fillRect(0,0,WIDTH,HEIGHT,BGCOLOR);
      if(!boot_img && (millis()-i>3600)) tft.drawXBitmap((WIDTH-238)/2,(HEIGHT-133)/2,bits, bits_width, bits_height,TFT_BLACK,FGCOLOR);
    #endif
  #endif
    if(checkAnyKeyPress())  // If any key or M5 key is pressed, it'll jump the boot screen
    {
      tft.fillScreen(TFT_BLACK);
      tft.fillScreen(TFT_BLACK);
      delay(10);
      return;
    }
  }

  // Clear splashscreen
  tft.fillScreen(TFT_BLACK);

  // Clear splashscreen
  tft.fillScreen(TFT_BLACK);
}

/*********************************************************************
**  Function: init_clock
**  Clock initialisation for propper display in menu
*********************************************************************/
void init_clock() {
  #if defined(HAS_RTC)
    RTC_TimeTypeDef _time;
    cplus_RTC _rtc;
    _rtc.begin();
    _rtc.GetBm8563Time();
    _rtc.GetTime(&_time);
  #endif
}

/*********************************************************************
**  Function: startup_sound
**  Play sound or tone depending on device hardware
*********************************************************************/
void startup_sound() {
#if !defined(LITE_VERSION)
  #if defined(BUZZ_PIN)
    // Bip M5 just because it can. Does not bip if splashscreen is bypassed
    _tone(5000, 50);
    delay(200);
    _tone(5000, 50);
  /*  2fix: menu infinite loop */
  #elif defined(HAS_NS4168_SPKR)
    // play a boot sound
    if(SD.exists("/boot.wav")) playAudioFile(&SD, "/boot.wav");
    else if(LittleFS.exists("/boot.wav")) playAudioFile(&LittleFS, "/boot.wav");
    setup_gpio(); // temp fix for menu inf. loop
  #endif
#endif
}

float accX = 0;
float accY = 0;
float accZ = 0;

double theta, last_theta = 0;
double phi, last_phi     = 0;
double alpha = 0.2;
unsigned long press_events[8];
int last_event;

/*********************************************************************
**  Function: setup
**  Where the devices are started and variables set
*********************************************************************/
void setup() {
  last_theta = 0;
  last_phi     = 0;
  alpha = 0.2;
 
  Serial.setRxBufferSize(SAFE_STACK_BUFFER_SIZE);  // Must be invoked before Serial.begin(). Default is 256 chars
  Serial.begin(115200);

  log_d("Total heap: %d", ESP.getHeapSize());
  log_d("Free heap: %d", ESP.getFreeHeap());
  if(psramInit()) log_d("PSRAM Started");
  if(psramFound()) log_d("PSRAM Found");
  else log_d("PSRAM Not Found");
  log_d("Total PSRAM: %d", ESP.getPsramSize());
  log_d("Free PSRAM: %d", ESP.getFreePsram());

  // declare variables
  prog_handler=0;
  sdcardMounted=false;
  wifiConnected=false;
  BLEConnected=false;

  setup_gpio();
  begin_tft();
  load_eeprom();
  init_clock();

  if(!LittleFS.begin(true)) { LittleFS.format(), LittleFS.begin();}

  setupSdCard();
  boot_screen();
  getConfigs();

  startup_sound();

  #if ! defined(HAS_SCREEN) || defined(ATOMS3)
    // start a task to handle serial commands while the webui is running
    startSerialCommandsHandlerTask();
  #endif

  delay(200);
  previousMillis = millis();
  last_event = 0;
}

/**********************************************************************
**  Function: loop
**  Main loop
**********************************************************************/

extern double theta, last_theta;
extern double phi, last_phi;
extern double alpha;
int current_event_index;

void print_event_queue() {
    char buff[256] = "", *pos = buff;
    for(int i = 0; i < 8; i++) {
        if(press_events[i]) {
            pos += sprintf(pos, " %lu", press_events[i]);
        }
    }
    log_d("%s", buff);
}

#if defined(HAS_SCREEN)
void loop() {
  #if defined(HAS_RTC)
    RTC_TimeTypeDef _time;
  #endif
  bool redraw = true;
  long clock_update=0;
  mainMenu.begin();

  #if defined(ATOMS3)
    /*
  if(!wifiConnected) {
    Serial.println("wifiConnect");
    wifiConnect("",0,true);  // TODO: read mode from settings file
  }
  Serial.println("startWebUi");
  startWebUi(true);  // MEMO: will quit when checkEscPress
  */
  #endif

  // Interpreter must be ran in the loop() function, otherwise it breaks
  // called by 'stack canary watchpoint triggered (loopTask)'
#if !defined(CORE) && !defined(CORE2)
  if(interpreter_start) {
    interpreter_start=false;
    interpreter();
    previousMillis = millis(); // ensure that will not dim screen when get back to menu
    //goto END;
  }
#endif
  tft.fillRect(0,0,WIDTH,HEIGHT,BGCOLOR);
  getConfigs();


  while(1){
    
    M5.update();
    if(M5.BtnA.isPressed() && !last_event) {
        press_events[current_event_index] = millis();
        last_event = 1;
        current_event_index++;
    }

    if(!M5.BtnA.isPressed() && last_event) {
        press_events[current_event_index] = millis();
        last_event = 0;
        current_event_index++;
    }

    if(current_event_index == 8) {
        for(int i = 0; i < 7; i++) {
            press_events[i] = press_events[i+1];
        }

        current_event_index = 7;
        press_events[current_event_index] = 0;
    }

    print_event_queue();


    auto imu_update = M5.Imu.update();
  
    M5.Imu.getAccelData(&accX, &accY, &accZ);
    // M5.MPU6886.getAccelData(&accX, &accY, &accZ);
    if ((accX < 1) && (accX > -1)) {
        theta = asin(-accX) * 57.295;
    }
    if (accZ != 0) {
        phi = atan(accY / accZ) * 57.295;
    }

    theta = alpha * theta + (1 - alpha) * last_theta;
    phi   = alpha * phi + (1 - alpha) * last_phi;
        
    //log_d("theta:%f phi:%f", theta, phi);

    last_theta = theta;
    last_phi   = phi;



    if(interpreter_start) goto END;
    if (returnToMenu) {
      returnToMenu = false;
      tft.fillScreen(BGCOLOR); //fix any problem with the mainMenu screen when coming back from submenus or functions
      redraw=true;
    }

    if (redraw) {
      mainMenu.draw();
      clock_update=0; // forces clock drawing
      redraw = false;
      delay(200);
    }

    handleSerialCommands();
#ifdef CARDPUTER
    checkShortcutPress();  // shortctus to quickly start apps without navigating the menus
#endif

    if (checkPrevPress()) {
      checkReboot();
      mainMenu.previous();
      redraw = true;
    }
    /* DW Btn to next item */
    if (checkNextPress()) {
      mainMenu.next();
      redraw = true;
    }

    /* Select and run function */
    if (checkSelPress()) {
      mainMenu.openMenuOptions();
      drawMainBorder(true);
      redraw=true;
    }
    


    // update battery and clock once every 30 seconds
    // it was added to avoid delays in btns readings from Core and improves overall performance
    if(millis()-clock_update>30000) {
      drawBatteryStatus();
      if (clock_set) {
        #if defined(HAS_RTC)
          _rtc.GetTime(&_time);
          setTftDisplay(12, 12, FGCOLOR, 1, BGCOLOR);
          snprintf(timeStr, sizeof(timeStr), "%02d:%02d", _time.Hours, _time.Minutes);
          tft.print(timeStr);
        #else
          updateTimeStr(rtc.getTimeStruct());
          setTftDisplay(12, 12, FGCOLOR, 1, BGCOLOR);
          tft.print(timeStr);
        #endif
      }
      else {
        setTftDisplay(12, 12, FGCOLOR, 1, BGCOLOR);
        tft.print("BRUCE " + String(BRUCE_VERSION));
      }
      clock_update=millis();
    }
  }

  
  END:
  delay(1);
}
#else

// alternative loop function for headless boards
#include "core/wifi_common.h"
#include "modules/others/webInterface.h"

void loop() {
  setupSdCard();
  getConfigs();

  if(!wifiConnected) {
    Serial.println("wifiConnect");
    wifiConnect("",0,true);  // TODO: read mode from settings file
  }
  Serial.println("startWebUi");
  startWebUi(true);  // MEMO: will quit when checkEscPress
}
#endif
