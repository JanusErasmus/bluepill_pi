/*
 * utils.h
 *
 *  Created on: 09 Jul 2018
 *      Author: Janus Erasmus
 */

#ifndef UTILS_INC_UTILS_H_
#define UTILS_INC_UTILS_H_
#include <stdint.h>

#ifdef __cplusplus
 extern "C" {
#endif

#ifndef VT100_OFF
#define COLOR(__c,__x)    "\x1b[3" #__c "m" __x "\x1b[0m"
#define COLOR_BOLD(__c,__x)    "\x1b[3" #__c ";1m" __x "\x1b[0m"
#define UNDERLINE(__x) "\x1b[4m" __x "\x1b[0m"
#define CLEAR_SCREEN    "\x1b[2J\x1b[H"
#else
#define COLOR(__c,__x)    __x
#define COLOR_BOLD(__c,__x) __x
#define UNDERLINE(__x) __x
#define CLEAR_SCREEN
#endif
#define RED(__x)        COLOR(1, __x )
#define GREEN(__x)        COLOR(2, __x )
#define YELLOW(__x)        COLOR(3, __x )
#define BLUE(__x)        COLOR(4, __x )
#define MAGENTA(__x)    COLOR(5, __x )
#define CYAN(__x)        COLOR(6, __x )
#define RED_B(__x)        COLOR_BOLD(1, __x )
#define GREEN_B(__x)        COLOR_BOLD(2, __x )
#define YELLOW_B(__x)        COLOR_BOLD(3, __x )
#define BLUE_B(__x)        COLOR_BOLD(4, __x )
#define MAGENTA_B(__x)    COLOR_BOLD(5, __x )
#define CYAN_B(__x)        COLOR_BOLD(6, __x )

void diag_dump_buf(void *p, uint32_t s);

#define PRINTF printf

#define printReg(_c)  printf(#_c " : 0x%08X\n", (unsigned int)READ_REG(_c));

#ifdef __cplusplus
 }
#endif
#endif /* UTILS_INC_UTILS_H_ */
