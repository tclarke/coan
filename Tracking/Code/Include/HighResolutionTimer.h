/****************************************************************************

                         ***** UNCLASSIFIED *****
                         -------------------------

This source code was developed by Ball Aerospace & Technologies Corp.,
Fairborn, Ohio, under Prime Contract F33600-01-D-1017 monitored by the
National Air & Space Intelligence Center of the Air Intelligence Agency.

This material (software and user documentation) is subject to export
controls imposed by the United States Export Administration Act of 1979,
as amended and the International Traffic In Arms Regulation (ITAR),
22 CFR 120-130.
   -------------------------

   File:           HighResolutionTimer.h

   Module:         CppTests

   Library:        CppTests

   Description:    

   Dependencies:   

                        ***** UNCLASSIFIED *****

****************************************************************************/



#ifndef HIGH_RESOLUTION_TIMER_H
#define HIGH_RESOLUTION_TIMER_H 1

#include "AppConfig.h"

#if defined(WIN_API)
#include "Windows.h"
#else
#include <sys/time.h>
#endif

#include <ostream>

namespace HrTimer
{

#if defined(WIN_API)
#define HrTimingType LONGLONG
#else
#define HrTimingType hrtime_t
#endif

inline HrTimingType getTime()
{
#if defined(WIN_API)
LARGE_INTEGER currentTime;
QueryPerformanceCounter(&currentTime);
return currentTime.QuadPart;
#else
return gethrtime();
#endif
}

inline double convertToSeconds(HrTimingType val)
{
#if defined(WIN_API)
LARGE_INTEGER frequency;
QueryPerformanceFrequency(&frequency);
LONGLONG longFrequency = frequency.QuadPart;
return val / ((double)longFrequency); 
#else
return val / 1000000000.0; //val on Solaris is in nano-seconds
#endif
}

class Resource
{
public:

   Resource()
	{
		mOutputType = 0; //use ostream
		init();
	}

   Resource(double* outputInto, bool milliSecond = true)
	{
		mOutputType = 2; //use double
		mpDoubleOutput = outputInto;
		mMillisecondResolution = milliSecond;
		init();
	}

   Resource(HrTimingType* start, HrTimingType* end)
   {
      mOutputType = 3;
      mpStartOutput = start;
      mpEndOutput = end;
      init();
   }

	~Resource()
	{
		HrTimingType end = HrTimer::getTime();
      if (mOutputType == 2)
		{
			//output value into a double
			double timeDiff = convertToSeconds(end - mStart);
			if (mMillisecondResolution)
			{
				timeDiff *= 1000.0;
			}
			*mpDoubleOutput = timeDiff;
		}
      if (mOutputType == 3)
      {
         *mpStartOutput = mStart;
         *mpEndOutput = end;
      }
	}

private:
	void init()
	{
		mStart = HrTimer::getTime();
	}

	HrTimingType mStart;
	int mOutputType;
	double* mpDoubleOutput;
   HrTimingType* mpStartOutput;
   HrTimingType* mpEndOutput;
	bool mMillisecondResolution;
};

};

#endif //HIGH_RESOLUTION_TIMER_H
