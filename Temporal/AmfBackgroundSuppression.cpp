/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AmfBackgroundSuppression.h"
#include "AoiElement.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DynamicObject.h"
#include "ObjectResource.h"
#include "ProgressTracker.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "switchOnEncoding.h"
#include "ThresholdLayer.h"
#include "TimelineWidget.h"

namespace
{
   template<typename T>
   void updateMedianEstimate(T *pBackground, void *pFrameVoid)
   {
      T *pFrame = reinterpret_cast<T*>(pFrameVoid);
      if(*pFrame > *pBackground)
      {
         (*pBackground)++;
      }
      else if(*pFrame < *pBackground)
      {
         (*pBackground)--;
      }
   }
   template<typename T>
   void setDiff(T *pBackground, void *pFrameVoid, void *pForegroundVoid)
   {
      T *pFrame = reinterpret_cast<T*>(pFrameVoid);
      T *pForeground = reinterpret_cast<T*>(pForegroundVoid);
      *pForeground = static_cast<T>(fabs(static_cast<double>(*pFrame) - *pBackground));
   }
}

AmfBackgroundSuppression::AmfBackgroundSuppression() : mBackground(static_cast<RasterElement*>(NULL)),
                                                       mForeground(static_cast<RasterElement*>(NULL))
{
   setName("Approximated Median Filter Background Suppression");
   setDescription("Extract the foreground from video frames using an approximated median filter background model.");
   setDescriptorId("{38B55C08-085E-4517-9C81-8F2DFA178806}");
   setVersion("0.1");
   setProductionStatus(false);
   setCreator("Trevor Clarke");
   setMenuLocation("[Temporal]/Background Estimation/Approximated Median Filter");
   setSingleForegroundMask(true);
}

AmfBackgroundSuppression::~AmfBackgroundSuppression()
{
}

BackgroundSuppressionShell::InitializeReturnType AmfBackgroundSuppression::initializeModel()
{
   if(getCurrentFrame() > 0)
   {
      return INIT_COMPLETE;
   }
   RasterDataDescriptor *pDesc = static_cast<RasterDataDescriptor*>(getRasterElement()->getDataDescriptor());
   mBackground = ModelResource<RasterElement>(RasterUtilities::createRasterElement("AMF Background Model",
      pDesc->getRowCount(), pDesc->getColumnCount(), pDesc->getDataType(), true, getRasterElement()));
   mForeground = ModelResource<RasterElement>(RasterUtilities::createRasterElement("AMF Foreground",
      pDesc->getRowCount(), pDesc->getColumnCount(), pDesc->getBandCount(), pDesc->getDataType(), BSQ, true, getRasterElement()));
   if(mBackground.get() == NULL || mForeground.get() == NULL)
   {
      mProgress.report("Unable to initialize background model.", 0, ERRORS, true);
      return INIT_ERROR;
   }
   DynamicObject *pForeMeta = mForeground->getMetadata();
   DynamicObject *pFrameMeta = pDesc->getMetadata();
   if(pForeMeta != NULL && pFrameMeta != NULL)
   {
      try
      {
         std::vector<double> frameTimes = dv_cast<std::vector<double> >(pFrameMeta->getAttributeByPath(FRAME_TIMES_METADATA_PATH));
         pForeMeta->setAttributeByPath(FRAME_TIMES_METADATA_PATH, frameTimes);
      }
      catch(const std::bad_cast&)
      {
      }
   }
   FactoryResource<DataRequest> request;
   request->setWritable(true);
   DataAccessor backgroundAccessor = mBackground->getDataAccessor(request.release());
   DataAccessor frameAccessor = getCurrentFrameAccessor();
   size_t rowSize = pDesc->getColumnCount() * pDesc->getBytesPerElement();
   for(unsigned int row = 0; row < pDesc->getRowCount(); row++)
   {
      if(!backgroundAccessor.isValid() || !frameAccessor.isValid())
      {
         mProgress.report("Unable to initialize background model.", 0, ERRORS, true);
         return INIT_ERROR;
      }
      memcpy(backgroundAccessor->getRow(), frameAccessor->getRow(), rowSize);
      backgroundAccessor->nextRow();
      frameAccessor->nextColumn();
   }

   return INIT_CONTINUE;
}

bool AmfBackgroundSuppression::preprocess()
{
   return true;
}

bool AmfBackgroundSuppression::updateBackgroundModel()
{
   RasterDataDescriptor *pDesc = static_cast<RasterDataDescriptor*>(mBackground->getDataDescriptor());
   FactoryResource<DataRequest> request;
   request->setWritable(true);
   DataAccessor backgroundAccessor = mBackground->getDataAccessor(request.release());
   DataAccessor frameAccessor = getCurrentFrameAccessor();
   for(unsigned int row = 0; row < pDesc->getRowCount(); row++)
   {
      for(unsigned int col = 0; col < pDesc->getColumnCount(); col++)
      {
         if(!backgroundAccessor.isValid() || !frameAccessor.isValid())
         {
            return false;
         }
         switchOnEncoding(pDesc->getDataType(), updateMedianEstimate, backgroundAccessor->getColumn(), frameAccessor->getColumn());
         backgroundAccessor->nextColumn();
         frameAccessor->nextColumn();
      }
      backgroundAccessor->nextRow();
      frameAccessor->nextRow();
   }
   return true;
}

bool AmfBackgroundSuppression::extractForeground()
{
   RasterDataDescriptor *pDesc = static_cast<RasterDataDescriptor*>(mForeground->getDataDescriptor());
   FactoryResource<DataRequest> backgroundRequest;
   DataAccessor backgroundAccessor = mBackground->getDataAccessor(backgroundRequest.release());
   FactoryResource<DataRequest> foregroundRequest;
   foregroundRequest->setWritable(true);
   foregroundRequest->setInterleaveFormat(BSQ);
   DimensionDescriptor frame = pDesc->getBands()[getCurrentFrame()];
   foregroundRequest->setBands(frame, frame, 1);
   DataAccessor foregroundAccessor = mForeground->getDataAccessor(foregroundRequest.release());
   DataAccessor frameAccessor = getCurrentFrameAccessor();
   for(unsigned int row = 0; row < pDesc->getRowCount(); row++)
   {
      for(unsigned int col = 0; col < pDesc->getColumnCount(); col++)
      {
         if(!backgroundAccessor.isValid() || !frameAccessor.isValid() || !foregroundAccessor.isValid())
         {
            return false;
         }
         switchOnEncoding(pDesc->getDataType(), setDiff, backgroundAccessor->getColumn(),
            frameAccessor->getColumn(),
            foregroundAccessor->getColumn());
         backgroundAccessor->nextColumn();
         foregroundAccessor->nextColumn();
         frameAccessor->nextColumn();
      }
      backgroundAccessor->nextRow();
      foregroundAccessor->nextRow();
      frameAccessor->nextRow();
   }
   return true;
}

bool AmfBackgroundSuppression::validate()
{
   return true;
}

bool AmfBackgroundSuppression::displayResults()
{
   if(getView() != NULL)
   {
      ThresholdLayer *pLayer = static_cast<ThresholdLayer*>(getView()->createLayer(THRESHOLD, mForeground.release()));
      if(pLayer == NULL)
      {
         return false;
      }
      pLayer->setPassArea(UPPER);
      pLayer->setRegionUnits(RAW_VALUE);
      pLayer->setFirstThreshold(25);
   }
   return true;
}
