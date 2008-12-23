/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "ColorSpaceConversionShell.h"
#include "GcpLayer.h"
#include "GcpList.h"
#include "ImProcVersion.h"
#include "LayerList.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "ProgressTracker.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterLayer.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "Statistics.h"
#include "switchOnEncoding.h"
#include "Undo.h"

#define EPSILON 0.000001

ColorSpaceConversionShell::ColorSpaceConversionShell() :
   mpSourceView(NULL),
   mAbortFlag(false),
   mScaleData(true)
{
   setCopyright(IMPROC_COPYRIGHT);
   setVersion(IMPROC_VERSION_NUMBER);
   setProductionStatus(IMPROC_IS_PRODUCTION_RELEASE);
   setSubtype("Color Conversion");
   setAbortSupported(true);
}

ColorSpaceConversionShell::~ColorSpaceConversionShell()
{
}

bool ColorSpaceConversionShell::getInputSpecification(PlugInArgList*& pInArgList)
{
   VERIFY(pInArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pInArgList->addArg<Progress>(ProgressArg(), NULL));
   VERIFY(pInArgList->addArg<RasterElement>(DataElementArg()));
   VERIFY(pInArgList->addArg<SpatialDataView>(ViewArg(), NULL));
   VERIFY(pInArgList->addArg<std::string>("Result Name"));
   return true;
}

bool ColorSpaceConversionShell::getOutputSpecification(PlugInArgList*& pOutArgList)
{
   VERIFY(pOutArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pOutArgList->addArg<RasterElement>("Data Element"));
   VERIFY(pOutArgList->addArg<SpatialDataView>("View"));
   return true;
}

bool ColorSpaceConversionShell::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (pInArgList == NULL || pOutArgList == NULL)
   {
      return false;
   }
   if (!extractInputArgs(pInArgList))
   {
      return false;
   }

   mProgress.report("Begin color space conversion.", 1, NORMAL);

   EncodingType resultType = mInput.mpDescriptor->getDataType();
   if (resultType == INT4SCOMPLEX || resultType == FLT8COMPLEX)
   {
      mProgress.report("Complex data is not supported.", 0, ERRORS, true);
      return false;
   }
   if (isBatch())
   {
      mProgress.report("Batch mode is not supported.", 0, ERRORS, true);
      return false;
   }
   { // scope the lifetime
      RasterElement *pResult = static_cast<RasterElement*>(
         Service<ModelServices>()->getElement(mResultName, TypeConverter::toString<RasterElement>(), NULL));
      if (pResult != NULL)
      {
         Service<ModelServices>()->destroyElement(pResult);
      }
   }
   ModelResource<RasterElement> pResult(RasterUtilities::createRasterElement(mResultName,
      mInput.mpDescriptor->getRowCount(), mInput.mpDescriptor->getColumnCount(), 3, FLT8BYTES, BIP,
      mInput.mpDescriptor->getProcessingLocation() == IN_MEMORY));
   mInput.mpResult = pResult.get();
   if (mInput.mpResult == NULL)
   {
      mProgress.report("Unable to create result data set.", 0, ERRORS, true);
      return false;
   }
   mInput.mpAbortFlag = &mAbortFlag;
   mInput.mpCaller = this;
   ColorSpaceConversionShellThreadOutput outputData;
   mta::ProgressObjectReporter reporter("Converting", mProgress.getCurrentProgress());
   mta::MultiThreadedAlgorithm<ColorSpaceConversionShellThreadInput, ColorSpaceConversionShellThreadOutput, ColorSpaceConversionShellThread>     
          alg(Service<ConfigurationSettings>()->getSettingThreadCount(), mInput, outputData, &reporter);
   switch(alg.run())
   {
   case mta::SUCCESS:
      if (!mAbortFlag)
      {
         mProgress.report("Conversion complete.", 100, NORMAL);
         if (!displayResult())
         {
            return false;
         }
         pResult.release();
         mProgress.upALevel();
         return true;
      }
      // fall through
   case mta::ABORT:
      mProgress.report("Conversion aborted.", 0, ABORT, true);
      return false;
   case mta::FAILURE:
      mProgress.report("Conversion failed.", 0, ERRORS, true);
      return false;
   }
   return true; // make the compiler happy
}

bool ColorSpaceConversionShell::extractInputArgs(PlugInArgList* pInArgList)
{
   VERIFY(pInArgList);
   mProgress = ProgressTracker(pInArgList->getPlugInArgValue<Progress>(ProgressArg()),
      "Executing " + getName(), "app", "{5faee035-ff9d-4d51-b555-b3c169dfbefa}");
   if ((mInput.mpRaster = pInArgList->getPlugInArgValue<RasterElement>(DataElementArg())) == NULL)
   {
      mProgress.report("No raster element.", 0, ERRORS, true);
      return false;
   }
   mInput.mpDescriptor = static_cast<const RasterDataDescriptor*>(mInput.mpRaster->getDataDescriptor());
   
   mpSourceView = pInArgList->getPlugInArgValue<SpatialDataView>(ViewArg());
   if (mpSourceView == NULL)
   {
      mProgress.report("No view specified.", 0, ERRORS, true);
      return false;
   }
   RasterLayer* pLayer = static_cast<RasterLayer*>(mpSourceView->getLayerList()->getLayer(RASTER, mInput.mpRaster));
   if (pLayer == NULL)
   {
      mProgress.report("The raster cube specified is not displayed in the specified view.", 0, ERRORS, true);
      return false;
   }
   if (pLayer->getDisplayMode() != RGB_MODE)
   {
      mProgress.report("The display mode must be RGB.", 0, ERRORS, true);
      return false;
   }
   mInput.mRedBand = pLayer->getDisplayedBand(RED).getActiveNumber();
   mInput.mGreenBand = pLayer->getDisplayedBand(GREEN).getActiveNumber();
   mInput.mBlueBand = pLayer->getDisplayedBand(BLUE).getActiveNumber();

   if (mScaleData)
   {
      // Normalize the data to (0.0,1.0)
      mInput.mMaxScale = std::max(std::max(
         pLayer->getStatistics(RED)->getMax(), pLayer->getStatistics(GREEN)->getMax()), pLayer->getStatistics(BLUE)->getMax());
   }
   else
   {
      mInput.mMaxScale = 1.0;
   }

   pInArgList->getPlugInArgValue("Result Name", mResultName);
   if (mResultName.empty())
   {
      mResultName = mInput.mpRaster->getName() + ":" + getName();
   }
   return true;
}

bool ColorSpaceConversionShell::displayResult()
{
   if (mInput.mpResult == NULL)
   {
      return false;
   }
   SpatialDataWindow* pWindow = static_cast<SpatialDataWindow*>(
      Service<DesktopServices>()->createWindow(mInput.mpResult->getName(), SPATIAL_DATA_WINDOW));
   SpatialDataView* pView = (pWindow == NULL) ? NULL : pWindow->getSpatialDataView();
   if (pView == NULL)
   {
      mProgress.report("Unable to create view.", 0, ERRORS, true);
      return false;
   }
   pView->setPrimaryRasterElement(mInput.mpResult);
   
   UndoLock lock(pView);
   RasterLayer* pLayer = static_cast<RasterLayer*>(pView->createLayer(RASTER, mInput.mpResult));
   if (pLayer == NULL)
   {
      mProgress.report("Unable to create view.", 0, ERRORS, true);
      return false;
   }
   const RasterDataDescriptor* pDesc = static_cast<const RasterDataDescriptor*>(mInput.mpResult->getDataDescriptor());
   VERIFY(pDesc);
   pLayer->setDisplayMode(RGB_MODE);
   pLayer->setDisplayedBand(RED, pDesc->getActiveBand(0));
   pLayer->setDisplayedBand(GREEN, pDesc->getActiveBand(1));
   pLayer->setDisplayedBand(BLUE, pDesc->getActiveBand(2));

   if (mInput.mpRaster->isGeoreferenced())
   {
      ModelResource<GcpList> gcps("Corner Coordinates", mInput.mpResult, TypeConverter::toString<GcpList>());
      if (gcps.get())
      {
         unsigned int lastRow = mInput.mpDescriptor->getRowCount() - 1;
         unsigned int lastCol = mInput.mpDescriptor->getColumnCount() - 1;
         std::list<GcpPoint> points;
         GcpPoint ul;
         ul.mPixel = LocationType(0, 0);
         ul.mCoordinate = mInput.mpRaster->convertPixelToGeocoord(ul.mPixel);
         points.push_back(ul);
         GcpPoint ur;
         ur.mPixel = LocationType(lastCol, 0);
         ur.mCoordinate = mInput.mpRaster->convertPixelToGeocoord(ur.mPixel);
         points.push_back(ur);
         GcpPoint ll;
         ll.mPixel = LocationType(0, lastRow);
         ll.mCoordinate = mInput.mpRaster->convertPixelToGeocoord(ll.mPixel);
         points.push_back(ll);
         GcpPoint lr;
         lr.mPixel = LocationType(lastCol, lastRow);
         lr.mCoordinate = mInput.mpRaster->convertPixelToGeocoord(lr.mPixel);
         points.push_back(lr);
         GcpPoint center;
         center.mPixel = LocationType(lastCol / 2, lastRow / 2);
         center.mCoordinate = mInput.mpRaster->convertPixelToGeocoord(center.mPixel);
         points.push_back(center);
         gcps->addPoints(points);
         GcpLayer *pGcpLayer = static_cast<GcpLayer*>(pView->createLayer(GCP_LAYER, gcps.release()));
         if (pGcpLayer == NULL)
         {
            mProgress.report("Unable to create corner coordinates.", 0, WARNING, true);
         }
      }
      else
      {
         mProgress.report("Unable to create corner coordinates.", 0, WARNING, true);
      }
   }
   else
   {
      std::vector<Layer*> layers;
      mpSourceView->getLayerList()->getLayers(GCP_LAYER, layers);
      for (std::vector<Layer*>::iterator layer = layers.begin(); layer != layers.end(); ++layer)
      {
         Layer* pLayer = *layer;
         if (pLayer != NULL && pLayer->getLayerType() == GCP_LAYER && pLayer->getName() == "Corner Coordinates")
         {
            Layer* pNewLayer = pLayer->copy("Corner Coordinates", true, mInput.mpResult);
            if (pNewLayer == NULL)
            {
               mProgress.report("Unable to create corner coordinates.", 0, WARNING, true);
            }
            else
            {
               pView->addLayer(pNewLayer);
            }
            break;
         }
      }
   }

   return true;
}

ColorSpaceConversionShell::ColorSpaceConversionShellThread::ColorSpaceConversionShellThread(
   const ColorSpaceConversionShellThreadInput &input, int threadCount, int threadIndex, mta::ThreadReporter &reporter) :
               mta::AlgorithmThread(threadIndex, reporter),
               mInput(input),
               mRowRange(getThreadRange(threadCount, input.mpDescriptor->getRowCount()))
{
}

void ColorSpaceConversionShell::ColorSpaceConversionShellThread::run()
{
   EncodingType encoding = mInput.mpDescriptor->getDataType();
   int numCols = mInput.mpDescriptor->getColumnCount();

   if (mInput.mpResult == NULL)
   {
      getReporter().reportError("No result data element.");
      return;
   }

   const RasterDataDescriptor* pResultDescriptor = static_cast<const RasterDataDescriptor*>(
      mInput.mpResult->getDataDescriptor());

   FactoryResource<DataRequest> pResultRequest;
   pResultRequest->setInterleaveFormat(BIP);
   pResultRequest->setRows(pResultDescriptor->getActiveRow(mRowRange.mFirst),
      pResultDescriptor->getActiveRow(mRowRange.mLast));
   pResultRequest->setColumns(pResultDescriptor->getActiveColumn(0),
      pResultDescriptor->getActiveColumn(numCols - 1));
   pResultRequest->setWritable(true);
   DataAccessor resultAccessor = mInput.mpResult->getDataAccessor(pResultRequest.release());
   if (!resultAccessor.isValid())
   {
      getReporter().reportError("Invalid data access.");
      return;
   }

   int startRow = mRowRange.mFirst;
   int stopRow = mRowRange.mLast;

   FactoryResource<DataRequest> pRequest;
   pRequest->setInterleaveFormat(BIP);
   pRequest->setRows(mInput.mpDescriptor->getActiveRow(startRow), mInput.mpDescriptor->getActiveRow(stopRow));
   pRequest->setColumns(mInput.mpDescriptor->getActiveColumn(0), mInput.mpDescriptor->getActiveColumn(numCols - 1));
   DataAccessor accessor = mInput.mpRaster->getDataAccessor(pRequest.release());
   if (!accessor.isValid())
   {
      getReporter().reportError("Invalid data access.");
      return;
   }

   int oldPercentDone = 0;
   for (int row_index = startRow; row_index <= stopRow; row_index++)
   {
      int percentDone = mRowRange.computePercent(row_index);
      if (percentDone > oldPercentDone)
      {
         oldPercentDone = percentDone;
         getReporter().reportProgress(getThreadIndex(), percentDone);
      }
      if (mInput.mpAbortFlag != NULL && *mInput.mpAbortFlag)
      {
         getReporter().reportProgress(getThreadIndex(), 100);
         break;
      }

      for (int col_index = 0; col_index < numCols; col_index++)
      {
         if (!accessor.isValid() || !resultAccessor.isValid())
         {
            getReporter().reportError("Invalid data access.");
            return;
         }

         switchOnEncoding(encoding, convertFunctor, accessor->getColumn(), reinterpret_cast<double*>(resultAccessor->getColumn()));

         accessor->nextColumn();
         resultAccessor->nextColumn();
      }
      accessor->nextRow();
      resultAccessor->nextRow();
   }
   getReporter().reportCompletion(getThreadIndex());
}

template<typename T>
void ColorSpaceConversionShell::ColorSpaceConversionShellThread::convertFunctor(const T* pData, double* pResultData)
{
   double pInput[3];
   double pOutput[3];
   pInput[0] = pData[mInput.mRedBand] / mInput.mMaxScale;
   pInput[1] = pData[mInput.mGreenBand] / mInput.mMaxScale;
   pInput[2] = pData[mInput.mBlueBand] / mInput.mMaxScale;
   double maxComponent = std::max(std::max(pInput[0], pInput[1]), pInput[2]);
   double minComponent = std::min(std::min(pInput[0], pInput[1]), pInput[2]);
   mInput.mpCaller->colorspaceConvert(pOutput, pInput, maxComponent, minComponent);

   pResultData[0] = pOutput[0];
   pResultData[1] = pOutput[1];
   pResultData[2] = pOutput[2];
}

bool ColorSpaceConversionShell::ColorSpaceConversionShellThreadOutput::compileOverallResults(
   const std::vector<ColorSpaceConversionShellThread*>& threads)
{
   return true;
}