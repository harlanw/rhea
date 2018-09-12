#ifndef RHEA_ATTRIBUTES_H
#define RHEA_ATTRIBUTES_H

#ifndef __GNUC__
	#warn "Compiler not supported"
	#define ATTR(attrs) ((void) 0)
#else
	/**
	 * @brief Automatic attribute definition/expansion for one or more attributes
	 */
	#define ATTR(...) __attribute__((__VA_ARGS__))

	/**
	 * @brief Indicates a function that should run before main
	 */
	#define ATTR_CTOR	ATTR(constructor)

	/**
	 * @brief Indicates a function that should run after main
	 */
	#define ATTR_DTOR	ATTR(destructor)

	/**
	 * @brief Forces the compiler to inline the function
	 */
	#define ATTR_INLINE	ATTR(always_inline)

	/**
	 * @brief Informs the compiler that no paramters should be NULL
	 */
	#define ATTR_NONNULL	ATTR(nonnull)

	/**
	 * @brief Indicates a function that never returns
	 */
	#define ATTR_NORETURN	ATTR(noreturn)
#endif

#ifndef __has_attribute
	#define __has_attribute(attr) 0
#endif

#if __has_attribute(fallthrough)
	#define ATTR_FALLTHROUGH ATTR(fallthrough)
#else
	#define ATTR_FALLTHROUGH ((void) 0)
#endif

#endif
