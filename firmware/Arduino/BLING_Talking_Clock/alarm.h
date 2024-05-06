#include <HTTPClient.h>

// Track if the alarm is currently active
bool isAlarmSet = false;

/*
 * Fetches the number of seconds after midnight for each day of the week.
 * Returns true if successful.
 * alarmTimes: Entries for the alarm time for each day in minutes. Should have length of 7.
*/
bool GetAlarmTimes(int *alarmTimes)
{
  HTTPClient http;
  int httpCode = -1;

  // This is the location of your alarm.txt file. Change as needed!
  http.begin("https://raw.githubusercontent.com/EugeneScully/bling-alarm/main/alarm.txt");
  httpCode = http.GET();  //send GET request
  if (httpCode != 200) {
    http.end();
    return false;
  } else {
    const char *s;
    int i;
    String payload = http.getString();
    http.end();
    s = payload.c_str();

    // Response is in the format: 
    // --:--,18:30,18:30,18:30,18:30,18:30,--:--,
    // Copy to the 'alarmTimes' array

    // Split the entries
    char *token = strtok((char *)s, ",");
    i = 0;
    while (token != NULL) {
      if (strcmp(token, "--:--") == 0) {
        alarmTimes[i] = -1;
      } else {
        // Convert to minutes
        int hours = atoi(strtok(token, ":"));
        int minutes = atoi(strtok(NULL, ":"));
        alarmTimes[i] = hours * 60 + minutes;
      }
    }
    return true;
  } // if successfully connected
  return false;
}

/*
* Is it the alarm time?
*/
bool CheckIfAlarm(int alarmTime, int currentTime) {

  if (alarmTime == -1) { // if no alarm set for today
    isAlarmSet = false;
    return false;
  }

  if (currentTime != alarmTime) {
    isAlarmSet = false;
    return false;
  }

  if (!isAlarmSet) {
    isAlarmSet = true;
    Serial.println("Alarm set!");
    return true;
  }

  return false;
}
