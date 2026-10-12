#ifndef TIMEPARSER_H
#define TIMEPARSER_H

#define TIME_LEN_ERROR       -1
#define TIME_ARRAY_ERROR     -2
#define TIME_VALUE_ERROR     -3

#define TIME_NOT_DIGIT_ERROR -4
#define TIME_ZERO_ERROR      -5

int time_parse(char *time);

#endif