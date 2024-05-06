#include <HTTPClient.h>

#include "DateRange.h"

/*
 * Fetches the start and end days for the school terms for this year
 * Returns the number of days,
 * starts: The school term start days as the number of days from the start of the year.
 * ends: The school term end days as the number of days from the start of the year.
*/
int GetSchoolDays(DateRange *dates, uint8_t currentYear) {
  HTTPClient http;
  int httpCode = -1;

  // This is the location of your alarm.txt file. Change as needed!
  http.begin("https://raw.githubusercontent.com/EugeneScully/bling-alarm/main/school_day.csv");
  httpCode = http.GET();  //send GET request
  if (httpCode != 200) {
    http.end();
    return -1;
  } else {
    const char *s;
    int i = 0;
    String payload = http.getString();
    http.end();
    s = payload.c_str();

    // The first line is the header, skip it
    s = strchr(s, '\n');
    s++;

    // While not EOF, read each line. Each line should contain a start end pair, comma separated, in format d/m/y
    // If the year doesn't match this year, skip it!
    
    int startDay, startMonth, startYear, endDay, endMonth, endYear;
    while (s != NULL) {
      // Read the start and end days
      int count = sscanf(s, "%d/%*d/%*d,%d/%*d/%*d", &startDay, &startMonth, &startYear, &endDay, &endMonth, &endYear);
      if (count != 6) {
        return false; // If we didn't get six values, something is wrong!
      }

      // If the year doesn't match this year, skip it!
      if (startYear != currentYear || endYear != currentYear) {
        continue;
      }

      if (startDay > 0 && startMonth > 0 && endDay > 0 && endMonth > 0) {
        // Convert the start and end days to the number of days from the start of the year
        dates[i].startDay = startDay;
        dates[i].startMonth = startMonth;
        dates[i].endDay = endDay;
        dates[i].endMonth = endMonth;
        i++;
      }
    }

    return i;
  }
}
