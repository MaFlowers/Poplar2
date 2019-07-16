#ifndef __POP_COMM_HDR_H__
#define __POP_COMM_HDR_H__
#include <string.h>
#define CHECK_FLAG(V,F)      ((V) & (F))
#define SET_FLAG(V,F)        ((V) |= (F))
#define UNSET_FLAG(V,F)      ((V) &= ~(F))

#define STRCMP_EQUAL(A, B)	(strcmp(A, B) == 0)


#endif
