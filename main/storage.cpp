#include "storage.h"

void loadTimezoneSelection() {
  EEPROM.begin(EEPROM_SIZE);
  selectedCount = 0;
  for (int i = 0; i < TIMEZONE_COUNT; i++) {
    timezoneSelected[i] = EEPROM.read(i) == 1;
    if (timezoneSelected[i]) selectedCount++;
  }
  EEPROM.end();
}

void saveTimezoneSelection() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < TIMEZONE_COUNT; i++) {
    EEPROM.write(i, timezoneSelected[i] ? 1 : 0);
  }
  EEPROM.commit();
  EEPROM.end();
}