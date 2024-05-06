#include "Arduino.h"
#include "WiFi.h"
#include "Audio.h" // Library Manager - ESP32_AudioI2S
#include "Wire.h"
#include "esp32-hal-tinyusb.h" // required for entering download mode

#include <Adafruit_GFX.h> // Library Manager
#include <Adafruit_NeoMatrix.h> // Library Manager
#include <Adafruit_NeoPixel.h> // Library Manager
#include <RV3028C7.h> // HitHub - https://github.com/UnexpectedMaker/RV-3028-C7-Arduino-Library

#include "secret.h"
#include "display.h"
#include "utc_offset.h"

#include "alarm.h"
#include "school_day.h"

#define LED_PIN     18
#define I2S_DOUT    3
#define I2S_BCLK    2
#define I2S_LRC     1
#define I2S_SD      4

// Buttons
#include "OneButton.h"  // From Library Manager
#define BUT_A       11
#define BUT_B       10
#define BUT_C       33
#define BUT_D       34
OneButton Button_A(BUT_A, false);
OneButton Button_B(BUT_B, false);
OneButton Button_C(BUT_C, false);
OneButton Button_D(BUT_D, false);

RV3028C7 rtc;
const char* ntpServer = "pool.ntp.org";

unsigned long nextDisplayUpdate = 0;
modes mode = RUNNING;
bool ready = false;
int alarmTimes[7];
DateRange schoolDays[20];
int schoolDaysCount = -1;

Audio audio;

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(40, 8, LED_PIN,
                            NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
                            NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE,
                            NEO_GRB            + NEO_KHZ800);


void Speak(String text)
{
  Serial.print("Saying: ");
  Serial.println(text);
  audio.connecttospeech(text.c_str(), "en"); // Google TTS
}

void setup()
{
  // we need serial output for the plotter
  Serial.begin(115200);

  // Matrix Power
  pinMode(6, OUTPUT);
  digitalWrite(6, HIGH);

  // I2S Mic Left channel select
  pinMode(39, OUTPUT);
  digitalWrite(39, LOW);

  // I2S Amp Enable
  pinMode(I2S_SD, OUTPUT);
  digitalWrite(I2S_SD, HIGH);

  // Button IO
  pinMode(BUT_A, INPUT);
  pinMode(BUT_B, INPUT);
  pinMode(BUT_C, INPUT);
  pinMode(BUT_D, INPUT);

  Wire.begin();

  Load_Settings();

  // Attatch button callbacks for buttons A,B,C & D
  Button_A.attachClick(BUTTON_A_Press);
  Button_A.attachDuringLongPress(BUTTON_A_LongPress);

  Button_B.attachClick(BUTTON_B_Press);
  Button_B.attachDuringLongPress(BUTTON_B_LongPress);

  Button_C.attachClick(BUTTON_C_Press);
  Button_C.attachLongPressStart(BUTTON_C_LongPress);

  Button_D.attachClick(BUTTON_D_Press);
  Button_D.attachDoubleClick(BUTTON_D_Double);
  Button_D.attachLongPressStart(BUTTON_D_LongPress);

  delay(200);

  // Initialiase the RGB Matrix
  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(5);

  // Statup WiFi
  matrix.setRotation(2);
  matrix.fillScreen(0);

  if (ssid == "")
  {
    matrix.setCursor(3, 0);
    matrix.setTextColor(matrix.Color(255, 0, 0));
    matrix.print("!SSID!");
    matrix.show();

    return;
  }

  matrix.setCursor(0, 0);
  matrix.setTextColor(matrix.Color(0, 255, 0));
  matrix.print("WiFi");
  matrix.show();

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  nextDisplayUpdate = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    if ((long)(millis() - nextDisplayUpdate) > 500)
    {
      Serial.print(".");
      matrix.print(".");
      matrix.show();
      nextDisplayUpdate = millis();
    }
    //    delay(100);
  }

  // Startup RTC and set time
  if (rtc.begin() == false)
  {
    Serial.println("Failed to detect RV-3028-C7!");
  }
  else
  {
    bool time_error = false;
    int user_GMT = UpdateGMT_NTP();

    if (user_GMT != 999)
    {
      configTime(user_GMT * 3600, 0, ntpServer);
      delay(100);

      struct tm timeinfo;
      if (!getLocalTime(&timeinfo))
      {
        Serial.println("Failed to obtain time");
        time_error = true;
      }
      else
      {
        Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
        rtc.setDateTimeFromTM(timeinfo);
      }
    }
    else
      time_error = true;

    if (time_error)
    {
      // Hard-coded for time error
      rtc.setDateTimeFromISO8601("1900-01-01T00:00:00");
    }

    // Writes the new date time to RTC, good or bad
    rtc.synchronize();
  }

  // Fetch the alarm
  if (GetAlarmTimes(alarmTimes)) {
    Serial.print("Alarms loaded : ");
    for (int i = 0; i < 7; i++) {
      Serial.print(alarmTimes[i]);
      Serial.print(",");
    }
    Serial.println();
  }
  else {
    Serial.println("Failed to load alarms");
  }

  // Fetch the school dates
  uint8_t year = rtc.getCurrentDateTimeComponent(DATETIME_YEAR);
  schoolDaysCount = GetSchoolDays(schoolDays, year);
  if (schoolDaysCount > 0) {
    Serial.print("School days loaded : ");
    for (int i = 0; i < schoolDaysCount; i++) {
      Serial.print(schoolDays[i].startDay);
      Serial.print("/");
      Serial.print(schoolDays[i].startMonth);
      Serial.print("-");
      Serial.print(schoolDays[i].endDay);
      Serial.print("/");
      Serial.print(schoolDays[i].endMonth);
      Serial.print(",");
    }
  }
  else {
    Serial.println("Failed to load school days");
  }

  // Init I2S Amplifier
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, -1);
  audio.setVolume(settings_volume); // 0...21
  audio.setConnectionTimeout(500, 2700);

  Speak("Bling");
  String text = "Bling!";
  DisplayText(matrix, "BLING", matrix.Color(255, 84, 255), 2000);
}

void loop()
{
  //Process the audio loop
  audio.loop();
  // If we are talking, don't update the display or we'll stutter!
  if (audio.isRunning())
    return;

  // Don't process anything during initial boot
  if (!ready)
    return;

  Button_A.tick();
  Button_B.tick();
  Button_C.tick();
  Button_D.tick();

  // Check if it's time to wake up
  uint8_t hour = rtc.getCurrentDateTimeComponent(DATETIME_HOUR);
  uint8_t mins = rtc.getCurrentDateTimeComponent(DATETIME_MINUTE);
  uint8_t dayOfWeek = rtc.getCurrentDateTimeComponent(DATETIME_DAY_OF_WEEK);
  int currentTime =hour * 60 + mins; // in minutes
  int alarmTime = alarmTimes[dayOfWeek];
  if (CheckIfAlarm(alarmTime, currentTime)) {
    Speak("Wake up");
  }

  // Scrolling text overrides any other display
  if (ScrollText(matrix))
    return;

  // Displaying a static, non scrolling message
  if (isDisplaying)
  {
    if ((long)(millis() - nextDisplayUpdate > 0))
    {
      nextDisplayUpdate = millis();
      isDisplaying = false;
    }
    else
      return;
  }

  // No messages are being displayed, so let's show the good stuff
  if (mode == RUNNING)
  {
    ShowClock(matrix, false);
  }
}

void SpeakCurrentTime()
{
  uint8_t hour = rtc.getCurrentDateTimeComponent(DATETIME_HOUR);
  String mins = String(pad_digit(rtc.getCurrentDateTimeComponent(DATETIME_MINUTE), "0"));
  if ( mins == "00")
    mins = "";
  else
    mins = " " + mins;

  String when = " am";

  if (hour > 12)
  {
    hour -= 12;
    when = " pm";
  }

  Speak(String(hour) + mins + when);
}

// Button Callbacks
void BUTTON_A_Press()
{
  if (mode == MENU)
  {
    UpdateSetting(matrix, colors[1], -1);
  }
  else if (mode == RUNNING)
  {
    if (settings_volume > 0)
      settings_volume--;
    audio.setVolume(settings_volume);
    DisplayText(matrix, "VOL " + String(settings_volume), colors[1], 1000);
    Save_Settings();
  }
}

void BUTTON_A_LongPress() {


}

void BUTTON_B_Press()
{
  if (mode == MENU)
  {
    UpdateSetting(matrix, colors[1], 1);
  }
  else if (mode == RUNNING)
  {
    if (settings_volume < 21)
      settings_volume++;
    audio.setVolume(settings_volume);
    DisplayText(matrix, "VOL " + String(settings_volume), colors[1], 1000);
    Save_Settings();
  }

}

void BUTTON_B_LongPress() {
}

void BUTTON_C_Press()
{
  if (mode == RUNNING )
  {
    SpeakCurrentTime();
  }
  else
  {
    SwitchSetting(matrix, colors[1]);
  }
}

void BUTTON_C_LongPress() {
  if (mode == RUNNING)
  {
    SwitchSetting(matrix, colors[1]);
  }
  else
  {
    DisplayText(matrix, "SAVING", colors[1], 1000);
    Save_Settings();
    matrix.setRotation(0);
    mode = RUNNING;
  }
}

void BUTTON_D_Press()
{
  if (mode == RUNNING )
  {
    settings_clock_col++;
    if (settings_clock_col > 8)
      settings_clock_col = 0;

    ShowClock(matrix, true);
    Save_Settings();
  }
  else
  {
    matrix.setRotation(0);
    mode = RUNNING;
  }
}

void BUTTON_D_Double() {
  DisplayText(matrix, "REBOOT", matrix.Color(255, 0, 0), 2000);
  ESP.restart();
}

void BUTTON_D_LongPress() {
  DisplayText(matrix, "DOWNLOAD", matrix.Color(255, 0, 0), 2000);
  usb_persist_restart(RESTART_BOOTLOADER);
}

// optional
void audio_info(const char *info) {
  Serial.print("info        "); Serial.println(info);
}

void audio_eof_speech(const char *info) {
  Serial.print("eof_speech  "); Serial.println(info);
  ready = true;
}

const uint16_t colors[9] = {
  matrix.Color(128, 0, 0),
  matrix.Color(128, 64, 0),
  matrix.Color(128, 128, 0),
  matrix.Color(64, 128, 0),
  matrix.Color(0, 128, 0),
  matrix.Color(0, 128, 64),
  matrix.Color(0, 128, 128),
  matrix.Color(0, 64, 128),
  matrix.Color(0, 0, 128),
};

const uint16_t colors_colon[9] = {
  matrix.Color(128, 128, 128),
  matrix.Color(128, 128, 128),
  matrix.Color(128, 128, 128),
  matrix.Color(128, 128, 128),
  matrix.Color(128, 128, 128),
  matrix.Color(128, 128, 128),
  matrix.Color(128, 128, 128),
  matrix.Color(128, 128, 128),
  matrix.Color(128, 128, 128),
};
