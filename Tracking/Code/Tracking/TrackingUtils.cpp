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

CvMat* ransac_affine(std::vector<std::pair<CvPoint2D32f, CvPoint2D32f> >& corr, int maxIter, float thresh, unsigned int numNeeded)
{
   if (corr.empty())
   {
      return NULL;
   }
   thresh = thresh * thresh; // calculating sqrt for each test is expensive, so we square the check value to change from RMS to MSE
   unsigned int bestCount = 0;
   CvMat* pBest = NULL;
   CvMat* pTest = cvCreateMat(2, 3, CV_32F);
   CvMat* pVer = cvCreateMat(3, 1, CV_32F);
   CvMat* pVerRes = cvCreateMat(2, 1, CV_32F);
   pVer->data.fl[2] = 1.0;
   for (int it = 0; it < maxIter; ++it)
   {
      // build mss
      int pMss[3];
      pMss[0] = rand() % corr.size();
      do
      {
         pMss[1] = rand() % corr.size();
      }
      while (pMss[1] == pMss[0]);
      do
      {
         pMss[2] = rand() % corr.size();
      }
      while (pMss[2] == pMss[0] || pMss[2] == pMss[1]);
      CvPoint2D32f pSrc[3] = {corr[pMss[0]].first, corr[pMss[1]].first, corr[pMss[2]].first};
      CvPoint2D32f pDst[3] = {corr[pMss[0]].second, corr[pMss[1]].second, corr[pMss[2]].second};

      cvGetAffineTransform(pSrc, pDst, pTest);

      // test the mss
      unsigned int fitCount = 0;
      for (unsigned int j = 0; j < corr.size(); ++j)
      {
         if (j == pMss[0] || j == pMss[1] || j == pMss[2])
         {
            continue;
         }
         pVer->data.fl[0] = corr[j].first.x;
         pVer->data.fl[1] = corr[j].first.y;
         cvGEMM(pTest, pVer, 1.0, NULL, 0.0, pVerRes);
         float tmpX = pVerRes->data.fl[0] - corr[j].second.x;
         float tmpY = pVerRes->data.fl[1] - corr[j].second.y;
         float mse = (tmpX * tmpX + tmpY * tmpY) / 2.0f;
         if (mse < thresh)
         {
            // fits the affine transform
            fitCount++;
         }
      }
      if (fitCount >= numNeeded && fitCount > bestCount)
      {
         if (pBest == NULL)
         {
            pBest = pTest;
            pTest = cvCreateMat(2, 3, CV_32F);
         }
         else
         {
            std::swap(pBest, pTest);
         }
         bestCount = fitCount;
      }
   }
   cvReleaseMat(&pTest);
   cvReleaseMat(&pVer);
   cvReleaseMat(&pVerRes);
   return pBest;
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