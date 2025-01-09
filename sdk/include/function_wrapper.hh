#include <cdefs.h>
#include <functional>
#include <tuple>

/**
 * Base template for `FunctionWrapper`, never used.
 */
template<typename FnType>
class FunctionWrapper;

/**
 * A non-owning type-erased reference to a callable object.  This is used
 * to pass lambdas (and similar) down the stack without increasing code
 * size by template specialisation.  Instances of this class must not be
 * stored, they should be used only for passing type-erased callable objects
 * down the stack.
 *
 * Instances of this class are two words: a reference to the lambda, and a
 * wrapper callback that invokes the lambda.
 *
 * This is similar to `std::function` but is non-owning and so is guaranteed
 * not to allocate memory (the called function may capture memory).
 */
template<class R, class... Args>
class FunctionWrapper<R(Args...)>
{
	/**
	 * Storage for the type-erased function.  This holds the reference to
	 * lambda and to the invoke function.
	 */
	alignas(void *) char storage[2 * sizeof(void *)];

	/**
	 * Base type for the type-erased function.  This defines the virtual
	 * function that is used to invoke the captured lambda.
	 */
	struct ErasedFunctionWrapperBase
	{
		virtual R operator()(Args... args) = 0;
	};

	/**
	 * Returns a pointer to the storage, cast to the type-erased function
	 * type.
	 */
	ErasedFunctionWrapperBase &stored_function()
	{
		return *reinterpret_cast<ErasedFunctionWrapperBase *>(storage);
	}

	/**
	 * Templated subclass that is specialised for each concrete callable
	 * type `T` that is passed.  One instance of this will be created for
	 * each lambda type, with a single method in its vtable that invokes
	 * the lambda.
	 */
	template<typename T>
	class ErasedFunctionWrapper : public ErasedFunctionWrapperBase
	{
		/// Pointer to the captured lambda.
		T &&fn;

		public:
		/**
		 * Invoke function.  This is virtual and overrides the version in
		 * the parent class, allowing this to be called from code that does
		 * not know the cocrete type of the lambda.
		 */
		R operator()(Args... args) override
		{
			return fn(std::forward<Args>(args)...);
		}

		/**
		 * Construct the type-erased function wrapper, capturing the
		 * lambda.
		 */
		ErasedFunctionWrapper(T &&fn) : fn{std::forward<T>(fn)} {}
	};

	public:
	/**
	 * This is a non-owning reference, delete its copy and move
	 * constructors to avoid accidental copies.
	 */
	FunctionWrapper(FunctionWrapper &)             = delete;
	FunctionWrapper(FunctionWrapper &&)            = delete;
	FunctionWrapper &operator=(FunctionWrapper &&) = delete;

	/**
	 * Construct the type-erased function wrapper, capturing the lambda.
	 */
	template<typename T>
	__always_inline FunctionWrapper(T &&fn)
	{
		// Make sure that we got the size for the storage right!
		static_assert(sizeof(storage) >= sizeof(ErasedFunctionWrapper<T>));
		// Construct the type-erased function in place.
		new (storage) ErasedFunctionWrapper<T>(std::forward<T>(fn));
	}

	/**
	 * Invoke the captured lambda.
	 */
	__always_inline R operator()(Args... args)
	{
		return stored_function()(std::forward<Args>(args)...);
	}
};
