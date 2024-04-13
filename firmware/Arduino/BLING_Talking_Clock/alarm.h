#include <HTTPClient.h>

// Returns number of seconds after midnight for the alarm
int GetAlarmTime()
{
  HTTPClient http;
  int httpCode = -1;

  // This is the location of your alarm.txt file. Change as needed!
  http.begin("https://raw.githubusercontent.com/EugeneScully/bling-alarm/main/alarm.txt");
  httpCode = http.GET();  //send GET request
  if (httpCode != 200) {
    http.end();
    return -1;
  } else {
    const char *s;
    int i;
    String payload = http.getString();
    http.end();
    s = payload.c_str();
    // Get the raw HTTP response text (+HHMM)
    // and convert the time zone offset (HHMM) into seconds

    //  Serial.print("TZ offset ");
    //  Serial.println(s);
    i = ((s[0] - '0') * 10) + (s[1] - '0'); // hour
    i *= 60;
    i += ((s[3] - '0') * 10) + (s[4] - '0'); // minute
    
    return i; // return minutes
  } // if successfully connected
  return -1;
}

void CheckIfAlarm(int alarmTime) {
  int alarmTimeHours = alarmTime / 60; // get hours
  if (rtc.getHours() != alarmTimeHours) {
    return; // check if hours match
  } 

  int alarmTimeMinutes = alarmTime % 60; // get minutes
  if (rtc.getMinutes() != alarmTimeMinutes) {
    return; // check if minutes match
  }

  // If hours and minutes match, sound alarm
  Serial.println("Alarm time reached!");
  // TODO: Sound alarm
}
