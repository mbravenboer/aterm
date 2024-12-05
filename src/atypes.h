#ifndef ATYPES_H
#define ATYPES_H

#include "abool.h"

#ifdef __cplusplus
extern "C"
{
#endif/* __cplusplus */

typedef unsigned int ShortHashNumber;

typedef long MachineWord;
typedef unsigned long HashNumber;

#define ADDR_TO_SHORT_HNR(a) ((ShortHashNumber)(((((unsigned long)(char*)(a)) >> 2)&0xffffffff) ^ (((unsigned long)(char*)(a)) >> 34)))
#define ADDR_TO_HNR(a) (((HashNumber)(a)) >> 2)

#ifdef __cplusplus
}
#endif/* __cplusplus */

#endif /* ATYPES_H */
