// Copyright CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <randombytes.h>

#include <platform-entropy.hh>

namespace
{
	auto entropy()
	{
		static EntropySource entropySource;
		return entropySource();
	}

} // namespace

int randombytes(uint8_t *output, size_t n)
{
	constexpr size_t         BytesPerCall = sizeof(EntropySource::ValueType);
	EntropySource::ValueType value;
	size_t                   bytesRemaining = 0;

	auto next = [&]() {
		if (bytesRemaining == 0)
		{
			bytesRemaining = BytesPerCall;
			value          = entropy();
		}
		uint8_t nextByte = value;
		value >>= 8;
		bytesRemaining--;
		return nextByte;
	};

	for (size_t i = 0; i < n; i++)
	{
		output[i] = next();
	}
	return 0;
}
