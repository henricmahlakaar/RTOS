#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "TimeParser.h"

int time_parse(char *time)
{
    int seconds = TIME_LEN_ERROR;

    if (time == NULL) {
        return TIME_ARRAY_ERROR;
    }

    if (strlen(time) != 6) {
        return TIME_LEN_ERROR;
    }

    for (int i = 0; i < 6; i++) {
        if (!isdigit(time[i])) {
            return TIME_NOT_DIGIT_ERROR;
        }
    }

    int values[3];

    values[2] = atoi(time + 4);
    time[4] = 0;

    values[1] = atoi(time + 2);
    time[2] = 0;

    values[0] = atoi(time);

    if (values[0] < 0 || values[0] > 23 ||
        values[1] < 0 || values[1] > 59 ||
        values[2] < 0 || values[2] > 59)
    {
        return TIME_VALUE_ERROR;
    }

    seconds =
        values[0] * 3600 +
        values[1] * 60 +
        values[2];

    if (seconds == 0) {
        return TIME_ZERO_ERROR;
    }

    return seconds;
}