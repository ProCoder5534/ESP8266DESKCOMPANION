#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include "config.h"

struct TimeInfo {
  int hour;
  int minute;
  int second;
  unsigned long lastUpdate;
};

// Time functions
void initTimeCache();
void invalidateTimeCache(int cacheIndex);
TimeInfo getTimeInfo(const char* tz, int cacheIndex);
String formatTime12Hour(int hour, int minute, int second, bool showSeconds = true);
String getTimeString(const char* tz);

#endif