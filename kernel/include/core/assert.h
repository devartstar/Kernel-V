#ifndef KERN_ASSERT_H
#define KERN_ASSERT_H

#include "panik.h"

#define KASSERT(expr) \
	do { \
		if(!(expr)) { \
			panik("Assertion Failed: %s %s:%d in %s()\n", #expr, __FILE__, __LINE__, __func__); \
		} \
	} while(0)

#define assert(expr) KASSERT(expr)

#define BUG() \
	panik("BUG at %s:%d in %s()\n", __FILE__, __LINE__, __func__)

#define BUG_ON(condition) \
	do { \
		if (condition) { \
			BUG(); \
		} \
	} while(0)

#endif
