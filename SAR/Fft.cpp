/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AoiElement.h"
#include "AoiLayer.h"
#include "AppVerify.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DesktopServices.h"
#include "Fft.h"
#include "LayerList.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "switchOnEncoding.h"

#include <ossim/matrix/newmat.h>
#include <ossim/matrix/newmatap.h>
#include <QtCore/QStringList>
#include <QtGui/QInputDialog>
#include <QtGui/QMessageBox>

namespace
{
   template<typename T>
   void copyData(const T *pSrc, NEWMAT::Real &U, NEWMAT::Real &V)
   {
      U = *pSrc;
      V = 0.0;
   }
   template<>
   void copyData<IntegerComplex>(const IntegerComplex *pSrc, NEWMAT::Real &U, NEWMAT::Real &V)
   {
      U = pSrc->mReal;
      V = pSrc->mImaginary;
   }
   template<>
   void copyData<FloatComplex>(const FloatComplex *pSrc, NEWMAT::Real &U, NEWMAT::Real &V)
   {
      U = pSrc->mReal;
      V = pSrc->mImaginary;
   }
}

Fft::Fft()
{
   setName("FFT");
   setDescriptorId("{0cf6e784-0eb0-4178-9a16-84ce2dc8f754}");
   setDescription("Calculate the FFT or IFFT of a data set.");
   setMenuLocation("[SAR]/General/FFT");
   setProductionStatus(false);
   setAbortSupported(true);
}

Fft::~Fft()
{
}

bool Fft::getInputSpecification(PlugInArgList *&pInArgList)
{
   VERIFY(pInArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pInArgList->addArg<Progress>(ProgressArg(), NULL));
   VERIFY(pInArgList->addArg<RasterElement>(DataElementArg()));
   VERIFY(pInArgList->addArg<AoiElement>("AOI", NULL));
   VERIFY(pInArgList->addArg<SpatialDataView>(ViewArg(), NULL));
   unsigned int zero = 0;
   VERIFY(pInArgList->addArg<unsigned int>("Pad Rows", zero));
   VERIFY(pInArgList->addArg<unsigned int>("Pad Columns", zero));
   return true;
}

bool Fft::getOutputSpecification(PlugInArgList *&pOutArgList)
{
   VERIFY(pOutArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pOutArgList->addArg<RasterElement>("FFT Element"));
   return true;
}

bool Fft::execute(PlugInArgList *pInArgList, PlugInArgList *pOutArgList)
{
   if(pInArgList == NULL || pOutArgList == NULL)
   {
      return false;
   }
   if(!extractInputArgs(pInArgList))
   {
      return false;
   }
   RasterDataDescriptor *pDesc = static_cast<RasterDataDescriptor*>(mpRaster->getDataDescriptor());
   RasterElement *pParent = mpRaster;
   std::string outname = "FFT";
   bool inverse = false;
   if(mpRaster->getName() == outname && mpRaster->getParent() != NULL)
   {
      outname = "IFFT";
      pParent = NULL;
      inverse = true;
   }
   mpFft = static_cast<RasterElement*>(Service<ModelServices>()->getElement(outname, TypeConverter::toString<RasterElement>(), pParent));
   if(mpFft != NULL && !isBatch())
   {
      if(QMessageBox::question(Service<DesktopServices>()->getMainWidget(), "Use existing FFT",
         "An FFT already exists for this dataset, would you like to reuse it?",
         QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
      {
         mProgress.report("Reusing existing FFT data.", 100, NORMAL, true);
         mProgress.upALevel();
         return true;
      }
   }
   ModelResource<RasterElement> pFft(RasterUtilities::createRasterElement(outname,
      pDesc->getRowCount() + mPadRows, pDesc->getColumnCount() + mPadColumns, FLT8COMPLEX, true, pParent));
   mpFft = pFft.get();
   if(mpFft == NULL)
   {
      mProgress.report("Unable to create FFT data element.", 0, ERRORS, true);
      return false;
   }

   NEWMAT::Matrix U(pDesc->getRowCount(), pDesc->getColumnCount());
   NEWMAT::Matrix V(pDesc->getRowCount(), pDesc->getColumnCount());

   // populate U and V
   FactoryResource<DataRequest> pRequest;
   if(pDesc->getDataType() == FLT8BYTES)
   {
      pRequest->setRows(DimensionDescriptor(), DimensionDescriptor(), pDesc->getRowCount());
      pRequest->setColumns(DimensionDescriptor(), DimensionDescriptor(), pDesc->getColumnCount());
   }
   pRequest->setBands(pDesc->getActiveBand(0), pDesc->getActiveBand(0), 1);
   pRequest->setInterleaveFormat(BSQ);
   DataAccessor acc = mpRaster->getDataAccessor(pRequest.release());
   if(!acc.isValid())
   {
      mProgress.report("Unable to access an entire band of the source image.", 0, ERRORS, true);
      return false;
   }
   if(pDesc->getDataType() == FLT8BYTES)
   {
      memcpy(U.Store(), acc->getRow(), U.Storage());
   }
   else
   {
      size_t u_idx = 0;
      for(size_t row = 0; row < pDesc->getRowCount(); row++)
      {
         for(size_t col = 0; col < pDesc->getColumnCount(); col++)
         {
            VERIFY(acc.isValid());
            switchOnComplexEncoding(pDesc->getDataType(), copyData, acc->getColumn(), U.Store()[u_idx], V.Store()[u_idx]);
            u_idx++;
            acc->nextColumn();
         }
         acc->nextRow();
      }
   }
   U.ReSize(U.Nrows() + mPadRows, U.Ncols() + mPadColumns);
   V.ReSize(V.Nrows() + mPadRows, V.Ncols() + mPadColumns);

   // calculate...there's an FFT2() function but we'll unroll it here so we get some progress updates
   if(inverse)
   {
      if(!ifft2(U, V))
      {
         return false;
      }
   }
   else
   {
      if(!fft2(U, V))
      {
         return false;
      }
   }

   // copy back result
   pDesc = static_cast<RasterDataDescriptor*>(mpFft->getDataDescriptor());
   pRequest = FactoryResource<DataRequest>();
   pRequest->setBands(pDesc->getActiveBand(0), pDesc->getActiveBand(0), 1);
   pRequest->setInterleaveFormat(BSQ);
   acc = mpFft->getDataAccessor(pRequest.release());
   if(!acc.isValid())
   {
      mProgress.report("Unable to access an entire band of the FFT.", 0, ERRORS, true);
      return false;
   }
   size_t u_idx = 0;
   for(size_t row = 0; row < pDesc->getRowCount(); row++)
   {
      for(size_t col = 0; col < pDesc->getColumnCount(); col++)
      {
         VERIFY(acc.isValid());
         FloatComplex *pPixel = reinterpret_cast<FloatComplex*>(acc->getColumn());
         pPixel->mReal = U.Store()[u_idx];
         pPixel->mImaginary = V.Store()[u_idx];
         u_idx++;
         acc->nextColumn();
      }
      acc->nextRow();
   }

   if(!isBatch())
   {
      SpatialDataWindow *pWindow = static_cast<SpatialDataWindow*>(
         Service<DesktopServices>()->createWindow(mpFft->getName(), SPATIAL_DATA_WINDOW));
      SpatialDataView *pView = (pWindow == NULL) ? NULL : pWindow->getSpatialDataView();
      if(pView == NULL)
      {
         mProgress.report("Unable to create a view for the result.", 0, ERRORS, true);
         return false;
      }
      pView->setPrimaryRasterElement(mpFft);
      pView->createLayer(RASTER, mpFft);
   }

   pOutArgList->setPlugInArgValue("FFT Element", mpFft);

   pFft.release();
   mProgress.report("FFT calculation complete.", 100, NORMAL, true);
   mProgress.upALevel();
   return true;
}

bool Fft::extractInputArgs(PlugInArgList *pInArgList)
{
   VERIFY(pInArgList);
   mProgress = ProgressTracker(pInArgList->getPlugInArgValue<Progress>(ProgressArg()), "Calculating FFT",
      "app", "{9b235442-9cc0-4c5a-80d3-5a05723f0fd7}");
   if((mpRaster = pInArgList->getPlugInArgValue<RasterElement>(DataElementArg())) == NULL)
   {
      mProgress.report("No raster element.", 0, ERRORS, true);
      return false;
   }
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
   pInArgList->getPlugInArgValue("Pad Rows", mPadRows);
   pInArgList->getPlugInArgValue("Pad Columns", mPadColumns);

   return true;
}

bool Fft::fft2(NEWMAT::Matrix &U, NEWMAT::Matrix &V)
{
   int m = U.Nrows();
   int n = U.Ncols();
   if(m != V.Nrows() || n != V.Ncols() || m == 0 || n == 0)
   {
      mProgress.report("Matrix dimensions unequal or zero", 0, ERRORS, true);
      return false;
   }
   NEWMAT::ColumnVector CVR;
   NEWMAT::ColumnVector CVI;
   int curProgress = 0, totalProgress = m+n;
   for(int i = 1; i <= m; ++i)
   {
      mProgress.report("Calculating FFT", 99 * curProgress++ / totalProgress, NORMAL);
      if(isAborted())
      {
         mProgress.report("Aborted", 0, ABORT, true);
         return false;
      }
      NEWMAT::FFT(U.Row(i).t(), V.Row(i).t(), CVR, CVI);
      U.Row(i) = CVR.t();
      V.Row(i) = CVI.t();
   }
   for(int i = 1; i <= n; ++i)
   {
      mProgress.report("Calculating FFT", 99 * curProgress++ / totalProgress, NORMAL);
      if(isAborted())
      {
         mProgress.report("Aborted", 0, ABORT, true);
         return false;
      }
      NEWMAT::FFT(U.Column(i), V.Column(i), CVR, CVI);
      U.Column(i) = CVR;
      V.Column(i) = CVI;
   }
   return true;
}

bool Fft::ifft2(NEWMAT::Matrix &U, NEWMAT::Matrix &V)
{
   V = -V;
   if(!fft2(U,V))
   {
      return false;
   }
   const NEWMAT::Real n = U.Nrows() * U.Ncols();
   U /= n;
   V /= (-n);
   return true;
}