/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "BitMask.h"
#include "AoiElement.h"
#include "AoiLayer.h"
#include "AppVerify.h"
#include "ConfigurationSettings.h"
#include "ConvolutionFilterShell.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "LayerList.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInResource.h"
#include "RasterDataDescriptor.h"
#include "RasterLayer.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "switchOnEncoding.h"
#include "Undo.h"

#include <ossim/matrix/newmatap.h>
#include <QtCore/QStringList>
#include <QtGui/QInputDialog>
#include <QtGui/QMessageBox>

namespace
{
   template<typename T>
   void assignResult(T *pPixel, double value)
   {
      *pPixel = static_cast<T>(value);
   }
}

struct CheckWithBitMask
{
   CheckWithBitMask(const BitMask *pMask, unsigned int totalSizeX, unsigned int totalSizeY) :
            mpMask(pMask), mTotalX(totalSizeX), mTotalY(totalSizeY)
   {
   }

   bool operator()(int x_index, int y_index) const 
   { 
      return mpMask == NULL ? true : mpMask->getPixel(x_index, y_index); 
   }

   int getNumRows() const
   {
      if(mpMask == NULL)
      {
         return mTotalY;
      }
      int x1 = 0, x2 = 0, y1 = 0, y2 = 0;
      bool outside = mpMask->isOutsideSelected();
      mpMask->getBoundingBox(x1, y1, x2, y2);
      if(outside)
      {
         return mTotalY;
      }
      return (y2 - y1 + 1);
   }

   int getNumColumns() const
   {
      if(mpMask == NULL)
      {
         return mTotalX;
      }
      int x1 = 0, x2 = 0, y1 = 0, y2 = 0;
      bool outside = mpMask->isOutsideSelected();
      mpMask->getBoundingBox(x1, y1, x2, y2);
      if(outside)
      {
         return mTotalX;
      }
      return (x2 - x1 + 1);
   }

   LocationType getOffset() const
   {
      if(mpMask == NULL)
      {
         return LocationType(0, 0);
      }
      int x1 = 0, x2 = 0, y1 = 0, y2 = 0;
      bool outside = mpMask->isOutsideSelected();
      mpMask->getBoundingBox(x1, y1, x2, y2);
      if(outside)
      {
         return LocationType(0, 0);
      }
      return LocationType(x1, y1);
   }

   bool useAllPixels() const 
   { 
      return (mpMask == NULL) || ((mpMask->getCount() == 0) && (mpMask->getPixel(0,0)));
   }

private:
   const BitMask *mpMask;
   unsigned int mTotalX;
   unsigned int mTotalY;
};

ConvolutionFilterShell::ConvolutionFilterShell() : mpRaster(NULL), mpDescriptor(NULL), mBand(0), mpAoi(NULL)
{
   setSubtype("Convolution Filter");
   setAbortSupported(true);
}

ConvolutionFilterShell::~ConvolutionFilterShell()
{
}

bool ConvolutionFilterShell::getInputSpecification(PlugInArgList *&pInArgList)
{
   VERIFY(pInArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pInArgList->addArg<Progress>(ProgressArg(), NULL));
   VERIFY(pInArgList->addArg<RasterElement>(DataElementArg()));
   VERIFY(pInArgList->addArg<AoiElement>("AOI", NULL));
   VERIFY(pInArgList->addArg<SpatialDataView>(ViewArg(), NULL));
   unsigned int defaultBand = 0;
   VERIFY(pInArgList->addArg<unsigned int>("Band Number", defaultBand));
   VERIFY(pInArgList->addArg<std::string>("Result Name"));
   return true;
}

bool ConvolutionFilterShell::getOutputSpecification(PlugInArgList *&pOutArgList)
{
   VERIFY(pOutArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pOutArgList->addArg<RasterElement>("Data Element"));
   return true;
}

bool ConvolutionFilterShell::execute(PlugInArgList *pInArgList, PlugInArgList *pOutArgList)
{
   if(pInArgList == NULL || pOutArgList == NULL)
   {
      return false;
   }
   if(!extractInputArgs(pInArgList))
   {
      return false;
   }
   if(!populateKernel())
   {
      mProgress.report("Invalid kernel.", 0, ERRORS, true);
      return false;
   }
   mProgress.report("Begin convolution matrix execution.", 0, NORMAL);

   // calculate FFT of the data set.
   RasterElement *pFftElement = NULL;
   { // resource scope
      ExecutableResource fftPlugIn("FFT", std::string(), mProgress.getCurrentProgress(), false);
      if(fftPlugIn->getPlugIn() == NULL)
      {
         mProgress.report("FFT Plugin not available.", 0, ERRORS, true);
         return false;
      }
      fftPlugIn->getInArgList().setPlugInArgValue(Executable::DataElementArg(), mpRaster);
      fftPlugIn->getInArgList().setPlugInArgValue("AOI", mpAoi);
      fftPlugIn->getInArgList().setPlugInArgValue(Executable::ViewArg(), pInArgList->getPlugInArgValue<SpatialDataView>(ViewArg()));
      if(!fftPlugIn->execute() || (pFftElement = fftPlugIn->getOutArgList().getPlugInArgValue<RasterElement>("FFT Element")) == NULL)
      {
         mProgress.report("Unable to calculate FFT.", 0, ERRORS, true);
         return false;
      }
   }
   const RasterDataDescriptor *pFftDesc = static_cast<const RasterDataDescriptor*>(pFftElement->getDataDescriptor());

   // calculate FFT of the kernel and 0 extend
   mKernel.ReSize(pFftDesc->getRowCount(), pFftDesc->getColumnCount());
   mKernelImag.ReSize(pFftDesc->getRowCount(), pFftDesc->getColumnCount());
   NEWMAT::FFT2(mKernel, mKernelImag, mKernel, mKernelImag);

   // multiply and combine
   FactoryResource<DataRequest> pRequest;
   pRequest->setWritable(true);
   pRequest->setInterleaveFormat(BSQ);
   DataAccessor fftAcc = pFftElement->getDataAccessor(pRequest.release());
   for(unsigned int row = 0; row < pFftDesc->getRowCount(); row++)
   {
      for(unsigned int col = 0; col < pFftDesc->getColumnCount(); col++)
      {
         VERIFY(fftAcc.isValid());
         FloatComplex *pFftPoint = reinterpret_cast<FloatComplex*>(fftAcc->getColumn());
         std::complex<double> fftPoint(pFftPoint->mReal, pFftPoint->mImaginary);
         std::complex<double> kernelPoint(mKernel.element(row, col), mKernelImag.element(row, col));
         fftPoint *= kernelPoint;
         pFftPoint->mReal = fftPoint.real();
         pFftPoint->mImaginary = fftPoint.imag();
         fftAcc->nextColumn();
      }
      fftAcc->nextRow();
   }

   // Invert the FFT
   { // resource scope
      ExecutableResource fftPlugIn("FFT", std::string(), mProgress.getCurrentProgress(), false);
      if(fftPlugIn->getPlugIn() == NULL)
      {
         mProgress.report("FFT Plugin not available.", 0, ERRORS, true);
         return false;
      }
      fftPlugIn->getInArgList().setPlugInArgValue(Executable::DataElementArg(), pFftElement);
      if(!fftPlugIn->execute() || (pFftElement = fftPlugIn->getOutArgList().getPlugInArgValue<RasterElement>("FFT Element")) == NULL)
      {
         mProgress.report("Unable to calculate FFT.", 0, ERRORS, true);
         return false;
      }
      pOutArgList->setPlugInArgValue("Data Element", pFftElement);
   }

   mProgress.upALevel();
   return true;
}

bool ConvolutionFilterShell::extractInputArgs(PlugInArgList *pInArgList)
{
   VERIFY(pInArgList);
   mProgress = ProgressTracker(pInArgList->getPlugInArgValue<Progress>(ProgressArg()),
      "Executing " + getName(), "app", "{64097F31-84D0-41bf-BBAF-F60DAF212836}");
   if((mpRaster = pInArgList->getPlugInArgValue<RasterElement>(DataElementArg())) == NULL)
   {
      mProgress.report("No raster element.", 0, ERRORS, true);
      return false;
   }
   mpDescriptor = static_cast<const RasterDataDescriptor*>(mpRaster->getDataDescriptor());
   mpAoi = pInArgList->getPlugInArgValue<AoiElement>("AOI");
   if(!pInArgList->getPlugInArgValue("Band Number", mBand))
   {
      mProgress.report("Error getting band number.", 0, ERRORS, true);
      return false;
   }
   pInArgList->getPlugInArgValue("Result Name", mResultName);
   if(mResultName.empty())
   {
      mResultName = mpRaster->getDisplayName() + " Convolved";
      if(!isBatch())
      {
         bool ok = true;
         mResultName = QInputDialog::getText(Service<DesktopServices>()->getMainWidget(), "Output data name",
            "Choose a name for the output data cube", QLineEdit::Normal, QString::fromStdString(mResultName), &ok).toStdString();
         if(!ok)
         {
            mProgress.report("User cancelled " + getName(), 0, ABORT, true);
         }
      }
   }
   return true;
}