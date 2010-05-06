/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppConfig.h"
#include "LocationType.h"
#include "RasterData.h"
#include "TrackingUtils.h"
#include <stdlib.h>
#include <opencv/cv.h>

#if defined(WIN_API)
#include <boost/cstdint.hpp>
using boost::uint8_t;
#endif

namespace TrackingUtils
{
subcubeid_t calculateSubcubeId(LocationType location, uint8_t levels,
                               Opticks::PixelLocation maxBb,
                               Opticks::PixelLocation minBb)
{
   subcubeid_t id(levels);
   for (uint8_t level = 0; level < levels; ++level)
   {
      Opticks::PixelLocation center(static_cast<int>((maxBb.mX - minBb.mX + 1) / 2.0 + minBb.mX),
                                    static_cast<int>((maxBb.mY - minBb.mY + 1) / 2.0 + minBb.mY));
      if (location.mX >= center.mX && location.mY <= center.mY) // quadrant 0
      {
         minBb.mX = center.mX;
         maxBb.mY = center.mY;
      }
      else if (location.mX < center.mX && location.mY <= center.mY) // quadrant 1
      {
         id |= 1 << (level * 2 + 4);
         maxBb = center;
      }
      else if (location.mX < center.mX && location.mY > center.mY) // quadrant 2
      {
         id |= 2 << (level * 2 + 4);
         maxBb.mX = center.mX;
         minBb.mY = center.mY;
      }
      else // quadrant 3
      {
         id |= 3 << (level * 2 + 4);
         minBb = center;
      }
   }
   return id;
}

bool calculateSubcubeBounds(subcubeid_t subcubeId, uint8_t& levels, Opticks::PixelLocation& maxBb, Opticks::PixelLocation& minBb)
{
   levels = subcubeId & 0x0f;
   subcubeId >>= 4;
   for (uint8_t level = 0; level < levels; ++level)
   {
      Opticks::PixelLocation center(static_cast<int>((maxBb.mX - minBb.mX + 1) / 2.0 + minBb.mX),
                                    static_cast<int>((maxBb.mY - minBb.mY + 1) / 2.0 + minBb.mY));
      switch (subcubeId & 0x03) // quadrant number
      {
      case 0:
         minBb.mX = center.mX;
         maxBb.mY = center.mY;
         break;
      case 1:
         maxBb = center;
         break;
      case 2:
         maxBb.mX = center.mX;
         minBb.mY = center.mY;
         break;
      case 3:
         minBb = center;
         break;
      default:
         break; // unreachable
      }
      subcubeId >>= 2;
   }
   return subcubeId == 0;
}

uint8_t calculateNeededLevels(uint32_t maxSubcubeSize,
                              Opticks::PixelLocation maxBb,
                              Opticks::PixelLocation minBb)
{
   maxBb = (maxBb - minBb) + 1;
   for (uint8_t level = 0; level <= 12; ++level) // maximum number of levels which can be represented
   {
      uint32_t curSize = maxBb.mX * maxBb.mY;
      if (curSize <= maxSubcubeSize)
      {
         return level;
      }
      maxBb.mX /= 2;
      maxBb.mY /= 2;
   }
   return 0xff; // Invalid return
}
}

IplImageResource::IplImageResource() : mpImage(NULL), mShallow(false)
{
}

IplImageResource::IplImageResource(IplImage* pImage) : mpImage(pImage), mShallow(false)
{
}

IplImageResource::IplImageResource(IplImageResource& other)
{
   reset(other.get());
   if (other.isShallow())
   {
      release();
   }
   else
   {
      take();
   }
   other.mpImage = NULL;
}

IplImageResource::IplImageResource(int width, int height, int depth, int channels) : mShallow(false)
{
   mpImage = cvCreateImage(cvSize(width, height), depth, channels);
}

IplImageResource::IplImageResource(int width, int height, int depth, int channels, char* pData) : mShallow(true)
{
   mpImage = cvCreateImageHeader(cvSize(width, height), depth, channels);
   mpImage->imageData = pData;
}

IplImageResource::~IplImageResource()
{
   reset(NULL);
}

bool IplImageResource::isShallow()
{
   return mShallow;
}

void IplImageResource::reset(IplImage* pImage)
{
   if (mpImage != NULL)
   {
      if (mShallow)
      {
         mpImage->imageData = NULL;
      }
      cvReleaseImage(&mpImage);
   }
   mpImage = pImage;
   mShallow = false;
}

IplImage* IplImageResource::get()
{
   return mpImage;
}

IplImage* IplImageResource::release()
{
   mShallow = true;
   return mpImage;
}

IplImage* IplImageResource::take()
{
   mShallow = false;
   return mpImage;
}

IplImageResource::operator IplImage*()
{
   return get();
}

IplImage& IplImageResource::operator*()
{
   return *get();
}

IplImageResource& IplImageResource::operator=(IplImageResource& other)
{
   mShallow = other.mShallow;
   mpImage = other.mpImage;
   other.mShallow = true;
   other.mpImage = NULL;
   return *this;
}

dataptr::dataptr(DataElement* pElement, DataPointerArgs args)
{
   int own(0);
   mpData = createDataPointer(pElement, &args, &own);
   mOwns = (own != 0);
}

dataptr::~dataptr()
{
   if (mOwns)
   {
      destroyDataPointer(mpData);
   }
}

dataptr::operator void*()
{
   return mpData;
}

dataptr::operator char*()
{
   return reinterpret_cast<char*>(mpData);
}

void* dataptr::get()
{
   return mpData;
}