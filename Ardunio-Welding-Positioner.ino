//Sample using LiquidCrystal library
#include <LiquidCrystal.h>
#include <EEPROM.h>

#define USING_PAUSE_SWITCH (0) // Set to 1 if using an external switch (PAUSE_IN) to pause the stepper motor

#define EEPROM_KEY 0xABCD // Change this if you modify any of the menus to refresh the EEPROM
#define PUL_OUT   13      // Pulse output
#define DIR_OUT   12      // Direction output
#define EN_OUT    11      // Enable output
#define KEY_IN    0       // Analog input from buttons on display
#define PAUSE_IN  3       // Foot switch to start or stop the rotation

const int stepsPerRevolution = 200;  // 1.8 degree step increments
const unsigned long microPulses = 60000000 / stepsPerRevolution; // micropulses for RPM calculation 
const unsigned int TopLineLen = 16;  // 16 character max for top line
const unsigned int BtmLineLen = 8;   // 8 character max for bottom line
const unsigned int ButtonDebounce = 5; // 50ms debounce timer when used with 100hz timer

// select the pins used on the LCD panel
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// panel and buttons
enum {BTN_RIGHT, 
      BTN_UP, 
      BTN_DOWN, 
      BTN_LEFT, 
      BTN_SELECT, 
      BTN_NONE};

// Display types
enum {DIS_NONE, 
      DIS_VALUE, 
      DIS_YESNO, 
      DIS_T1F0, 
      DIS_DIR, 
      DIS_POW};

// Run state
enum {READY, 
      PAUSED, 
      RUN};

// Menus
enum {SET_RATIO, 
      SET_MICROSTEP, 
      SET_PAUSE, 
      SET_TURN, 
      SET_RPM, 
      SET_DIR,
      SET_VERSION, 
      SET_ABOUT,
      SET_COUNT};


static int lcd_key  = BTN_NONE;
static int lcd_key_last  = BTN_NONE;
static int last_run_state = PAUSED;
volatile int togglePulse = LOW;

#if(USING_PAUSE_SWITCH == 1)
  int startStatePause = LOW;
  static int run_state = READY;
#else
  static int run_state = RUN;
#endif

bool home_display = true;
bool quick_adjust_rpm = true;
int settings_sub_menu = 0;

typedef struct 
{
  unsigned int startKey;
  int currentValue;
  int previousValue;
  int minValue;
  int maxValue;
  int divider;
  int stepValue;
  int displayType;
  char topLine[TopLineLen];
  char bottomLine[BtmLineLen];
  unsigned int endKey;
}settings_s;

settings_s settings[SET_COUNT];


// read the buttons
int read_LCD_buttons()
{
  int adc_key_in  = 0;
  adc_key_in = analogRead(KEY_IN);        // read the value from the sensor

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
                                                 // CRNT, PREV, MIN,   MAX, DIV, STP,     Type, "            TOP", " BTM"
  settings[SET_RATIO]     = (settings_s){EEPROM_KEY,  10,   0,   1,   100,  10,   1, DIS_VALUE, "Gear Ratio:    ", ":1     ", EEPROM_KEY};
  settings[SET_MICROSTEP] = (settings_s){EEPROM_KEY,   4,   0,   1,    32,   1,   2,   DIS_POW, "Micro Step:    ", "       ", EEPROM_KEY};
  settings[SET_PAUSE]     = (settings_s){EEPROM_KEY,   0,   0,   0,  5000,   1, 250, DIS_VALUE, "Pause:         ", "ms     ", EEPROM_KEY};
  settings[SET_TURN]      = (settings_s){EEPROM_KEY,   2,   0,   1,    25,   1,   1, DIS_VALUE, "Rotate:        ", " steps ", EEPROM_KEY};
  settings[SET_RPM]       = (settings_s){EEPROM_KEY, 100,   0,  10,  6000, 100,  10, DIS_VALUE, "Speed:         ", " RPM   ", EEPROM_KEY};
  settings[SET_DIR]       = (settings_s){EEPROM_KEY,   1,   0,   0,     1,   1,   1,   DIS_DIR, "Direction:     ", "       ", EEPROM_KEY};
  settings[SET_VERSION]   = (settings_s){EEPROM_KEY,   0,   0,   0,     0,   1,   0,  DIS_NONE, "Version:       ", "1.1.0  ", EEPROM_KEY};
  settings[SET_ABOUT]     = (settings_s){EEPROM_KEY,   0,   0,   0,     0,   0,   0,  DIS_NONE, "About:         ", "HCW    ", EEPROM_KEY};

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
      //Save the settings when we exit back to the home screen
      EEPROM.put(0, settings); 
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

  double dmicroseconds =  (double)((double)settings[SET_MICROSTEP].currentValue) * 
                          (double)((double)settings[SET_RPM].currentValue / (double)settings[SET_RPM].divider) * 
                          (double)((double)settings[SET_RATIO].currentValue / (double)settings[SET_RATIO].divider) *
                          2.0; //divide by 2 so there is equal time high and low

  //This is how many micro seconds half the period will be
  microseconds = microPulses / (unsigned long)dmicroseconds;

  //Arduino isnt the best timer accuracy, limit to 100us 
  microseconds = max(microseconds, 100);

  if(micronow - micropulse >= microseconds)
  {
    micropulse = micronow;
    togglePulse == LOW ? togglePulse = HIGH : togglePulse = LOW;
    microsteps += (int)togglePulse;
  }

  
  if(microsteps < (unsigned long)(settings[SET_TURN].currentValue * settings[SET_MICROSTEP].currentValue))
  {  
    micropause = micronow;
  }
  else
  {
    if(micronow - micropause < (unsigned long)(settings[SET_PAUSE].currentValue * 1000L))
    {
      togglePulse = LOW;
    }
    else
    {
      microsteps = 0;
    }
  }

#if(USING_PAUSE_SWITCH == 1)
  if(digitalRead(PAUSE_IN) == startStatePause)
  {
    run_state = PAUSED; 
  }
  else
#endif
  {
    run_state = RUN; 
  }
   
  digitalWrite(PUL_OUT, togglePulse ? HIGH : LOW);
  digitalWrite(DIR_OUT, settings[SET_DIR].currentValue > 0 ? HIGH : LOW);
  digitalWrite(EN_OUT, run_state == PAUSED ? HIGH : LOW);
}


void setup()
{
 // Start the display library
 lcd.begin(16, 2);              

 EEPROM.get(0, settings);
 
 // Check if we need to reset the EEPROM
 if(settings[0].startKey != EEPROM_KEY || 
    settings[0].endKey != EEPROM_KEY || 
    settings[SET_COUNT - 1].startKey != EEPROM_KEY || 
    settings[SET_COUNT - 1].endKey != EEPROM_KEY)
 {
   reset_settings();
   EEPROM.put(0, settings);
 }

 pinMode(DIR_OUT, OUTPUT);
 pinMode(EN_OUT, OUTPUT);
 pinMode(PUL_OUT, OUTPUT);
 pinMode(PAUSE_IN, INPUT_PULLUP);

 digitalWrite(DIR_OUT, LOW);
 digitalWrite(EN_OUT, HIGH); //Start paused
 digitalWrite(PUL_OUT, LOW);
  
 UpdateDisplay();

#if(USING_PAUSE_SWITCH == 1)
 run_state = READY;
 startStatePause = digitalRead(PAUSE_IN);
#else
 run_state = RUN;
#endif
 
}

void loop()
{
  static unsigned int button_timer = 0;
  static unsigned long system_timer = 0;

  StepperMotor();
  
  //make this a 100hz loop
  if((micros()) < system_timer) return;

  system_timer = (micros() + 10000);
 
  lcd_key = read_LCD_buttons();   

  if(lcd_key != BTN_NONE)
  {
    button_timer++; //debounce
    lcd_key_last = lcd_key;
  }
  else
  { 
    if((button_timer > ButtonDebounce && HandleButton(lcd_key_last)) || (last_run_state != run_state))
    {
      UpdateDisplay();
      last_run_state = run_state;
    }

    lcd_key_last = lcd_key;
    button_timer = 0;
  }
  
}
