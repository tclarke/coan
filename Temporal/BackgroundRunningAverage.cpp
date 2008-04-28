/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "BackgroundRunningAverage.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "LayerList.h"
#include "ModelServices.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "ProgressTracker.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterLayer.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "switchOnEncoding.h"
#include "TypeConverter.h"

namespace
{
   template<typename T>
   void updateEstimate(T *pData, void *pBackgroundVoid, double alpha)
   {
      T *pBackground = reinterpret_cast<T*>(pBackgroundVoid);
      *pBackground = static_cast<T>(alpha * static_cast<double>(*pData) + (1 - alpha) * static_cast<double>(*pBackground));
   }
   template<typename T>
   void copy(T *pData, void *pBackgroundVoid)
   {
      T *pBackground = reinterpret_cast<T*>(pBackgroundVoid);
      *pBackground = *pData;
   }
}

BackgroundRunningAverage::BackgroundRunningAverage()
{
   setName("Running Average Background Estimation");
   setDescription("Calculate a background estimation using a running average.");
   setDescriptorId("{71ce7c71-9c3a-41ca-8b51-822a632638ac}");
   setVersion("0.1");
   setProductionStatus(false);
   setCreator("Trevor Clarke");
   setSubtype("Background Estimation");
   setAbortSupported(true);
   setWizardSupported(true);
   setMenuLocation("[Temporal]/Background Estimation/Running Average");
}

BackgroundRunningAverage::~BackgroundRunningAverage()
{
}

bool BackgroundRunningAverage::getInputSpecification(PlugInArgList *&pArgList)
{
   VERIFY(pArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg(), NULL));
   VERIFY(pArgList->addArg<RasterElement>(Executable::DataElementArg()));
   if(isBatch())
   {
      VERIFY(pArgList->addArg<unsigned int>("Frame Number"));
   }
   else
   {
      VERIFY(pArgList->addArg<SpatialDataView>(Executable::ViewArg()));
   }
   VERIFY(pArgList->addArg<RasterElement>("Background Estimate", NULL));
   VERIFY(pArgList->addArg<double>("Learning Rate", 0.05));
   return true;
}

bool BackgroundRunningAverage::getOutputSpecification(PlugInArgList *&pArgList)
{
   VERIFY(pArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pArgList->addArg<RasterElement>("Background Estimate", NULL));
   return true;
}

bool BackgroundRunningAverage::execute(PlugInArgList *pInArgList, PlugInArgList *pOutArgList)
{
   bool initial = false;
   if(pInArgList == NULL)
   {
      return false;
   }
   ProgressTracker progress(pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg()),
      "Updating background estimate", "temporal", "{79725fd2-199b-4919-ac1a-36e6374d107f}");

   RasterElement *pData = pInArgList->getPlugInArgValue<RasterElement>(Executable::DataElementArg());
   VERIFY(pData);
   const RasterDataDescriptor *pDesc = static_cast<const RasterDataDescriptor*>(
      pData->getDataDescriptor());
   VERIFY(pDesc != NULL);
   double alpha;
   VERIFY(pInArgList->getPlugInArgValue("Learning Rate", alpha));
   SpatialDataView *pView = NULL;
   DimensionDescriptor frame;
   if(isBatch())
   {
      unsigned int frameNumber;
      VERIFY(pInArgList->getPlugInArgValue("Frame Number", frameNumber));
      frame = pDesc->getActiveBand(frameNumber);
   }
   else
   {
      pView = pInArgList->getPlugInArgValue<SpatialDataView>(Executable::ViewArg());
      VERIFY(pView);
      LayerList *pLayerList = pView->getLayerList();
      RasterLayer *pLayer = static_cast<RasterLayer*>(pLayerList->getLayer(RASTER, pLayerList->getPrimaryRasterElement()));
      VERIFY(pLayer);
      if(pLayer->getDisplayMode() != GRAYSCALE_MODE)
      {
         progress.report("Primary raster layer must be in grayscale display mode", 0, ERRORS, true);
         return false;
      }
      frame = pLayer->getDisplayedBand(GRAY);
   }
   VERIFY(frame.isValid());
   RasterElement *pBackground = pInArgList->getPlugInArgValue<RasterElement>("Background Estimate");
   if(pBackground == NULL)
   {
      pBackground = static_cast<RasterElement*>(
         Service<ModelServices>()->getElement("Background", TypeConverter::toString<RasterElement>(), pData));
   }
   if(pBackground == NULL)
   {
      pBackground = RasterUtilities::createRasterElement("Background", pDesc->getRowCount(), pDesc->getColumnCount(),
         pDesc->getDataType(), true, pData);
      VERIFY(pBackground);
      // initialize to zero
      initial = true;
      if(pView != NULL)
      {
         Layer *pBackgroundLayer = pView->createLayer(THRESHOLD, pBackground);
         if(pBackgroundLayer == NULL)
         {
            progress.report("Unable to create a threshold layer for the background estimate", 0, WARNING, true);
         }
      }
   }

   // Update the running averages
   FactoryResource<DataRequest> pRequest;
   pRequest->setInterleaveFormat(BSQ);
   pRequest->setBands(frame, frame, 1);
   DataAccessor dataAcc = pData->getDataAccessor(pRequest.release());
   if(!dataAcc.isValid())
   {
      progress.report("Unable to access original data", 0, ERRORS, true);
      return false;
   }
   FactoryResource<DataRequest> pBackgroundRequest;
   pBackgroundRequest->setWritable(true);
   DataAccessor backgroundAcc = pBackground->getDataAccessor(pBackgroundRequest.release());
   if(!backgroundAcc.isValid())
   {
      progress.report("Unable to access background estimate", 0, ERRORS, true);
      return false;
   }

   for(unsigned int row = 0; row < pDesc->getRowCount(); ++row)
   {
      VERIFY(dataAcc.isValid() && backgroundAcc.isValid());
      progress.report("Updating background estimate", 100 * row / pDesc->getRowCount(), NORMAL);
      for(unsigned int column = 0; column < pDesc->getColumnCount(); ++column)
      {
         if(initial)
         {
            switchOnEncoding(pDesc->getDataType(), copy, dataAcc->getColumn(), backgroundAcc->getColumn());
         }
         else
         {
            switchOnEncoding(pDesc->getDataType(), updateEstimate, dataAcc->getColumn(), backgroundAcc->getColumn(), alpha);
         }
         dataAcc->nextColumn();
         backgroundAcc->nextColumn();
      }
      dataAcc->nextRow();
      backgroundAcc->nextRow();
   }

   progress.upALevel();
   return true;
}