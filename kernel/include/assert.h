#ifndef KERN_ASSERT_H
#define KERN_ASSERT_H

#include "panik.h"

void panik(const char* fmt, ...);
#define KASSERT(expr) \
	do { \
		if(!(expr)) { \
			panik("Assertion Fields: %s %s:%d in %s()\n", #expr, __FILE__, __LINE__, __func__)); \
		} \
	} while(0)

#define BUG() \
	panik("BUG at %s:%d in %s()\n", __FILE__, __LINE__, __func__)

#define BUG_ON(condition) \
	do { \
		if (condition) { \
			BUG(); \
		} \
	} while(0)

#endif
