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
#include "ThresholdLayer.h"
#include <gsl/gsl_blas.h>
#include <gsl/gsl_matrix.h>

REGISTER_PLUGIN_BASIC(Tracking, Rx);

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
      success &= covar->getInArgList().setPlugInArgValue(DataElementArg(), pElement);
      if (isBatch())
      {
         success &= covar->getInArgList().setPlugInArgValue("AOI", pAoi);
      }
      success &= covar->execute();
      pCov = static_cast<RasterElement*>(
         Service<ModelServices>()->getElement("Covariance Matrix", TypeConverter::toString<RasterElement>(), pElement));
      success &= pCov != NULL;
      if (!success)
      {
         progress.report("Unable to calculate covariance.", 0, ERRORS, true);
         return false;
      }
   }
   const RasterDataDescriptor* pDesc = static_cast<const RasterDataDescriptor*>(pElement->getDataDescriptor());
   const BitMask* pBitmask = (pAoi == NULL) ? NULL : pAoi->getSelectedPoints();
   BitMaskIterator iter(pBitmask, pElement);
   FactoryResource<DataRequest> pReq;
   pReq->setInterleaveFormat(BIP);
   pReq->setRows(pDesc->getActiveRow(iter.getBoundingBoxStartRow()), pDesc->getActiveRow(iter.getBoundingBoxEndRow()));
   pReq->setColumns(pDesc->getActiveColumn(iter.getBoundingBoxStartColumn()), pDesc->getActiveColumn(iter.getBoundingBoxEndColumn()));
   DataAccessor acc(pElement->getDataAccessor(pReq.release()));

   ModelResource<RasterElement> pResult(
      RasterUtilities::createRasterElement("Anomalies", iter.getNumSelectedRows(), iter.getNumSelectedColumns(), FLT8BYTES, true, pElement));
   FactoryResource<DataRequest> pResReq;
   pResReq->setWritable(true);
   DataAccessor resacc(pResult->getDataAccessor(pResReq.release()));
   if (!acc.isValid() || !resacc.isValid())
   {
      progress.report("Unable to access data.", 0, ERRORS, true);
      return false;
   }
   { // scope temp matrices
      int bands = pDesc->getBandCount();
      EncodingType encoding = pDesc->getDataType();
      gsl_matrix covMat = {bands, bands, bands, reinterpret_cast<double*>(pCov->getRawData()), NULL, 0};
      gsl_matrix* pPixelMat = gsl_matrix_alloc(bands, 1);
      gsl_matrix* pTemp = gsl_matrix_alloc(1, bands);
      gsl_matrix* pMuMat = gsl_matrix_calloc(bands, 1);

      for (int row = iter.getBoundingBoxStartRow(); iter != iter.end(); ++iter)
      {
         LocationType loc;
         iter.getPixelLocation(loc);
         if (loc.mY > iter.getBoundingBoxEndRow()) // work around a bug
         {
            break;
         }
         if (loc.mY > row)
         {
            row = static_cast<int>(loc.mY);
            progress.report("Calculating means", row * 50 / iter.getNumSelectedRows(), NORMAL);
         }
         acc->toPixel(static_cast<int>(loc.mY), static_cast<int>(loc.mX));
         VERIFY(acc.isValid());
         for (int band = 0; band < bands; ++band)
         {
            double val = Service<ModelServices>()->getDataValue(encoding, acc->getColumn(), band);
            gsl_matrix_set(pPixelMat, band, 0, val);
         }
         gsl_matrix_add(pMuMat, pPixelMat);
      }
      gsl_matrix_scale(pMuMat, 1.0 / iter.getCount());
      iter.firstPixel();

      for (int row = iter.getBoundingBoxStartRow(); iter != iter.end(); ++iter)
      {
         LocationType loc;
         iter.getPixelLocation(loc);
         if (loc.mY > iter.getBoundingBoxEndRow()) // work around a bug
         {
            break;
         }
         if (loc.mY > row)
         {
            row = static_cast<int>(loc.mY);
            progress.report("Calculating RX", row * 49 / iter.getNumSelectedRows() + 50, NORMAL);
         }
         acc->toPixel(static_cast<int>(loc.mY), static_cast<int>(loc.mX));
         resacc->toPixel(static_cast<int>(loc.mY) - iter.getBoundingBoxStartRow(), static_cast<int>(loc.mX) - iter.getBoundingBoxStartColumn());
         VERIFY(acc.isValid() && resacc.isValid());

         for (int band = 0; band < bands; ++band)
         {
            double val = Service<ModelServices>()->getDataValue(encoding, acc->getColumn(), band);
            gsl_matrix_set(pPixelMat, band, 0, val);
         }
         gsl_matrix_sub(pPixelMat, pMuMat);
         gsl_matrix resMat = {1, 1, 1, reinterpret_cast<double*>(resacc->getColumn()), NULL, 0};
         gsl_matrix_set_zero(pTemp);
         gsl_blas_dgemm(CblasTrans, CblasNoTrans, 1.0, pPixelMat, &covMat, 0.0, pTemp);
         gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, pTemp, pPixelMat, 0.0, &resMat);
      }
      gsl_matrix_free(pTemp);
      gsl_matrix_free(pPixelMat);
   }
   if (!isBatch())
   {
      ThresholdLayer* pLayer = static_cast<ThresholdLayer*>(pView->createLayer(THRESHOLD, pResult.get()));
      pLayer->setXOffset(iter.getBoundingBoxStartColumn());
      pLayer->setYOffset(iter.getBoundingBoxStartRow());
      pLayer->setPassArea(UPPER);
      pLayer->setRegionUnits(STD_DEV);
      pLayer->setFirstThreshold(pLayer->convertThreshold(STD_DEV, 2.0, RAW_VALUE));
   }
   if (pOutArgList != NULL)
   {
      pOutArgList->setPlugInArgValue<RasterElement>("Results", pResult.get());
   }
   pResult.release();

   progress.report("Complete", 100, NORMAL);
   progress.upALevel();
   return true;
}