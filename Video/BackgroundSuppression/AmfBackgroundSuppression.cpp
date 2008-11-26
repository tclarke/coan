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
#include "BitMask.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DynamicObject.h"
#include "ObjectResource.h"
#include "PlugInFactory.h"
#include "ProgressTracker.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "switchOnEncoding.h"
#include "ThresholdLayer.h"

PLUGINFACTORY(AmfBackgroundSuppression);

namespace
{
   template<typename T>
   void updateMedianEstimate(T *pBackground, void *pFrameVoid, int off)
   {
      T *pFrame = reinterpret_cast<T*>(pFrameVoid) + off;
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
   void setDiff(T *pBackground, void *pFrameVoid, int off, BitMask *pPoints, int row, int col, double threshold)
   {
      T *pFrame = reinterpret_cast<T*>(pFrameVoid) + off;
      if(fabs(static_cast<double>(*pFrame) - *pBackground) > static_cast<T>(threshold))
      {
         pPoints->setPixel(col, row, true);
      }
   }
}

AmfBackgroundSuppression::AmfBackgroundSuppression() : mBackground(static_cast<RasterElement*>(NULL)),
                                                       mForeground(static_cast<AoiElement*>(NULL)),
                                                       mThreshold(20.0)
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
   mForeground = ModelResource<AoiElement>("AMF Foreground", getRasterElement());
   if(mBackground.get() == NULL || mForeground.get() == NULL)
   {
      mProgress.report("Unable to initialize background model.", 0, ERRORS, true);
      return INIT_ERROR;
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

   if(getView() != NULL)
   {
      getView()->createLayer(AOI_LAYER, mForeground.release());
   }

   mThreshold = 2500.0;

   return INIT_CONTINUE;
}

bool AmfBackgroundSuppression::preprocess()
{
   if(!BackgroundSuppressionShell::preprocess())
   {
      return false;
   }
   return boxFilter(3, EXTEND);
}

bool AmfBackgroundSuppression::updateBackgroundModel()
{
   RasterDataDescriptor *pDesc = static_cast<RasterDataDescriptor*>(mBackground->getDataDescriptor());
   FactoryResource<DataRequest> request;
   request->setWritable(true);
   DataAccessor backgroundAccessor = mBackground->getDataAccessor(request.release());
   void *pBuf = getTemporaryFrameBuffer();
   for(unsigned int row = 0; row < pDesc->getRowCount(); row++)
   {
      for(unsigned int col = 0; col < pDesc->getColumnCount(); col++)
      {
         if(!backgroundAccessor.isValid())
         {
            return false;
         }
         switchOnEncoding(pDesc->getDataType(), updateMedianEstimate, backgroundAccessor->getColumn(),
            pBuf, row * pDesc->getColumnCount() + col);
         backgroundAccessor->nextColumn();
      }
      backgroundAccessor->nextRow();
   }
   return true;
}

bool AmfBackgroundSuppression::extractForeground()
{
   RasterDataDescriptor *pDesc = static_cast<RasterDataDescriptor*>(mBackground->getDataDescriptor());
   FactoryResource<DataRequest> backgroundRequest;
   DataAccessor backgroundAccessor = mBackground->getDataAccessor(backgroundRequest.release());
   void *pBuf = getTemporaryFrameBuffer();
   FactoryResource<BitMask> pPoints;
   for(unsigned int row = 0; row < pDesc->getRowCount(); row++)
   {
      for(unsigned int col = 0; col < pDesc->getColumnCount(); col++)
      {
         if(!backgroundAccessor.isValid())
         {
            return false;
         }
         switchOnEncoding(pDesc->getDataType(), setDiff, backgroundAccessor->getColumn(),
            pBuf, row * pDesc->getColumnCount() + col,
            pPoints.get(), row, col, mThreshold);
         backgroundAccessor->nextColumn();
      }
      backgroundAccessor->nextRow();
   }
   mForeground->clearPoints();
   mForeground->addPoints(pPoints.get());
   return true;
}

bool AmfBackgroundSuppression::validate()
{
   return true;
}

bool AmfBackgroundSuppression::displayResults()
{
   return true;
}
