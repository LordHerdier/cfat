#ifndef CFAT_DATETIME_H
#define CFAT_DATETIME_H

#include <time.h>

void getDateTime(short* seconds, char* tenths, short* date);
time_t convertFATDateTime(short date, short time);
void convertDateTime(short time, short date, char* dateTimeStr);

#endif // CFAT_DATETIME_H
