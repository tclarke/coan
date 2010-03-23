/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AoiElement.h"
#include "AppVerify.h"
#include "BitMaskIterator.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "ProgressTracker.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "Rx.h"
#include <opencv/cv.h>

REGISTER_PLUGIN_BASIC(Tracking, Rx);

namespace
{
int opticksToCvType(EncodingType t)
{
   switch(t)
   {
   case INT1SBYTE:
      return CV_8S;
   case INT1UBYTE:
      return CV_8U;
   case INT2SBYTES:
      return CV_16S;
   case INT2UBYTES:
      return CV_16U;
   case INT4SBYTES:
      return CV_32S;
   case FLT4BYTES:
      return CV_32F;
   case FLT8BYTES:
      return CV_64F;
   };
   return -1;
}
}

Rx::Rx()
{
   setName("Rx");
   setDescriptorId("{55f85fb3-1a65-4686-9fdf-2c759383fbcb}");
   setSubtype("Anomaly Detection");
   setMenuLocation("[Tracking]/RX");
}

Rx::~Rx()
{
}

bool Rx::getInputSpecification(PlugInArgList*& pArgList)
{
   VERIFY(pArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pArgList->addArg<Progress>(ProgressArg(), NULL));
   VERIFY(pArgList->addArg<RasterElement>(DataElementArg()));
   VERIFY(pArgList->addArg<SpatialDataView>(ViewArg()));
   VERIFY(pArgList->addArg<AoiElement>("AOI", NULL));
   return true;
}

bool Rx::getOutputSpecification(PlugInArgList*& pArgList)
{
   VERIFY(pArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pArgList->addArg<RasterElement>("Results"));
   return true;
}

bool Rx::execute(PlugInArgList *pInArgList, PlugInArgList *pOutArgList)
{
   VERIFY(pInArgList);
   ProgressTracker progress(pInArgList->getPlugInArgValue<Progress>(ProgressArg()), "Executing RX.", "COAN", "{f5a21b68-013b-4d32-9923-b266e5311752}");

   RasterElement* pElement = pInArgList->getPlugInArgValue<RasterElement>(DataElementArg());
   SpatialDataView* pView = pInArgList->getPlugInArgValue<SpatialDataView>(ViewArg());
   AoiElement* pAoi = pInArgList->getPlugInArgValue<AoiElement>("AOI");

   if (pElement == NULL)
   {
      progress.report("No element specified.", 0, ERRORS, true);
      return false;
   }
   RasterElement* pCov = NULL;
   { // scope
      bool success = true;
      ExecutableResource covar("Covariance", std::string(), progress.getCurrentProgress(), isBatch());
      bool cinv = false;
      success &= covar->getInArgList().setPlugInArgValue(DataElementArg(), pElement);
      success &= covar->getInArgList().setPlugInArgValue("ComputeInverse", &cinv);
      if (isBatch())
      {
         success &= covar->getInArgList().setPlugInArgValue("AOI", pAoi);
      }
      success &= covar->execute();
      pCov = covar->getOutArgList().getPlugInArgValue<RasterElement>("Covariance Matrix");
      success &= pCov != NULL;
      if (!success)
      {
         progress.report("Unable to calculate covariance.", 0, ERRORS, true);
         return false;
      }
   }
   double* pCovData = reinterpret_cast<double*>(pCov->getRawData());

   const RasterDataDescriptor* pDesc = static_cast<const RasterDataDescriptor*>(pElement->getDataDescriptor());
   BitMaskIterator iter((pAoi == NULL) ? NULL : pAoi->getSelectedPoints(), pElement);
   FactoryResource<DataRequest> pReq;
   pReq->setInterleaveFormat(BIP);
   pReq->setRows(pDesc->getActiveRow(iter.getBoundingBoxStartRow()), pDesc->getActiveRow(iter.getBoundingBoxEndRow()));
   pReq->setColumns(pDesc->getActiveColumn(iter.getBoundingBoxStartColumn()), pDesc->getActiveColumn(iter.getBoundingBoxEndColumn()));
   DataAccessor acc(pElement->getDataAccessor(pReq.release()));

   ModelResource<RasterElement> pResult(
      RasterUtilities::createRasterElement("Anomalies", iter.getNumRows(), iter.getNumColumns(), FLT8BYTES, true, pElement));
   FactoryResource<DataRequest> pResReq;
   pResReq->setWritable(true);
   DataAccessor resacc(pElement->getDataAccessor(pResReq.release()));
   if (!acc.isValid() || !resacc.isValid())
   {
      progress.report("Unable to access data.", 0, ERRORS, true);
      return false;
   }
   int bands = pDesc->getBandCount();
   int srctype = opticksToCvType(pDesc->getDataType());
   if (srctype == -1)
   {
      progress.report("Type not supported.", 0, ERRORS, true);
      return false;
   }
   cv::Mat cov(bands, bands, CV_64F, pCovData);
   int row = iter.getBoundingBoxStartRow();
   while (iter != iter.end())
   {
      LocationType loc;
      iter.getPixelLocation(loc);
      if (loc.mY > row)
      {
         row = static_cast<int>(loc.mY);
         progress.report("Calculating RX", row * 100 / iter.getNumRows(), NORMAL);
      }
      acc->toPixel(static_cast<int>(loc.mX), static_cast<int>(loc.mY));
      resacc->toPixel(static_cast<int>(loc.mX) - iter.getBoundingBoxStartColumn(), static_cast<int>(loc.mY) - iter.getBoundingBoxStartRow());
      VERIFY(acc.isValid() && resacc.isValid());
      cv::Mat src(bands, 1, srctype, acc->getColumn());
      cv::Mat res(src.t() * cov * src);
      //*reinterpret_cast<double*>(resacc->getColumn()) = res.at<double>(cv::Point(0, 0));
   }

   progress.report("Complete", 100, NORMAL);
   progress.upALevel();
   return true;
}