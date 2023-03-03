Coding Conventions 
==================

This document describes the style guidelines for this project.

High-level goals
----------------

The style guide here is intended to help writing code that supports three core principles.

 - Intentionality.
 - Least privilege.
 - Correctness by construction.

The principle of intentionality means that we should avoid situations where anything happens as a result of ambient state and instead be explicit.
CHERI provides this at an object level, we must build systems that support it in higher-level abstractions.
The principle of least privilege means that we should avoid holding rights beyond those required.
Again, CHERI makes this easy to support at the object level, because a running piece of code can access only memory that it owns.
Correctness by construction means that we try to ensure that it is impossible to enter an undefined state by building systems where all invalid transitions are impossible.

Superficial style
-----------------

Anything that can be applied mechanically is applied via the clang-format definition.
Unfortunately, clang-format is not quite stable across releases and so we have decided to version-lock our clang-format to the version built from our compiler.
If you are using the dev container, this is installed in `/cheriot-tools/bin`, alongside the compiler.

All changes should have clang-format applied.
Unfortunately, clang-format cannot apply all style elements.

Any flow-control statement (`if`, `for`, `while` and so on) should always be followed by a block, not a single statement.
For example, this:

```c
if (condition)
{
	statement;
}
```

And not this:

```c
if (condition)
	statement;
```

The form with braces has two concrete advantages when later modifying the code:

 - Adding a new statement does not require adding braces, which reduces churn during code review.
 - It is not possible to accidentally forget to add braces when adding a new statement and have a line execute outside of the block, which caused the 'goto fail' vulnerability.

These and many other changes are enforced with `clang-tidy`.
As with `clang-format`, we use the version built from the same sources as our compiler.

Naming things
-------------

Consistent naming is important for navigating a new codebase.
Code is read far more times than it is written and so the goal is to optimise for reading, not for writing.
Prefer longer and more descriptive identifiers that make life easier for the reader of the code, to shorter ones that reduce the amount of typing that you need to do.

### General rules

Some rules apply of the thing that is being named.
We use different capitalisation rules for different kinds of identifier, which allows them to be unambiguous to the reader (and debugger) and also makes it easy to use the same name (but different capitalisation) for the only instance of a particular type and for a field and an accessor of that field.

#### Use full words

Abbreviations depend on shared context for understanding.
For example, `Cap` is understandable to someone with a lot of experience with CHERI as a common abbreviation for capability, but for someone more used to image processing it will be interpreted as being short for capture.
Some abbreviations are interpreted differently depending on the native language of the reader.
For example, an English speaker will read `a2b` as 'a to b', because two and to are homophones, whereas a French reader will read 'a deux b' and then have to remember that deux in English is a homophone for 'à'.

Abbreviations increase cognitive load for the reader.
Worse, they disproportionately increase the cognitive load for different groups.

#### Start with the most generic word

If an identifier contains multiple words, start with the one that provides the most context.
This helps when documentation and IDE autocomplete sort lexically or provide completions where prefix match is prioritised over substring match.
For example, `interrupt_enable` and `interrupt_disable` will appear adjacent in a sorted list, whereas `enable_interrupt` or `disable_interrupt` will be distinct.
In C, `enum`s are identifiers in a global namespace and so should start with their identifier name to improve autocompletion, whereas C++ `enum class`es are already namespaced and so do not need this.

#### Avoid ambiguous words

A lot of English words are both valid nouns and verbs.
In some uses, such as names of types, it is unambiguous that the noun form is meant, but in the names of functions and methods this is far less clear.
The C++ standard library has several examples of bad practice in this regard, for example `clear()` and `empty()` could both mean the other's behaviour and a reader knows which one empties the data structure and which returns whether the data structure is empty only by reading the documentation.
Compare these names to the OpenStep versions: `removeAllObjects` and `isEmpty`.
In this version, it is clear that the first is a mutator that removes every element in a collection and the second is a predicate that indicates whether the collection is empty.

### Types and constants

Depending on your perspective, constants are either variables that don't change or subtypes with a single element in their domain.
We lean towards the latter definition and so expect constants to follow the same naming rules as types.

Types should use `CapitalisedWords`.
Abstract types (C++ abstract classes or concepts) should use adjectives in their names, for example `Sortable` or `Comparable`.
Concrete types (C/C++ structures, C++ concrete classes) should use nouns, for example `HashTable` or `InterruptController`.

Related groups of types should either be in a common namespace or use a common prefix.

### Functions and methods

Functions and methods should use `underscore_separated_words`.

Predicate functions should start with a verb describing the predicates, such as `is` or `has`, for example `is_empty` or `has_interrupts_pending`.
Side-effecting functions should have the imperative form of a verb.

Functions and methods should follow noun-verb naming rules, such as `interrupt_disable`, rather than `disable_interrupt`.
This makes finding the relevant function easier both for the reasons outlined in the 'Start with a generic word' section and also because the caller typically knows the noun that they want to do something with and wants to enumerate the things that they can do.
In the above example, they may discover that `interrupt_mask` is preferable to `interrupt_disable` for their case.

### Fields and variables

Variables should use `camelCase`.
Arguments may use the OpenStep convention of starting with an article to distinguish them from fields.
Most of the time, fields are unambiguously nouns and so, for example `fooStart`, is obviously the start of the foo (whatever that is) and not an operation that starts a foo.
This is not true for variables that hold lambdas or other callable objects and so these must follow the rules for unambiguous naming.

### Parameter order

Public functions that may block should take a `Timeout *` as the first parameter.
This position makes it easy to forward to other functions (the `Timeout` structure is designed to be reused across nested calls).

Any software capabilities that authorise an operation should be passed next.
This ordering makes it easy to write templated wrappers that forward to other functions.

C++ features
------------

This is, primarily, a C++20 codebase and should use modern C++ features.
This is especially important for anything related to security.

Note that, because we are targeting a CHERI platform, the normal advice to avoid raw pointers can be toned down slightly: pointers are spatially safe and heap pointers are temporally safe.
For any code where availability is important (i.e. where crashing the compartment is not acceptable, such as the memory allocator or scheduler), raw pointers should be avoided.

An embedded subset of C++
-------------------------

The C++ standards committee does not condone subsetting C++ and so we have to define an unofficial subset.
We globally disable exceptions and run-time type information (RTTI).

Exceptions require a stack unwinder, which is a large complex piece of code that significantly increases code size (a problem for embedded systems) and provides an attractive target for attackers.
RTTI is simply large, which causes a problem for firmware-image size.

We do not support non-constant global initialisers.
These would require an explicit compartment initialisation phase, which could be added at some point.

We do support thread-safe static initialisation, though components that run with interrupts disabled (such as the scheduler or allocator) may wish to opt out of these.

Avoid mutable global variables
------------------------------

If an attacker is able to compromise one compartment, mutable global variables provide a stepping stone to allow access to resources owned by the compartment on behalf of another caller.
Where possible, state should be explicitly passed in a context object into a compartment so that two threads or two entries into the same compartment never have access to the state owned by another caller.

Seal anything exposed from a compartment
----------------------------------------

The sealing mechanism (either from the hardware for core components or the allocator for everything else) makes it possible to hand out tamper-proof pointers to context.
Note that there is one exception to the immutability of sealed capabilities: sealed capabilities may still lose the global and load-global permissions if loaded via a capability without the load-global permission.
This means that compartments must validate sealed context pointers after unsealing them if they require the GLOBAL or LOAD_GLOBAL permissions.

The only unsealed pointers that should be passed between compartments are values where both the caller and callee need to read or write the owned data.
In these situations, the caller may not trust the contents after the call and the callee may not trust the contents at the start (or during if interrupts are enabled and the object is not on the stack).

Require rights as arguments
---------------------------

Where possible, the right to perform an operation should be conveyed as an argument.
In some cases, this is simple to express because it's an object.
For example, the right to receive a packet on a socket is conveyed by the fact that the caller must present the socket (a sealed capability to some connection state) as an argument.
In other cases, it may require adding explicit arguments, for example to track quotas.

Check values at the boundary
----------------------------

Where possible, functions that are exported from compartments should check that their arguments are safe before passing them to other code, which should then not require dynamic checks.
This is not possible for things that might be used by other threads.
Any capability that has store-local permission is guaranteed to not be aliased across threads and so is safe from time-of-check-to-time-of-use vulnerabilities.
Any other object should be considered to be concurrently mutated by an attacker.

Minimise rights when calling other compartments
-----------------------------------------------

If a callee does not require write access to an object, remove store permission (and load-mutable).
If a callee does not require to capture a reference to an object, remove the global permission (and load global).
If a callee expects a data buffer, remove the read/write capability permission.

Ensure that pointers to sub-objects passed to a compartment are correctly bounded.

Use rich types
--------------

Public interfaces to compartments may need to be usable from C and so provide an `int` return with negative values indicating success.
Internal interfaces or things that can be guaranteed to be exposed only to higher-level languages should use option or variant types.
For example, consider this C API:

```c
int do_something();
```

A caller may have to write code such as:

```
int ret = do_something();
if (ret < 0)
{
	// Handle error
	return;
}
use(ret);
```

Passing `ret` to `use` without checking the error will not raise any compiler warnings.
In contrast, the modern C++ version would look like one of the following:

```c++
std::optional<int> do_something();
std::variant<ErrorFoo, ErrorBar, int> do_something();
```

In both cases, the return type cannot be implicitly cast to `int` and a code reviewer can easily see when the return type is not checked.
The second form is more explicit in the set of error conditions and can, with `std::visit` make it clear that each one is handled.

Similarly, wrapper templates and `enum class` definitions make it possible to define types that convey intentionality.
Where possible, avoid using raw integer types for strongly typed values.

Don't repeat yourself
---------------------

Various studies have shown that bug counts are proportional to the quantity of code.
If you are doing the same thing in two different contexts, factor it out into a common class, function, or lambda.
If a bug is found in this code, it is then fixed for all uses.

This can be taken to extremes.
If the code required to call a generic implementation is of a similar level of complexity to the implementation itself, then this is a sign that you've gone too far.

Make invalid states unrepresentable
-----------------------------------

In general, a constructed class should hold a valid state.
Without exceptions, C++ does not provide a mechanism for constructors to fail.
If construction can fail, use a factory method and a private constructor instead.
For example:

```c++
struct Example
{
	std::optional<Example> create(int anIndex)
	{
		if (canConstructWith(anIndex))
		{
			return Example{anIndex};
		}
		return std::nullopt;
	}
	private:
	Example(int);
	canConstructWith(int);
};
```

Here, the check that the constructor arguments are valid is separate from the construction.
In more complex examples, individual field creation may fail and fields may need to be initialised with other fields.
In this case, create each one locally and pass it by r-value reference into the constructor.

If construction must happen in place, have a private constructor and an `is_valid` method that are used only by the factory method.

Note: stack and global objects without copy / move constructors can still be constructed with these techniques if the caller provides space into which the factor can do a placement `new`.

Minimise variable scope
-----------------------

Variables should always be declared with the smallest scope and should be initialised at their point of creation.
For example, compare:

```c
void *somePointer;
...
somePointer = initialise();
```
versus:

```
...
void *somePointer = initialise()
```

In the first case, the compiler *might* be able to perform flow-sensitive analysis and warn that you are using the variable uninitialised, but it might not.
In the second case, it definitely can tell the scope in which `somePointer` is valid and the value holds the same
This is a more specific implementation of making invalid states unrepresentable.

Note that C++ can make initialising variables at point of use somewhat cumbersome if their initialisation is the result of some complex flow control.
It is possible to address this with lambdas, for example:

```c++
void *somePointer = ([&]() { 
	if (someComplexCondition())
	{
		return nullptr;
	}
	someOtherFunctionThatSetsState();
	if (somethingInfluencedByThatState())
	{
		return &someGlobal;
	}
	return aNewThing();
})();
```

Note that lambdas of this form are called in precisely one place and are not exposed as external symbols and so are always inlined in any form of optimised build.
This means that they do not increase code size.

Use lexical lifetimes where possible
------------------------------------

Even in the absence of exceptions, RAII makes it easy to reason about lifetimes of anything that can be lexically scoped.
This includes things like lock ownership.
The dialect of C/C++ that we support uses attributes for functions that run with interrupts disabled or enabled.
This makes it possible to control interrupt posture with lexical scopes.

For heap objects, prefer `std::unique_ptr` where possible.
Note that this takes a custom deleter and so can be used with non-standard allocation APIs.

Anything where a function or method must be called at the end of performing an operation should be considered suspect.
This can be implemented instead using one of two idioms:

 - The start-operation function should return an object whose destructor calls the end-operation function.
 - A function that takes a lambda and calls it bracketed by whatever is necessary to start and end the operation.

Use range-based for loops
-------------------------

When iterating over any collection, use range-based for loops.
If a type does not support range-based iteration, write a range adaptor *once* and use that.
This means that any off-by-one errors or similar bugs in iteration can be fixed in a single place.

*Exception*: If you are mutating the collection (not just the elements in a collection) then range-based iteration is often unsafe.
In this case, it is best to use standard-library adaptors such as `std::remove_if`.
Where this is not possible, write a single implementation of a mutation-safe loop that takes a lambda.

Generic code
------------

C++ provides two distinct mechanisms for writing code that operates over multiple types:

 - Templates define functions and classes that are specialised based on another concrete type.
 - Virtual functions (especially in combination with abstract base classes) allow different types to be substituted at run time.

Templates permit inlining, which can significantly reduce code size if it then exposes opportunities for simplifying the inlined code, but can combinatorially increase code size if the specialisation does not encourage simplification.
Virtual functions present an optimisation barrier, but do not require any code duplication.

As a general rule of thumb, you should always use templates if the code needs to support multiple types but will use a single one in a given firmware image and interfaces (abstract classes) where you need to handle a large number of different types.
A lot of code falls between these two extremes and so requires judgement:

 - Can you use `if constexpr` to significantly reduce code size?  This suggests using templates.
 - Is the code provided by the plugged-in type (or value) large?  This suggests using virtual functions.

Note that it is possible to combine the two approaches by having a template that is instantiated with an abstract class and then used with concrete subclasses.
This approach makes it easy to profile the overheads of both approaches by switching the template instantiation used.

Don't put off to run time what you can check at compile time
------------------------------------------------------------

Modern C++ allows a lot to happen in `constexpr` functions and these can be used with `static_assert`.
Anything that your code depends on that can be checked at compile time should be.
For example, in the code for deriving capabilities in the loader, we use `static_assert`s to check that the requested permissions are all present on the root from which they were derived.

Conversely, where things cannot be checked at compile time, ensure that they are checked at run time.
If a value comes from an untrusted source then it should be checked and errors reported via function return values.
If a value comes internally but an invalid value would be a critical security problem then use the `Debug::Invariant` code to ensure that we fail-stop if it is not valid.
If a value comes internally and is less important, use a `Debug::Assert` to check that it is valid in debug builds.

Trust your compiler
-------------------

The compiler is a very powerful tool.
The more information that you can give it, the better it will work for you.
In particular, make good use of always-inline and no-inline annotations.
By default, optimising for size, the compiler will be very conservative about inlining.

Explicitly direct inlining if doing so will allow specialisation.
A few things to consider:

 - Functions that take a structure as an argument will require stack space for the structure if not inlined, but can decompose it into individual fields and optimise based on values of the structure when inlined.
 - Functions that are usually called with one or more arguments that are compile-time constant will often benefit from inlining.
 - Functions that will be called from multiple places and shrink only slightly after inlining are better never inlined.

As always, measure the effect (ideally in debug builds as well as release builds) of changing inlining heuristics.

More generally, the compiler will optimise based on what you tell it.
You can provide hints to the compiler via various builtins, such as `__builtin_assume` and `__builtin_assume_aligned` or `__builtin_unreachable`.
If the compiler has a builtin for a particular operation then it's likely that mid-level analyses will be better able to work with this than with a custom implementation and that the final code generation will be tuned for the target.

Documentation comments
----------------------

Every class, field, and method should start with a doc comment.
This can use Markdown syntax.
It is not required to use `@param` and friends: modern versions of Doxygen are context sensitive and can automatically link parameter names in documentation to their version.
Some methods are obvious (e.g. `is_empty` is pretty obviously a predicate that checks if something is empty) but 'obvious' is not a machine-checkable property and it is much easier to write trivial descriptions for trivial methods than it is to write a mechanical checker that all non-obvious methods have documentation comments.

When writing a documentation comment for a method, make sure that you capture all of:

 - Why does this method exist (when would I want to call it)?
 - What does it mutate, if anything?
 - When can it fail, what arguments are valid?
 - If it fails, how does it report the error?

For types consider the first point and why someone would use this rather than an alternative.
For fields and global variables, think about:

 - When is this read or mutated?
 - What are the valid values, if less than the range of the type?
 - What (if any) values are treated as special?
 - Are there relationships between this and other state that must be maintained?
