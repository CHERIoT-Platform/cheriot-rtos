#pragma once
#include <array>
#include <concepts>
#include <cstdint>

/**
 * Concept for an Ethernet adaptor.
 */
template<typename T>
concept IsEntropySource = requires(T source)
{
	/**
	 * Must export a flag indicating whether this is a cryptographically
	 * secure random number.
	 */
	{
		T::IsSecure
		} -> std::convertible_to<const bool>;

	/**
	 * Must return a random number.  All bits of the value type are assumed to
	 * be random, this should use a narrower type if it guarantees less
	 * entropy.
	 */
	{
		source()
		} -> std::same_as<typename T::ValueType>;

	/**
	 * Must provide a method that reseeds the entropy source.  If this is a
	 * hardware whitening engine that is continuously fed with entropy, this
	 * may be an empty function.  There are no constraints on the return type
	 * of this.
	 */
	{source.reseed()};
};
