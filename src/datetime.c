#include <stdio.h>
#include <string.h>
#include "datetime.h"

void getDateTime(short* seconds, char* tenths, short* date) {
    // Get the current time
    time_t now = time(NULL);
    struct tm* currentTime = localtime(&now);

    // Seconds: count of 2-second increments
    *seconds = (currentTime->tm_sec / 2) & 0x1F; // 5 bits for seconds (0-29)

    // Tenths of a second
    *tenths = 0; // Assuming we do not track tenths of a second

    // Minutes
    short minutes = (currentTime->tm_min & 0x3F) << 5; // 6 bits for minutes (0-59)

    // Hours
    short hours = (currentTime->tm_hour & 0x1F) << 11; // 5 bits for hours (0-23)

    // Combine hours and minutes into the seconds field
    *seconds |= minutes | hours;

    // Day of the month
    short day = (currentTime->tm_mday & 0x1F); // 5 bits for day (1-31)

    // Month of the year
    short month = ((currentTime->tm_mon + 1) & 0x0F) << 5; // 4 bits for month (1-12)

    // Year from 1980
    short year = ((currentTime->tm_year - 80) & 0x7F) << 9; // 7 bits for year (0-127)

    // Combine day, month, and year into the date field
    *date = day | month | year;
}

time_t convertFATDateTime(short date, short time) {
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));

    tm.tm_sec = (time & 0x1F) * 2;
    tm.tm_min = (time >> 5) & 0x3F;
    tm.tm_hour = (time >> 11) & 0x1F;
    tm.tm_mday = date & 0x1F;
    tm.tm_mon = ((date >> 5) & 0x0F) - 1;
    tm.tm_year = ((date >> 9) & 0x7F) + 80;

    return mktime(&tm);
}

// helper function to convert the date and time to a human-readable format
void convertDateTime(short time, short date, char* dateTimeStr) {
    // extract components from the date and time fields
    int seconds = (time & 0x1F) * 2;
    int minutes = (time >> 5) & 0x3F;
    int hours = (time >> 11) & 0x1F;

    int day = date & 0x1F;
    int month = (date >> 5) & 0x0F;
    int year = ((date >> 9) & 0x7F) + 1980;

    // format the date and time as a string
    sprintf(dateTimeStr, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hours, minutes, seconds);
}
