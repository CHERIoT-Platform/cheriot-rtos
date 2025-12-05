#pragma once
#include <array>
#include <concepts>
#include <cstdint>
#include <ds/xoroshiro.h>

/**
 * Concept for an Entropy source.
 */
template<typename T>
concept IsEntropySource = requires(T source) {
	/**
	 * Must export a flag indicating whether this is a cryptographically
	 * secure random number.
	 */
	{ T::IsSecure } -> std::convertible_to<const bool>;

	/**
	 * Must return a random number.  All bits of the value type are assumed to
	 * be random, this should use a narrower type if it guarantees less
	 * entropy.
	 */
	{ source() } -> std::same_as<typename T::ValueType>;

	/**
	 * Must provide a method that reseeds the entropy source.  If this is a
	 * hardware whitening engine that is continuously fed with entropy, this
	 * may be an empty function.  There are no constraints on the return type
	 * of this.
	 */
	{ source.reseed() };
};

/**
 * A simple entropy source.  This wraps a few weak entropy sources to seed a
 * PRNG.  It is absolutely not secure and should not be used for anything that
 * depends on cryptographically secure random numbers!  Unfortunately, there is
 * nothing on the Sonata instantiation of CHERIoT SAFE that can be used as a
 * secure entropy source.
 *
 * This is provided as a generic implementation that can be used with an
 * interrupt source to provide a slightly better version for testing on FPGA
 * and simulation environments that don't have anything secure.
 */
class TrivialInsecureEntropySource
{
	ds::xoroshiro::P128R64 prng;

	protected:
	/// Reseed the PRNG with a provided set of jumps.
	void reseed(uint16_t jumps)
	{
		// Start from a not very random seed
		uint64_t seed = rdcycle64();
		prng.set_state(seed, seed >> 24);
		// Permute it with another not-very-random number
		for (uint32_t i = 0; i < ((jumps & 0xff00) >> 8); i++)
		{
			prng.long_jump();
		}
		for (uint32_t i = 0; i < (jumps & 0xff); i++)
		{
			prng.jump();
		}
		// At this point, our random number is in a fairly predictable state,
		// but with a fairly low probability of being the same predictable
		// state as before.
	}

	public:
	using ValueType = uint64_t;

	/// Definitely not secure!
	static constexpr bool IsSecure = false;

	/// Constructor, tries to generate an independent sequence of random numbers
	TrivialInsecureEntropySource()
	{
		reseed();
	}

	/// Reseed the PRNG.
	void reseed()
	{
		reseed(rdcycle64());
	}

	/// Get the next value from the PRNG.
	ValueType operator()()
	{
		return prng();
	}
};
