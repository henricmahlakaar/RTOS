#include <gtest/gtest.h>
#include "TimeParser.h"

TEST(TimeParserTest, CorrectTime)
{
    char time_test[] = "000120";

    ASSERT_EQ(time_parse(time_test), 80);
}

TEST(TimeParserTest, NullPointer)
{
    ASSERT_EQ(time_parse(nullptr),
              TIME_ARRAY_ERROR);
}

TEST(TimeParserTest, WrongLength)
{
    char time_test[] = "123";

    ASSERT_EQ(time_parse(time_test),
              TIME_LEN_ERROR);
}

TEST(TimeParserTest, InvalidHour)
{
    char time_test[] = "240000";

    ASSERT_EQ(time_parse(time_test),
              TIME_VALUE_ERROR);
}

TEST(TimeParserTest, InvalidMinute)
{
    char time_test[] = "006000";

    ASSERT_EQ(time_parse(time_test),
              TIME_VALUE_ERROR);
}

TEST(TimeParserTest, InvalidSecond)
{
    char time_test[] = "000060";

    ASSERT_EQ(time_parse(time_test),
              TIME_VALUE_ERROR);
}

TEST(TimeParserTest, ContainsLetters)
{
    char time_test[] = "12AB34";

    ASSERT_EQ(time_parse(time_test),
              TIME_NOT_DIGIT_ERROR);
}

TEST(TimeParserTest, ZeroTime)
{
    char time_test[] = "000000";

    ASSERT_EQ(time_parse(time_test),
              TIME_ZERO_ERROR);
}
