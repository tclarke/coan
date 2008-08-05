/**
* The information in this file is
* Copyright(c) 2007 Ball Aerospace & Technologies Corporation
* and is subject to the terms and conditions of the
* Opticks Limited Open Source License Agreement Version 1.0
* The license text is available from https://www.ballforge.net/
* 
* This material (software and user documentation) is subject to export
* controls imposed by the United States Export Administration Act of 1979,
* as amended and the International Traffic In Arms Regulation (ITAR),
* 22 CFR 120-130
*/

#include "AoiElement.h"
#include "AoiLayer.h"
#include "AppVerify.h"
#include "BitMask.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DesktopServices.h"
#include "CalculateLooks.h"
#include "LayerList.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "SpatialDataView.h"
#include "Statistics.h"

#include <QtCore/QStringList>
#include <QtGui/QInputDialog>

CalculateLooks::CalculateLooks() : mpRaster(NULL), mpDescriptor(NULL), mpAoi(NULL), mpAccessor(NULL, NULL), mLooks(0)
{
   setName("Calculate Looks");
   setDescriptorId("{6B13D8E6-B26B-41e6-90C6-724442ED46FE}");
   setDescription("Calculate the estimated number of looks in a single look image.");
   setMenuLocation("[SAR]/General/Calculate Looks");
   setSubtype("SAR");
   setProductionStatus(false);
}

CalculateLooks::~CalculateLooks()
{
}

bool CalculateLooks::getInputSpecification(PlugInArgList *&pInArgList)
{
   VERIFY(pInArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pInArgList->addArg<Progress>(ProgressArg(), NULL));
   VERIFY(pInArgList->addArg<RasterElement>(DataElementArg()));
   VERIFY(pInArgList->addArg<AoiElement>("AOI", NULL));
   VERIFY(pInArgList->addArg<SpatialDataView>(ViewArg(), NULL));
   unsigned int defaultBand = 0;
   VERIFY(pInArgList->addArg<unsigned int>("Band Number", defaultBand));
   return true;
}

bool CalculateLooks::getOutputSpecification(PlugInArgList *&pOutArgList)
{
   VERIFY(pOutArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pOutArgList->addArg<double>("Looks"));
   return true;
}

bool CalculateLooks::execute(PlugInArgList *pInArgList, PlugInArgList *pOutArgList)
{
   if(pInArgList == NULL || pOutArgList == NULL)
   {
      return false;
   }
   if(!extractInputArgs(pInArgList))
   {
      return false;
   }
   if(mpAoi == NULL)
   {
      // If we have a Statistics object and want looks over the entire image
      // we've already calculated this so make use of the data.
      Statistics *pStats = mpRaster->getStatistics(mpDescriptor->getActiveBand(mBand));
      if(pStats != NULL)
      {
         double mean = pStats->getAverage(COMPLEX_MAGNITUDE);
         double stddev = pStats->getStandardDeviation(COMPLEX_MAGNITUDE);
         mLooks = (mean / stddev) * (mean / stddev);
         if(!display(pOutArgList))
         {
            return false;
         }
         mProgress.upALevel();
         return true;
      }
   }
   mStartRow = mStartColumn = 0;
   mEndRow = mpDescriptor->getRowCount() - 1;
   mEndColumn = mpDescriptor->getColumnCount() - 1;
   const BitMask *pBitMask = mpAoi->getSelectedPoints();
   if(pBitMask != NULL && !pBitMask->isOutsideSelected())
   {
      int x1, y1, x2, y2;
      pBitMask->getBoundingBox(x1, y1, x2, y2);
      mStartRow = std::max(std::min(y1, y2), static_cast<int>(mStartRow));
      mEndRow = std::min(std::max(y1, y2), static_cast<int>(mEndRow));
      mStartColumn = std::max(std::min(x1, x2), static_cast<int>(mStartColumn));
      mEndColumn = std::min(std::max(x1, x2), static_cast<int>(mEndColumn));
   }
   mRow = mStartRow;
   mColumn = mStartColumn;
   FactoryResource<DataRequest> pRequest;
   pRequest->setInterleaveFormat(BSQ);
   pRequest->setBands(mpDescriptor->getActiveBand(mBand), mpDescriptor->getActiveBand(mBand), 1);
   pRequest->setRows(mpDescriptor->getActiveRow(mStartRow), mpDescriptor->getActiveRow(mEndRow), 1);
   pRequest->setColumns(mpDescriptor->getActiveColumn(mStartColumn), mpDescriptor->getActiveColumn(mEndColumn), 1);
   mpAccessor = mpRaster->getDataAccessor(pRequest.release());
   while(pBitMask != NULL && !pBitMask->getPixel(mColumn, mRow))
   {
      mColumn++;
      if(mColumn > mEndColumn)
      {
         mColumn = mStartColumn;
         mRow++;
      }
   }
   mpAccessor->toPixel(mRow, mColumn);

   if(!initialize() || !process() || !display(pOutArgList))
   {
      return false;
   }
   mProgress.upALevel();
   return true;
}

bool CalculateLooks::extractInputArgs(PlugInArgList *pInArgList)
{
   VERIFY(pInArgList);
   mProgress = ProgressTracker(pInArgList->getPlugInArgValue<Progress>(ProgressArg()),
      "Executing " + getName(), "app", "{64097F31-84D0-41bf-BBAF-F60DAF212836}");
   if((mpRaster = pInArgList->getPlugInArgValue<RasterElement>(DataElementArg())) == NULL)
   {
      mProgress.report("No raster element.", 0, ERRORS, true);
      return false;
   }
   mpDescriptor = static_cast<RasterDataDescriptor*>(mpRaster->getDataDescriptor());
   mpAoi = pInArgList->getPlugInArgValue<AoiElement>("AOI");
   if(mpAoi == NULL && !isBatch())
   {
      SpatialDataView *pView = pInArgList->getPlugInArgValue<SpatialDataView>(ViewArg());
      if(pView != NULL)
      {
         std::vector<Layer*> layers;
         pView->getLayerList()->getLayers(AOI_LAYER, layers);
         if(!layers.empty())
         {
            QStringList layerNames;
            layerNames << "<none>";
            for(std::vector<Layer*>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
            {
               layerNames << QString::fromStdString((*layer)->getName());
            }
            bool ok = true;
            std::string selectedLayer = QInputDialog::getItem(Service<DesktopServices>()->getMainWidget(), "Choose an AOI",
               "Select an AOI or <none> to process the entire image", layerNames, 0, false, &ok).toStdString();
            if(!ok)
            {
               mProgress.report("User cancelled " + getName(), 0, ABORT, true);
               return false;
            }
            for(std::vector<Layer*>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
            {
               if((*layer)->getName() == selectedLayer)
               {
                  mpAoi = static_cast<AoiElement*>((*layer)->getDataElement());
                  break;
               }
            }
         }
      }
   }
   if(!pInArgList->getPlugInArgValue("Band Number", mBand))
   {
      mProgress.report("Error getting band number.", 0, ERRORS, true);
      return false;
   }
   return true;
}

bool CalculateLooks::initialize()
{
   mLooks = 1;
   return true;
}

bool CalculateLooks::process()
{
   size_t totalPixels = mpDescriptor->getRowCount() * mpDescriptor->getColumnCount();
   if(mpAoi != NULL)
   {
      totalPixels = mpAoi->getSelectedPoints()->getCount();
   }
   size_t pixel = 0;
   double n = 0, mean = 0, M2 = 0;
   do
   {
      mProgress.report("Calculating looks", pixel++ * 100 / totalPixels, NORMAL);
      double pixelValue = Service<ModelServices>()->getDataValue(mpDescriptor->getDataType(), getCurrentPixel(), 0);
      n++;
      double delta = pixelValue - mean;
      mean += delta / n;
      M2 += delta * (pixelValue - mean);
   }
   while(nextPixel());
   double stddev = sqrt(M2 / (n - 1));
   mLooks = (mean / stddev) * (mean / stddev);
   return true;
}

bool CalculateLooks::display(PlugInArgList *pOutArgList)
{
   if(pOutArgList != NULL)
   {
      if(!pOutArgList->setPlugInArgValue("Looks", &mLooks))
      {
         return false;
      }
   }
   if(!isBatch())
   {
      Service<DesktopServices>()->showMessageBox("Estimated Number of Looks",
         QString("The data contains %1 looks.").arg(mLooks).toStdString(), "Ok");
   }
   return true;
}

bool CalculateLooks::nextPixel()
{
   const BitMask *pBitMask = (mpAoi == NULL) ? NULL : mpAoi->getSelectedPoints();
   do
   {
      mColumn++;
      if(mColumn > mEndColumn)
      {
         mColumn = mStartColumn;
         mRow++;
      }
   }
   while(mRow <= mEndRow && pBitMask != NULL && !pBitMask->getPixel(mColumn, mRow));
   mpAccessor->toPixel(mRow, mColumn);
   return mpAccessor->isValid();
}

DimensionDescriptor CalculateLooks::getCurrentRow() const
{
   return mpDescriptor->getActiveRow(mRow);
}

DimensionDescriptor CalculateLooks::getCurrentColumn() const
{
   return mpDescriptor->getActiveColumn(mColumn);
}

DimensionDescriptor CalculateLooks::getCurrentBand() const
{
   return mpDescriptor->getActiveBand(mBand);
}

void *CalculateLooks::getCurrentPixel()
{
   if(mpAccessor->isValid())
   {
      return mpAccessor->getColumn();
   }
   return NULL;
}