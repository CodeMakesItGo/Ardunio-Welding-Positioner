//Sample using LiquidCrystal library
#include <LiquidCrystal.h>
#include <EEPROM.h>

#define EEPROM_KEY 0xABCD
#define PUL_OUT   13
#define DIR_OUT   12
#define EN_OUT    11
#define PAUSE_IN  3

const int stepsPerRevolution = 200;  // 1.8 degree step increments

// select the pins used on the LCD panel
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);


// define some values used by the panel and buttons
enum {BTN_RIGHT, BTN_UP, BTN_DOWN, BTN_LEFT, BTN_SELECT, BTN_NONE};
enum {DIS_NONE, DIS_VALUE, DIS_YESNO, DIS_T1F0, DIS_DIR, DIS_POW};
enum {READY, PAUSED, RUN};
enum {SET_RATIO, SET_MICROSTEP, 
      SET_PAUSE, SET_TURN, SET_RPM, SET_DIR,
      SET_VERSION, SET_ABOUT,
      SET_COUNT};


static int lcd_key  = BTN_NONE;
static int lcd_key_last  = BTN_NONE;
static int run_state = READY;
static int last_run_state = RUN;

int button_timer = 0;
int system_timer = 0;
int start_paused_time = 0;
int paused_time = 0;
int adc_key_in  = 0;
int eepromKey = 0;
int togglePulse = LOW;
int startStatePause = LOW;


bool home_display = true;
bool quick_adjust_rpm = true;
int settings_sub_menu = 0;

typedef struct 
{
  int currentValue;
  int previousValue;
  int minValue;
  int maxValue;
  int divider;
  int stepValue;
  int displayType;
  char *topLine;
  char *bottomLine;
}settings_s;

settings_s settings[SET_COUNT];

const char * system_version = "1.0.0";
const char * system_about = "HCW 2018";
bool reset_factory = false;

// read the buttons
int read_LCD_buttons()
{
 adc_key_in = analogRead(0);      // read the value from the sensor 
 if (adc_key_in > 1000) return BTN_NONE; // We make this the 1st option for speed reasons since it will be the most likely result

 if (adc_key_in < 50)   return BTN_RIGHT;  
 if (adc_key_in < 195)  return BTN_UP; 
 if (adc_key_in < 380)  return BTN_DOWN; 
 if (adc_key_in < 555)  return BTN_LEFT; 
 if (adc_key_in < 790)  return BTN_SELECT;   

 return BTN_SELECT;  // when all others fail, return this...
}

void reset_settings()
{ 
                                     // CRNT, PREV, MIN,   MAX, DIV, STP,     Type, "             TOP", " BTM"
  settings[SET_RATIO]     = (settings_s){  10,   0,   1,   100,  10,   1, DIS_VALUE, "Gear Ratio:    ", ":1 "};
  settings[SET_MICROSTEP] = (settings_s){   4,   0,   1,    32,   1,   2,   DIS_POW, "Micro Step:    ", "   "};
  settings[SET_PAUSE]     = (settings_s){1000,   0,   0,  5000,   1, 250, DIS_VALUE, "Pause:         ", "ms "};
  settings[SET_TURN]      = (settings_s){   2,   0,   1,    25,   1,   1, DIS_VALUE, "Rotate:        ", " steps"};
  settings[SET_RPM]       = (settings_s){ 100,   0,  10,  1000, 100,  10, DIS_VALUE, "Speed:         ", " RPM"};
  settings[SET_DIR]       = (settings_s){   1,   0,   0,     1,   1,   1,   DIS_DIR, "Direction:     ", "   "};
  settings[SET_VERSION]   = (settings_s){   0,   0,   0,     0,   1,   0,  DIS_NONE, "Version:       ", system_version};
  settings[SET_ABOUT]     = (settings_s){   0,   0,   0,     0,   0,   0,  DIS_NONE, "About:         ", system_about};
}

void Increase(int item)
{
  if(item >= 0 && item < SET_COUNT)
  {
    settings[item].previousValue = settings[item].currentValue;
    if(settings[settings_sub_menu].displayType == DIS_POW)
    {
      settings[item].currentValue *= settings[item].stepValue;
    }
    else
    {
      settings[item].currentValue += settings[item].stepValue;
    }
    settings[item].currentValue = min(settings[item].maxValue, settings[item].currentValue);
  }
}

void Decrease(int item)
{
  if(item >= 0 && item < SET_COUNT)
  {
    settings[item].previousValue = settings[item].currentValue;
    if(settings[settings_sub_menu].displayType == DIS_POW)
    {
      settings[item].currentValue /= settings[item].stepValue;
    }
    else
    {
      settings[item].currentValue -= settings[item].stepValue;
    }
    settings[item].currentValue = max(settings[item].minValue, settings[item].currentValue);
  }
}

void UpdateDisplay()
{
  String bottomLine;
  lcd.clear();
  
  if(home_display)
  {
    lcd.setCursor(0,0);
    switch(run_state)
    {
      case READY:
      lcd.print("Ready...");
      break;
      
      case PAUSED:
      lcd.print("Paused");
      break;
      
      case RUN:
      lcd.print("Running");
      break;
    }
  
    lcd.setCursor(0,1);
    if(quick_adjust_rpm)
    {
      bottomLine = "Steps:" + String(settings[SET_TURN].currentValue);
    }
    else
    {
      bottomLine = "Pause:" + String((float)settings[SET_PAUSE].currentValue / (float)settings[SET_PAUSE].divider, 1) + "ms";
    }
    lcd.print(bottomLine);
  }
  else
  {
    lcd.setCursor(0,0);
    lcd.print(settings[settings_sub_menu].topLine);

    lcd.setCursor(0,1);
    switch(settings[settings_sub_menu].displayType)
    {
      case DIS_NONE:
      break;
      
      case DIS_VALUE:
      case DIS_POW:
      if(settings[settings_sub_menu].divider > 1)
      {
        bottomLine = String((float)settings[settings_sub_menu].currentValue / (float)settings[settings_sub_menu].divider, 1);
      }
      else
      {
        bottomLine = (String(settings[settings_sub_menu].currentValue));
      }
      break;
      
      case DIS_YESNO:
      (settings[settings_sub_menu].currentValue > 0) ? bottomLine = ("YES") : bottomLine = ("NO");
      break;
      
      case DIS_T1F0:
      (settings[settings_sub_menu].currentValue > 0) ? bottomLine = ("TRUE") : bottomLine = ("FALSE");
      break;
      
      case DIS_DIR:
      (settings[settings_sub_menu].currentValue > 0) ? bottomLine = ("CCW") : bottomLine = ("CW");
      break;
    }
    lcd.print(bottomLine);
    lcd.setCursor(bottomLine.length(), 1);
    lcd.print(settings[settings_sub_menu].bottomLine);
  }
}

bool HandleButton(int button)
{
  bool refresh = true;
  
  if(home_display)
  {
    switch(button)
    {    
      case (BTN_UP):
      if(quick_adjust_rpm) 
      {
         Increase(SET_TURN);
      }
      else
      {
         Increase(SET_PAUSE);
      }
      break;
      
      case (BTN_DOWN):
      if(quick_adjust_rpm) 
      {
         Decrease(SET_TURN);
      }
      else
      {
         Decrease(SET_PAUSE);
      }
      break;
      
      case (BTN_LEFT):
      quick_adjust_rpm = true;
      break;

      case (BTN_RIGHT):
      quick_adjust_rpm = false;
      break;
      
      case (BTN_SELECT):
      home_display = false;
      break;
      
      default:
      refresh = false;
      break;
    }
  }
  else
  {
    switch(button)
    {    
      case (BTN_UP):
      Increase(settings_sub_menu);
      break;
      
      case (BTN_DOWN):
      Decrease(settings_sub_menu);
      break;
      
      case (BTN_LEFT):
      settings_sub_menu--;
      settings_sub_menu = max(0, settings_sub_menu);
      break;

      case (BTN_RIGHT):
      settings_sub_menu++;
      settings_sub_menu = min(SET_COUNT - 1, settings_sub_menu);      
      break;
      
      case (BTN_SELECT):
      home_display = true;
      EEPROM.put(sizeof(eepromKey), settings);
      break;
      
      default:
      refresh = false;
      break;
    }
  }
  return refresh;
}

void StepperMotor()
{
  static unsigned long micropulse = 0;
  static unsigned long micropause = 0;
  static unsigned long microsteps = 0;
  
  unsigned long microseconds = 0;
  unsigned long micronow = micros();

  double dmicroseconds =  (double)((double)stepsPerRevolution * (double)settings[SET_MICROSTEP].currentValue) * 
                          (double)((double)settings[SET_RPM].currentValue / (double)settings[SET_RPM].divider) * 
                          (double)((double)settings[SET_RATIO].currentValue / (double)settings[SET_RATIO].divider) *
                          2.0;
                          
  microseconds = 60L * 1000L * 1000L / (unsigned long)dmicroseconds;
  
  if(micronow - micropulse > microseconds)
  {
    micropulse = micronow;
    togglePulse == LOW ? togglePulse = HIGH : togglePulse = LOW;
    microsteps += (int)togglePulse;
  }

  
  if(microsteps < (settings[SET_TURN].currentValue * settings[SET_MICROSTEP].currentValue))
  {  
    micropause = micronow;
  }
  else
  {
    if(micronow - micropause < (settings[SET_PAUSE].currentValue * 1000L))
    {
      togglePulse = LOW;
    }
    else
    {
      microsteps = 0;
    }
  }

  if(digitalRead(PAUSE_IN) == startStatePause)
  {
    run_state = PAUSED; 
  }
  else
  {
    run_state = RUN; 
  }
   
 
  digitalWrite(PUL_OUT, togglePulse ? HIGH : LOW);
  digitalWrite(DIR_OUT, settings[SET_DIR].currentValue > 0 ? HIGH : LOW);
  digitalWrite(EN_OUT, run_state == PAUSED ? HIGH : LOW);
}


void setup()
{
 lcd.begin(16, 2);              // start the library
 
 EEPROM.get(0, eepromKey);
 
 if(eepromKey != EEPROM_KEY)
 {
  eepromKey = EEPROM_KEY;
  EEPROM.put(0, eepromKey);

  reset_settings();
  EEPROM.put(sizeof(eepromKey), settings);
 }
 
  EEPROM.get(sizeof(eepromKey), settings);

 pinMode(DIR_OUT, OUTPUT);
 pinMode(EN_OUT, OUTPUT);
 pinMode(PUL_OUT, OUTPUT);
 pinMode(PAUSE_IN, INPUT_PULLUP);

 digitalWrite(DIR_OUT, LOW);
 digitalWrite(EN_OUT, HIGH); //Start paused
 digitalWrite(PUL_OUT, LOW);
  
 UpdateDisplay();

 run_state = READY;
 startStatePause = digitalRead(PAUSE_IN);
}

void loop()
{
  StepperMotor();
  
  //make this a 100 millisecond loop
  if((millis()/100) == system_timer) return;
  
  system_timer = millis()/100;
 
  adc_key_in = analogRead(0);
  lcd_key = read_LCD_buttons(); //global  

  if(lcd_key != BTN_NONE)
  {
    button_timer++;
    lcd_key_last = lcd_key;
  }
  else
  { 
    if(HandleButton(lcd_key_last) ||(last_run_state != run_state))
    {
      UpdateDisplay();
      last_run_state = run_state;
    }

    lcd_key_last = lcd_key;
       
    button_timer = 0;
  }
}


