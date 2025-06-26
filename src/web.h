#pragma once

extern RTC_DATA_ATTR tHistory history[100];
extern RTC_DATA_ATTR int historyIndex;

extern void loopWeb();
void initWeb(void);
