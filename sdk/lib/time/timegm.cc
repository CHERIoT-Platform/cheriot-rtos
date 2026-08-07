// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/**
 * This file provides implementations of the timegm and gmtime_r functions,
 * which convert between a human-friendly time and a UNIX timestamp.
 *
 * UNIX timestamps ignore leap seconds, which means that this conversion can
 * also do so,
 */

#include <array>
#include <debug.hh>
#include <time.h>

namespace
{
	using Debug = ConditionalDebug<true, "Time">;

	/// The number of seconds in one non-leap-second minute
	constexpr int SecondsPerMinute = 60;
	/// The number of minutes in one hour.
	constexpr int MinutesPerHour = 60;
	/// The number of seconds in one hour
	constexpr int SecondsPerHour = MinutesPerHour * SecondsPerMinute;
	/// The number of hours in one day.
	constexpr int HoursPerDay = 24;
	/// The number of hours in one day.
	constexpr int SecondsPerDay = HoursPerDay * SecondsPerHour;
	/// The number of months in a year
	constexpr int MonthsPerYear = 12;

	/**
	 * Calculate whether the specified year is a leap year.  Leap years happen
	 * when a year is divisible by 4, unless it is also divisible by 100 but
	 * not 400.  2000 is a helpful test vector here (it is not a leap year).
	 */
	constexpr bool year_is_leapyear(int year)
	{
		return ((year % 4) == 0) &&
		       (((year % 100) != 0) || ((year % 400) == 0));
	}

	/// The year that is zero in `struct tm::tm_year`
	constexpr int ZeroYear = 1900;
	/// The year whose start is the zero point for a `time_t`.
	constexpr int UnixEpochYear = 1970;

	// Check some common incorrect values and a simple check of an obvious one.
	static_assert(year_is_leapyear(2000));
	static_assert(year_is_leapyear(1996));
	static_assert(!year_is_leapyear(2100));
	static_assert(!year_is_leapyear(1900));

	/**
	 * Return the number of days in `month` in `year`.  The `year` parameter is
	 * necessary only for February (month 1 counting from 0), but is provided
	 * for all months simplify call sites.
	 */
	constexpr int days_in_month(int month, int year)
	{
		// Number of days in each month in a year that is not a leap year.
		constexpr std::array<int, MonthsPerYear> DaysInEachMonth = {
		  31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

		// Check that the months really do add up to the days in a year.
		static_assert(
		  [&]() {
			  int days = 0;
			  for (auto m : DaysInEachMonth)
			  {
				  days += m;
			  }
			  return days;
		  }() == 365,
		  "Days in each month don't add up to the days in a year!");

		Debug::Assert(month >= 0, "Negative month {} is invalid", month);
		Debug::Assert(
		  month < DaysInEachMonth.size(), "Month {} is invalid", month);

		int days = DaysInEachMonth[month];
		// Fix leap years.
		if ((month == 1) && year_is_leapyear(year))
		{
			days++;
		}
		return days;
	}

	/**
	 * Returns the number of days in the specified year.
	 */
	constexpr int days_in_year(int year)
	{
		return year_is_leapyear(year) ? 366 : 365;
	}

	/**
	 * Returns the number of seconds in the year, accounting for leap years.
	 */
	constexpr time_t seconds_in_year(int year)
	{
		return days_in_year(year) * SecondsPerDay;
	}

	/**
	 * Returns the number of seconds in the given month, in the specified year,
	 * accounting for leap years and leap seconds.
	 */
	constexpr time_t seconds_in_month(int month, int year)
	{
		return days_in_month(month, year) * SecondsPerDay;
	}

	/**
	 * Compute the day of the week, using the table-lookup method.  The day is
	 * indexed from 1 and the month from 0, as in `struct tm`.
	 */
	constexpr int day_of_the_week(int day, int month, int year)
	{
		static constexpr std::array<int, MonthsPerYear> MonthCodes = {
		  0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5};
		static constexpr std::array<int, 7> CenturyCodes = {
		  4, 2, 0, 6, 4, 2, 0};
		int yearInCentury = year % 100;
		int yearCode      = ((yearInCentury + (yearInCentury / 4)) % 7);
		int monthCode     = MonthCodes[month];
		int century       = year / 100;
		// Every day outside of the range this calculation works for is Monday.
		if ((century < 17) || (century > 23))
		{
			return 0;
		}
		int centuryCode        = CenturyCodes[century - 17];
		int leapYearCorrection = year_is_leapyear(year) && (month < 2) ? -1 : 0;
		return (yearCode + monthCode + centuryCode + day + leapYearCorrection) %
		       7;
	}

	// Test some cases for computing the day of the year, including ones either
	// side of February 29th in a leap year.
	static_assert(day_of_the_week(14, 2, 1879) == 5);
	static_assert(day_of_the_week(7, 7, 2026) == 5);
	static_assert(day_of_the_week(14, 1, 1996) == 3);
	static_assert(day_of_the_week(14, 3, 1996) == 0);
	static_assert(day_of_the_week(1, 2, 1997) == 6);

} // namespace

time_t timegm(struct tm *time)
{
	// Normalise `value` such that it is treated as a digit in base `base` and
	// any over / underflow is propagated to the next digit.
	constexpr auto Normalise = [](int &value, int &next, int base) {
		// This is a subtraction if value is negative
		next += (value / base);
		// If the value overflows, just add the overflowed amount to the next
		// 'digit'.
		if (value >= base)
		{
			value = (value % base);
		}
		else if (value < 0)
		{
			// The remainder of a negative value is either zero or negative.
			// If it's zero, 'borrow' one from the next digit.
			auto remainder = (value % base);
			if (remainder == 0)
			{
				value = 0;
			}
			else
			{
				next--;
				value = base + (value % base);
			}
		}
	};

	// Check that the normalisation correctly handles the corner cases and does
	// not affect the result of arithmetic.  The lambdas below here are used
	// only in the static asserts below.
	constexpr auto SumNormalised = [=](int value, int next, int base) {
		int sum = (next * base) + value;
		Normalise(value, next, base);
		return (next * base) + value;
	};
	constexpr auto SumOriginal = [=](int value, int next, int base) {
		return (next * base) + value;
	};
	constexpr auto CheckNormalised = [=](int value, int next, int base) {
		return SumNormalised(value, next, base) ==
		       SumOriginal(value, next, base);
	};
	static_assert(CheckNormalised(0, 5, 60));
	static_assert(CheckNormalised(60, 5, 60));
	static_assert(CheckNormalised(130, 5, 60));
	static_assert(CheckNormalised(-1, 5, 60));
	static_assert(CheckNormalised(-130, 5, 60));

	// Normalise everything up to days of the month.
	Normalise(time->tm_sec, time->tm_min, SecondsPerMinute);
	Normalise(time->tm_min, time->tm_hour, MinutesPerHour);
	Normalise(time->tm_hour, time->tm_mday, HoursPerDay);

	// We now need to handle overflow in day-of-the-month.  This is not a
	// trivial normalisation because the length of a month is not constant
	// (and, in the case of February, *changes* based on the year, so if the
	// day causes us to walk back an entire year then the day may be wrong).

	// Do a first-pass normalisation of the month into the year so that we can
	// use the year to compute the length of the month.
	Normalise(time->tm_mon, time->tm_year, MonthsPerYear);

	// Years are relative to 1900 for struct tm, provide a helper to get the
	// current year: we need to update the `tm_year` field in the passed
	// structure.
	auto year = [&]() { return time->tm_year + ZeroYear; };

	// The `tm_mday` field is indexed from 1.  This is annoying to operate on
	// and so compute it relative to 0 and then write back the value later.
	int dayOfMonth = time->tm_mday - 1;

	// Walk backwards to reach the correct month if the value is less than the
	// start of a month.
	while (dayOfMonth < 0)
	{
		time->tm_mon--;
		// We've possibly made the month negative, so renormalise (if this is
		// the first loop iteration, we may have made it *more* negative).
		Normalise(time->tm_mon, time->tm_year, MonthsPerYear);
		dayOfMonth += days_in_month(time->tm_mon, year());
	}
	// Walk forwards to reach the correct month if the value is greater than
	// the length of the current month.
	while (dayOfMonth >= days_in_month(time->tm_mon, year()))
	{
		dayOfMonth -= days_in_month(time->tm_mon, year());
		time->tm_mon++;
		Normalise(time->tm_mon, time->tm_year, MonthsPerYear);
	}
	time->tm_mday = dayOfMonth + 1;

	// Now we finally have a normalised struct tm!  Now compute the time_t from
	// it.
	time_t result = 0;
	// Years can be before the epoch, which means that the result is negative.
	if (year() < UnixEpochYear)
	{
		for (int y = UnixEpochYear - 1; y >= year(); y--)
		{
			result -= seconds_in_year(y);
		}
	}
	else if (year() > UnixEpochYear)
	{
		for (int y = UnixEpochYear; y < year(); y++)
		{
			result += seconds_in_year(y);
		}
	}

	// At this point, we're at the start of the year, so everything else will
	// add to the result.

	// Initialise the day of the year to the day of the month.  As we add in
	// the time for any prior months, we'll also update this.
	time->tm_yday = dayOfMonth;

	// Add in the days from each prior month.
	for (int m = 0; m < time->tm_mon; m++)
	{
		result += seconds_in_month(m, year());
		time->tm_yday += days_in_month(m, year());
	}

	// Now compute the day of the week from the normalised day of the month,
	// month, and year.  This is done here because we don't need the inputs to
	// this later and it may slightly improve register allocation.
	time->tm_wday = day_of_the_week(time->tm_mday, time->tm_mon, year());

	// Everything else is a constant number of seconds, so just multiply and
	// add.
	result += dayOfMonth * SecondsPerDay;
	result += time->tm_hour * SecondsPerHour;
	result += time->tm_min * SecondsPerMinute;
	result += time->tm_sec;

	return result;
}

struct tm *gmtime_r(const time_t *__restrict timer,
                    struct tm *__restrict result)
{
	time_t time = *timer;
	int    year = UnixEpochYear;
	// While the time is negative, add the duration of years before the epoch
	// to it and move the year backwards.
	while (time < 0)
	{
		time += seconds_in_year(year);
		year--;
	}
	// While the time is more than the current year, subtract the length of the
	// current year from it and move the year forward.
	while (time > seconds_in_year(year))
	{
		time -= seconds_in_year(year);
		year++;
	}
	// At this point, `time` is now a positive value that is less than the
	// length of the year.
	result->tm_year = year - ZeroYear;
	result->tm_mon  = 0;
	result->tm_yday = 0;

	// Now add the seconds in each month to it.
	while (time > seconds_in_month(result->tm_mon, year))
	{
		time -= seconds_in_month(result->tm_mon, year);
		result->tm_yday += days_in_month(result->tm_mon, year);
		result->tm_mon++;
	}

	// Days of the month count from 1, everything else counts from 0.
	result->tm_mday = static_cast<int>(1 + (time / SecondsPerDay));
	time %= SecondsPerDay;
	result->tm_hour = static_cast<int>(time / SecondsPerHour);
	time %= SecondsPerHour;
	result->tm_min = static_cast<int>(time / SecondsPerMinute);
	time %= SecondsPerMinute;
	result->tm_sec = static_cast<int>(time);
	result->tm_yday += result->tm_mday;
	result->tm_wday = day_of_the_week(result->tm_mday, result->tm_mon, year);
	return result;
}
