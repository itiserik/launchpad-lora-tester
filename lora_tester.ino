#include "LCD_Launchpad.h"
#include "Sodaq_RN2483.h"
#include "StringLiterals.h"
#include "Utils.h"
#include "Timer.h"                     //http://github.com/JChristensen/Timer

#define debugSerial Serial
#define loraSerial Serial1
  
const uint8_t appEUI[8] = 
{0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x00, 0x01, 0x0F}
;
const uint8_t appKey[16] = 
{0x9B, 0xD5, 0x44, 0x41, 0x60, 0x5F, 0x07, 0x09, 0xAB, 0x5E, 0x43, 0x6F, 0xD3, 0xFE, 0xA0, 0x4D}
;


  
// constants won't change. They're used here to 
// set pin numbers:
const int buttonPin = PUSH2;     // the number of the pushbutton pin
const int ledPin =  RED_LED;      // the number of the LED pin

// Variables will change:
int ledState = LOW;         // the current state of the output pin
int lastButtonState = LOW;   // the previous reading from the input pin

long debouncing_time = 150; //Debouncing Time in Milliseconds
unsigned long last_micros[2];

unsigned char current_item_idx = -1;
unsigned char item_state = 0;

volatile boolean handleSelect = false;
volatile boolean handleMenu = false;

int txinterval = 10;
int autotx = 1;
int8_t timerid = -1;
boolean transparent = false;


LCD_LAUNCHPAD myLCD;
Timer t;                               //instantiate the timer object

#define MODE_NONE 0x00
#define MODE_R 0x01
#define MODE_W 0x02
#define MODE_ONOFF 0x04
#define MODE_RAW 0x08


typedef struct menuitem{
  const char item[7];
  const char text[9];
  int value;
  const int minval;
  const int maxval;
  const int stepsize;
  const unsigned char mode;
  void (*action)(void);
  int (*getter)(void);
  boolean (*setter)(int);
}menuitem_t;

menuitem_t menuitems[] =
{
  { "tx cnf", "",        0, 0, 1, 1,  MODE_NONE,                 TxCnf,0,0}, // 
  { "txucnf", "",        0, 0, 1, 1,  MODE_NONE,                 TxUnCnf, 0, 0}, //
  { "tx int", "",       10, 0, 300, 5,MODE_R|MODE_W,             0, getTxInt ,setTxInt}, //
  { "autotx", "",        1, 0, 1, 1,  MODE_R|MODE_W|MODE_ONOFF,  0, getAutoTx ,setAutoTx}, //
  { "save",   "",        0, 0, 1, 1,  MODE_NONE,                 Save,0,0}, // 
  { "adr",    "adr",     0, 0, 1, 1,  MODE_R|MODE_W|MODE_ONOFF,  0, 0, 0}, //
  { "dr",     "dr",      5, 0, 5, 1,  MODE_R|MODE_W,             0, 0, 0}, // for EU863-870 SF7/125kHZ = 5, SF8=4, SF9=3, SF10=2, SF11=1, SF12=0
  { "pwridx", "pwridx",  1, 1, 5, 1,  MODE_R|MODE_W,             0, 0, 0}, //
  { "retx",   "retx",    7, 0, 7, 1,  MODE_R|MODE_W,             0, 0, 0}, //
  { "ar",     "ar",      0, 0, 1, 1,  MODE_R|MODE_W|MODE_ONOFF,  0, 0, 0}, //
  { "reset",  "reset",   0, 0, 1, 1,  MODE_NONE,                 SwReset,0,0}, // 
  { "hwres",  "hwres",   0, 0, 1, 1,  MODE_NONE,                 HwReset,0,0}, // 
  { "tr19k2", "tr19k2", 0, 0, 1, 1,  MODE_NONE,                 TransParent19k2,0,0}, // 
  { "tr57k6", "tr57k6", 0, 0, 1, 1,  MODE_NONE,                 TransParent57k6,0,0}, // 
  { "band",   "band",    0, 0, 0, 1,  MODE_R,                    0, 0, 0}, //
  { "sync",   "sync",    0, 0, 0, 1,  MODE_R,                    0, 0, 0}, //
  { "mrgn",   "mrgn",    0, 0, 0, 1,  MODE_R,                    0, 0, 0}, //
  { "gwnb",   "gwnb",    0, 0, 0, 1,  MODE_R,                    0, 0, 0}, //
  { "status", "status",  0, 0, 0, 1,  MODE_R|MODE_RAW,           0, 0, 0}, //
  { "upctr",  "upctr",   0, 0, 0, 1,  MODE_R|MODE_RAW,           0, 0, 0}, //
  { "dnctr",  "dnctr",   0, 0, 0, 1,  MODE_R|MODE_RAW,           0, 0, 0}, //
  { "lnchk",  "linchk",  30, 0, 200, 30, MODE_W,                 0, 0, 0}, //
  { "rxdly1", "rxdelay1",0, 0, 0, 1,  MODE_R|MODE_RAW,           0, 0, 0}, //
  { "rxdly2", "rxdelay2",0, 0, 0, 1,  MODE_R|MODE_RAW,           0, 0, 0}, //
  { "rx2868", "rx2 868", 0, 0, 0, 1,  MODE_R|MODE_RAW,           0, 0, 0}, //
  { "dcycls", "dcycleps",0, 0, 0, 1,  MODE_R|MODE_RAW,           0, 0, 0}, //
};

int getTxInt()
{
  return  txinterval;
}

boolean setTxInt(int value)
{
  txinterval = value;
  return true;
}

int getAutoTx()
{
  return autotx;
}

boolean setAutoTx(int value)
{
  autotx = value;
  return true;
}

void TransParent19k2()
{
  debugSerial.begin(19200);

  loraSerial.begin(19200);
  HwResetEx();
  transparent = true;
  myLCD.displayText("tr19k2", 0 , false);
}

void TransParent57k6()
{
  debugSerial.begin(57600);

  loraSerial.begin(57600);
  HwResetEx();
  transparent = true;
  myLCD.displayText("tr57k6", 0 , false);
}

void SwReset() {
  
  bool res1, res2;
  myLCD.showSymbol(LCD_SEG_MARK, 1, true);
  res1 = LoRaBee.resetDevice();
  if (res1)
    debugSerial.println("Reset was successful.");
  else
    debugSerial.println("Reset failed!");
    
  res2 = doOTA();
    
  if (res1 && res2) {
    myLCD.showSymbol(LCD_SEG_RADIO, 1, false);
    myLCD.showSymbol(LCD_SEG_MARK, 0, true);
    myLCD.displayText("ok", 0, false);
    return;
  } else {
    myLCD.showSymbol(LCD_SEG_MARK, 1, false);
    myLCD.displayText("fail", 0, true);
  }
  
  return;
}

void HwReset() {
  
  HwResetEx();
  SwReset();
}

void HwResetEx() {
  
  pinMode(P1_7, OUTPUT);  
  digitalWrite(P1_7, LOW);

  debugSerial.println("Reset RN2483!");
  
  delay(1000);
  pinMode(P1_7, INPUT_PULLUP); 
}

void Save() {
  
  LoRaBee.macSave();
}

void TxCnf() {
  
  uint8_t payload[] = "Hello world!";
  
  myLCD.showSymbol(LCD_SEG_RX, 1, true);
  myLCD.showSymbol(LCD_SEG_TX, 1, false);
  myLCD.displayText("", 0 , false);
  uint8_t result = LoRaBee.macTransmit(STR_CONFIRMED, 1, payload, sizeof(payload));
  
  myLCD.showSymbol(LCD_SEG_RX, 0, true);
  myLCD.showSymbol(LCD_SEG_TX, 0, false);
  
  switch (result)
  {
    case InternalError:
      myLCD.displayText("IntErr", 0 , false);
      myLCD.showSymbol(LCD_SEG_MARK, 1, false);
      break;
    case Busy:
      myLCD.displayText("Busy", 0 , false);
      myLCD.showSymbol(LCD_SEG_MARK, 1, false);
      break;
    case NetworkFatalError:
      myLCD.displayText("NtwErr", 0 , false);
      myLCD.showSymbol(LCD_SEG_MARK, 1, false);
      break;
    case PayloadSizeError:
      myLCD.displayText("PayLen", 0 , false);
      myLCD.showSymbol(LCD_SEG_MARK, 1, false);
      break;
    case NoError:
      LoRaBee.macSave();
      myLCD.displayText("rx ok", 0 , false);
      myLCD.showSymbol(LCD_SEG_MARK, 0, false);
      delay(1000);
      char paramValue[7];
      myLCD.displayText("", 0);
      myLCD.setCursor(0,0);
      LoRaBee.getMacParam("mrgn", &paramValue[0], 7);
      myLCD.print("m");
      myLCD.print(paramValue);
      LoRaBee.getMacParam("gwnb", &paramValue[0], 7);
      myLCD.print("g");
      myLCD.print(paramValue);
      delay(1000);
      myLCD.displayText("", 0);
      myLCD.setCursor(0,0);
      LoRaBee.getMacParam("pwridx", &paramValue[0], 7);
      myLCD.print("p");
      myLCD.print(paramValue);
      LoRaBee.getMacParam("dr", &paramValue[0], 7);
      myLCD.print("d");
      myLCD.print(paramValue);
      break;
    case NoAcknowledgment:
      myLCD.displayText("no rx", 0 , false);
      myLCD.showSymbol(LCD_SEG_MARK, 1, false);
      break;
  }
  
  if (autotx && timerid == -1)
  {
    myLCD.showSymbol(LCD_SEG_CLOCK, 1, true);
    unsigned long interval = txinterval;
    timerid = t.every(interval*1000, TxCnf);
  }
  
  return;
}

void TxUnCnf() {
  
  uint8_t payload[] = "Hello world!";
  myLCD.showSymbol(LCD_SEG_TX, 1, true);
  myLCD.displayText("", 0 , false);
  uint8_t result = LoRaBee.macTransmit(STR_UNCONFIRMED, 1, payload, sizeof(payload));
  myLCD.showSymbol(LCD_SEG_TX, 0, false);
  
  switch (result)
  {
    case InternalError:
      myLCD.displayText("IntErr", 0 , false);
      myLCD.showSymbol(LCD_SEG_MARK, 1, false);
      break;
    case Busy:
      myLCD.displayText("Busy", 0 , false);
      myLCD.showSymbol(LCD_SEG_MARK, 1, false);
      break;
    case NetworkFatalError:
      myLCD.displayText("NtwErr", 0 , false);
      myLCD.showSymbol(LCD_SEG_MARK, 1, false);
      break;
    case PayloadSizeError:
      myLCD.displayText("PayLen", 0 , false);
      myLCD.showSymbol(LCD_SEG_MARK, 1, false);
      break;
    case NoError:
      LoRaBee.macSave();
      myLCD.displayText("tx ok", 0 , false);
      myLCD.showSymbol(LCD_SEG_MARK, 0, false);
      break;
    case NoAcknowledgment:
      myLCD.displayText("no rx", 0 , false);
      myLCD.showSymbol(LCD_SEG_MARK, 1, false);
      break;
  }
  
  if (autotx && timerid == -1)
  {
    myLCD.showSymbol(LCD_SEG_CLOCK, 1, true);
    timerid = t.every(txinterval*1000, TxUnCnf);
  }
      
  
  return;
}

boolean doOTA()
{
  uint8_t deveui[8];
  LoRaBee.getSysParam("hweui", &deveui[0], 8);
  
  char deveui_str[17];
  uint8_t j = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    deveui_str[j++] = static_cast<char>(NIBBLE_TO_HEX_CHAR(HIGH_NIBBLE(deveui[i])));
    deveui_str[j++] = static_cast<char>(NIBBLE_TO_HEX_CHAR(LOW_NIBBLE(deveui[i])));
  }
  deveui_str[16] = '\0';
  myLCD.displayScrollText(deveui_str, 100);
  debugSerial.println(deveui_str);
  
  if (LoRaBee.initOTA(loraSerial, deveui, appEUI, appKey))
  {
    myLCD.showSymbol(LCD_SEG_RADIO, 1, false);
    myLCD.displayText("OTA ok", 0 , false);
    debugSerial.println("OTA ok");
    LoRaBee.setMacParam("rx2 3", "869525000");
  } 
  else
  {
    myLCD.showSymbol(LCD_SEG_MARK, 1, false);
    myLCD.displayText("NJOIN", 0 , false);
    debugSerial.println("Connection to the network failed!");
  }
}
int maxmenuitem = ARRAY_SIZE(menuitems)-1;
void setup() {

  pinMode(RED_LED, OUTPUT);
  digitalWrite(RED_LED, ledState);
  pinMode(GREEN_LED, OUTPUT);
  digitalWrite(GREEN_LED, ledState);
  t.oscillate(RED_LED, 2000, HIGH);
  

  
  debugSerial.begin(57600);
  HwResetEx();
  loraSerial.begin(LoRaBee.getDefaultBaudRate());
  LoRaBee.init(loraSerial);
  myLCD.begin();
  myLCD.displayScrollText("Lora tester", 100);


  
  LoRaBee.setDiag(debugSerial); // optional
  
  doOTA();


  pinMode(PUSH2, INPUT_PULLUP);
  pinMode(PUSH1, INPUT_PULLUP);

  attachInterrupt(PUSH2, selectbuttoninterrupt, FALLING);
  attachInterrupt(PUSH1, menubuttoninterrupt, FALLING);

}


void loop() {
  
  if (transparent)
  {
    while (debugSerial.available()) 
    {
      loraSerial.write((char)debugSerial.read());
    }

    while (loraSerial.available()) 
    {
      debugSerial.write((char)loraSerial.read());
    }  
  }
  
  t.update();
  
  if (timerid >= 0 && (handleMenu || handleSelect)) {
    t.stop(timerid);
    timerid = -1;
    myLCD.showSymbol(LCD_SEG_CLOCK, 0, true);
  }
  
  if (transparent && (handleMenu || handleSelect)) {
    transparent = false;
    debugSerial.begin(57600);
    HwResetEx();
    loraSerial.begin(LoRaBee.getDefaultBaudRate());
  }
  
  if(handleMenu) {
    digitalWrite(GREEN_LED, LOW);
    handleMenu = false;
    current_item_idx++;
    if (current_item_idx > maxmenuitem)
      current_item_idx = 0;
    
    item_state = 0;
    //myLCD.clear();
    myLCD.displayText(menuitems[current_item_idx].item, 0, true);
      
  }
  if(handleSelect) {
    digitalWrite(GREEN_LED, LOW);
    handleSelect = false;
    
    if (current_item_idx < 0 || current_item_idx > maxmenuitem)
      return;
      
    char paramValue[7];
    long int value;
    
    struct menuitem* current_item = &menuitems[current_item_idx];
    
    switch (item_state) {
    case 1:
      if (current_item->action)
      {
        current_item->action();
        current_item_idx = -1;
        item_state = 0;
        break;
      }
      value = current_item->value;
      value+=current_item->stepsize;
      if (value > current_item->maxval)
        value = current_item->minval;
      if (current_item->setter) {
        current_item->setter(value);
      } else {
        if (current_item->mode & MODE_ONOFF)
          LoRaBee.setMacParam(current_item->text, BOOL_TO_ONOFF(value));
        else
          LoRaBee.setMacParam(current_item->text, value);
      }
      current_item->value = value;
      item_state = 0;
      // fall trough
    case 0:
       if (current_item->action) {
        myLCD.displayText(current_item->item, 0, false);
        myLCD.showChar('?', 5, true);
        item_state = 1;
        break;
      }
      if (current_item->mode & MODE_R) {
        if (current_item->getter) {
          value = current_item->getter();
        } else {
          LoRaBee.getMacParam(current_item->text, &paramValue[0], 7);
          if (current_item->mode & MODE_ONOFF)
            value = ONOFF_TO_BOOL(paramValue);
          else
            value = atoi(paramValue);
        }
        
        current_item->value = value;
      }
      
      myLCD.displayText("", 0, false);
      myLCD.setCursor(0,0);
      if (current_item->mode & MODE_ONOFF)
        myLCD.write(BOOL_TO_ONOFF(current_item->value));
      else {
        if (current_item->mode & MODE_RAW)
          myLCD.displayText(paramValue,0,false);
        else
          myLCD.print(current_item->value);
        
      }
      if (current_item->mode & MODE_W)
        item_state = 1;
    }
  }

}

void selectbuttoninterrupt() {
  digitalWrite(GREEN_LED, HIGH);
  if((unsigned long)(millis() - last_micros[0]) >= debouncing_time) {
    handleSelect = true;
    last_micros[0] = millis();
  }
}

void menubuttoninterrupt() {
  digitalWrite(GREEN_LED, HIGH);
  if((unsigned long)(millis() - last_micros[1]) >= debouncing_time) {
    handleMenu = true;
    last_micros[1] = millis();
  }
}


