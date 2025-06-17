/*****************************************************************************
* | File      	:	Debug.h
* | Author      :   Waveshare team + t0mg
* | Function    :	Debug to serial with printf or println
*----------------
* |	This version:   V1.1
* | Date        :   2025-06-16
* | Info        :   Added a println and printf-like macro for formatted output
*
******************************************************************************/
#ifndef __DEBUG_H
#define __DEBUG_H

#include <Wire.h>

#define USE_DEBUG 1
#if USE_DEBUG
	#define Debug(__info) Serial.print(__info)
	#define Debugln(__info) { Serial.print(__info); Serial.println(); }
	#define Debugf(format, ...) { \
		char __debug_buffer[256]; \
		snprintf(__debug_buffer, sizeof(__debug_buffer), format, ##__VA_ARGS__); \
		Serial.print(__debug_buffer); \
	}
#else
	#define Debug(__info)  
	#define Debugln(__info)
	#define Debugf(format, ...)
#endif

#endif
