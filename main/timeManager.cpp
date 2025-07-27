#include "timeManager.h"

TimeInfo cachedTimes[11]; // 0 for IST, 1-10 for other timezones

void initTimeCache() {
  for (int i = 0; i < 11; i++) {
    cachedTimes[i].lastUpdate = 0;
  }
}

void invalidateTimeCache(int cacheIndex) {
  if (cacheIndex >= 0 && cacheIndex < 11) {
    cachedTimes[cacheIndex].lastUpdate = 0;
  }
}

TimeInfo getTimeInfo(const char* tz, int cacheIndex) {
  unsigned long currentMillis = millis();
  
  if (cacheIndex >= 0 && cacheIndex < 11 && 
      cachedTimes[cacheIndex].lastUpdate > 0 && 
      currentMillis - cachedTimes[cacheIndex].lastUpdate < TIME_CACHE_DURATION) {
    return cachedTimes[cacheIndex];
  }
  
  
  static bool ntpInitialized = false;
  if (!ntpInitialized) {
    delay(2000);
    ntpInitialized = true;
  }
  
  setenv("TZ", tz, 1);
  tzset();
  delay(100);
  
  struct tm timeinfo;
  TimeInfo result = {0, 0, 0, currentMillis};
  
  for (int attempts = 0; attempts < 3; attempts++) {
    if (getLocalTime(&timeinfo)) {
      result.hour = timeinfo.tm_hour;
      result.minute = timeinfo.tm_min;
      result.second = timeinfo.tm_sec;
      break;
    }
    delay(500);
  }
  
  if (cacheIndex >= 0 && cacheIndex < 11) {
    cachedTimes[cacheIndex] = result;
  }
  
  return result;
}

String formatTime12Hour(int hour, int minute, int second, bool showSeconds) {
  bool isPM = hour >= 12;
  int displayHour = hour;
  
  if (displayHour == 0) {
    displayHour = 12;
  } else if (displayHour > 12) {
    displayHour -= 12;
  }
  
  String timeStr = String(displayHour) + ":" + 
                   String(minute < 10 ? "0" : "") + String(minute);
  
  if (showSeconds) {
    timeStr += ":" + String(second < 10 ? "0" : "") + String(second);
  }
  
  timeStr += (isPM ? " PM" : " AM");
  return timeStr;
}

String getTimeString(const char* tz) {
  TimeInfo time = getTimeInfo(tz, -1);
  char buf[9];
  sprintf(buf, "%02d:%02d:%02d", time.hour, time.minute, time.second);
  return String(buf);
}