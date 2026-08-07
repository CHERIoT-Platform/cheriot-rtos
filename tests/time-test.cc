#define TEST_NAME "Time"
#include "tests.hh"
#include <time.h>

namespace
{
	void test_time_conversion(time_t timestamp,
	                          int    year,
	                          int    month,
	                          int    day,
	                          int    hour,
	                          int    minute,
	                          int    second,
	                          int    dayOfWeek)
	{
		debug_log("Checking conversion of {}-{}-{} {}:{}:{}",
		          year,
		          month + 1,
		          day,
		          hour,
		          minute,
		          second);
		struct tm tm;
		auto     *ret = gmtime_r(&timestamp, &tm);
		TEST_EQUAL(ret, &tm, "gmtime_r failed");
		TEST_EQUAL(tm.tm_year, year - 1900, "Incorrect year from timestamp");
		TEST_EQUAL(tm.tm_mon, month, "Incorrect month from timestamp");
		TEST_EQUAL(tm.tm_mday, day, "Incorrect day from timestamp");
		TEST_EQUAL(tm.tm_hour, hour, "Incorrect hour from timestamp");
		TEST_EQUAL(tm.tm_min, minute, "Incorrect minute from timestamp");
		TEST_EQUAL(tm.tm_sec, second, "Incorrect second from timestamp");
		TEST_EQUAL(
		  tm.tm_wday, dayOfWeek, "Incorrect day of the week from timestamp");
		time_t reverse = timegm(&tm);
		TEST_EQUAL(reverse, timestamp, "timegm and gmtime disagree");
	}

	void test_time_wrapping(time_t timestamp,
	                        int    year,
	                        int    month,
	                        int    day,
	                        int    hour,
	                        int    minute,
	                        int    second,
	                        int    dayOfWeek)
	{
		debug_log("Checking wrapping for {}-{}-{} {}:{}:{}",
		          year,
		          month + 1,
		          day,
		          hour,
		          minute,
		          second);
		struct tm tm;
		tm.tm_year     = year - 1900;
		tm.tm_mon      = month;
		tm.tm_mday     = day;
		tm.tm_hour     = hour;
		tm.tm_min      = minute;
		tm.tm_sec      = second;
		time_t reverse = timegm(&tm);
		debug_log("Wrapping gave {}-{}-{} {}:{}:{}",
		          tm.tm_year + 1900,
		          tm.tm_mon + 1,
		          tm.tm_mday,
		          tm.tm_hour,
		          tm.tm_min,
		          tm.tm_sec);
		TEST_EQUAL(reverse, timestamp, "timegm and gmtime disagree");
		TEST_EQUAL(
		  tm.tm_wday, dayOfWeek, "Incorrect day of the week from timestamp");
	}
} // namespace

int test_time()
{
	// About now:
	test_time_conversion(1786102686, 2026, 7, 7, 11, 38, 06, 5);
	// February 29 on a leap year
	test_time_conversion(825595722, 1996, 1, 29, 12, 8, 42, 4);
	// Test a negative number (Ada Lovelace's birthday
	test_time_conversion(-4861728000, 1815, 11, 10, 0, 0, 0, 0);
	// Day that looks like a leap year but is actually not and needs normalising
	test_time_wrapping(857218122, 1997, 1, 29, 12, 8, 42, 6);

	// See if wrapping works all of the way back to February 29th from a
	// non-leap year with a large negative day.
	test_time_wrapping(825595722, 1997, 1, -337, 12, 8, 42, 4);
	// Check negative values, which should all give the same timestamp
	test_time_wrapping(857218122, 1997, 3, -30, 12, 8, 42, 6);
	test_time_wrapping(857218122, 1997, 4, -60, 12, 8, 42, 6);
	test_time_wrapping(857218122, 1997, 5, -91, 12, 8, 42, 6);
	test_time_wrapping(857218122, 1997, 5, -90, -12, 8, 42, 6);
	test_time_wrapping(857218122, 1997, 5, -90, -11, -52, 42, 6);
	test_time_wrapping(857218122, 1997, 5, -90, -11, -51, -18, 6);
	return 0;
}
