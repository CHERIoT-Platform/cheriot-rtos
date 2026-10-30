#pragma once

/**
 * \file Feature detection for CHERIoT RTOS versions.
 *
 * This file contains macros of the form `CHERIOT_FEATURE_*` which are defined
 * to an integer that will monotonically increment if the feature is modified.
 *
 * This is intended to be used with new features to allow downstream consumers
 * to adopt them if compiled against a version of the RTOS with that feature
 * but still compile on older versions.
 *
 * This file contains two helper macros, `CHERIOT_HAS_FEATURE` and
 * `CHERIOT_HAS_FEATURE_VERSION` that can be used to simplify feature checks.
 */

/**
 * Macro that evaluates to true if this version of CHERIoT RTOS has the named
 * feature.  This is directly equivalent to checking the feature name, but is
 * useful in generic code or in more complex conditional preprocessor
 * expressions.  It is most commonly used in code of the following form:
 *
 * ```c
 * #if CHERIOT_HAS_FEATURE(SOME_FEATURE)
 * // Code that depends on SOME_FEATURE here.
 * #else
 * // Fallback code for working without the feature here.
 * #endif
 * ```
 */
#define CHERIOT_HAS_FEATURE(feature) defined(CHERIOT_FEATURE_##feature)

/**
 * Macro that evaluates to true if and only if `feature` exists and has a
 * version that is at least `version`.  This allows graceful fallback to older
 * versions.  It is most commonly used in a block of the following form:
 *
 * ```c
 * #if CHERIOT_HAS_FEATURE_VERSION(SOME_FEATURE, 42)
 * // Code that depends on SOME_FEATURE version 42 here.
 * #elif CHERIOT_HAS_FEATURE(SOME_FEATURE)
 * // Code that depends on SOME_FEATURE but not on a specific version here.
 * #else
 * // Fallback code for working without the feature here.
 * #endif
 * ```
 */
#define CHERIOT_HAS_FEATURE_VERSION(feature, version)                          \
	CHERIOT_HAS_FEATURE(feature) && ((CHERIOT_FEATURE_##feature) >= (version))

/**
 * Example feature.  This feature advertises the existence of this header.  At
 * present, it is of illustrative value only because it is guaranteed to exist.
 */
#define CHERIOT_FEATURE_FEATURES 1

// Checks that the macros behave as expected.  Uses `FEATURES` as a feature
// that does exist and has a version 1, `NOT_IMPLEMENTED` as a feature that
// does not exist.
#if !CHERIOT_HAS_FEATURE(FEATURES)
#	error Internal consistency error
#endif
#if !CHERIOT_HAS_FEATURE_VERSION(FEATURES, 1)
#	error Internal consistency error
#endif
#if CHERIOT_HAS_FEATURE_VERSION(FEATURES, 100)
#	error Internal consistency error
#endif
#if CHERIOT_HAS_FEATURE_VERSION(NOT_IMPLEMENTED, 1)
#	error Internal consistency error
#endif
