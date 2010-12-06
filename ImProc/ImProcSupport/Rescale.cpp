/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "ImProcVersion.h"
#include "GcpLayer.h"
#include "GcpList.h"
#include "LayerList.h"
#include "PlugInArgList.h"
#include "PlugInRegistration.h"
#include "PlugInManagerServices.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterLayer.h"
#include "RasterUtilities.h"
#include "Rescale.h"
#include "RescaleInputDialog.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "StringUtilitiesMacros.h"
#include "switchOnEncoding.h"
#include "Undo.h"

#include <QtGui/QDialog>

REGISTER_PLUGIN_BASIC(ImProcSupport, Rescale);

namespace StringUtilities
{
BEGIN_ENUM_MAPPING(InterpType)
ADD_ENUM_MAPPING(NEAREST, "Nearest Neighbor", "Nearest Neighbor")
ADD_ENUM_MAPPING(BILINEAR, "Bilinear", "Bilinear")
ADD_ENUM_MAPPING(BICUBIC, "Bicubic", "Bicubic")
END_ENUM_MAPPING()
}

Rescale::Rescale() :
   mpSourceView(NULL),
   mAbortFlag(false)
{
   setName("Rescale");
   setDescription("Spatially rescale/resample a data set.");
   setDescriptorId("{11A0F609-1525-4A5C-9D92-46C8AFFB65A1}");
   setCopyright(IMPROC_COPYRIGHT);
   setVersion(IMPROC_VERSION_NUMBER);
   setProductionStatus(IMPROC_IS_PRODUCTION_RELEASE);
   setAbortSupported(true);
   setMenuLocation("[General Algorithms]/Rescale");
}

Rescale::~Rescale()
{
}

bool Rescale::getInputSpecification(PlugInArgList*& pInArgList)
{
   VERIFY(pInArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pInArgList->addArg<Progress>(ProgressArg(), NULL));
   VERIFY(pInArgList->addArg<RasterElement>(DataElementArg()));
   VERIFY(pInArgList->addArg<SpatialDataView>(ViewArg(), NULL));
   VERIFY(pInArgList->addArg<std::string>("Result Name"));
   VERIFY(pInArgList->addArg<double>("Row Factor"));
   VERIFY(pInArgList->addArg<double>("Column Factor"));
   return true;
}

bool Rescale::getOutputSpecification(PlugInArgList*& pOutArgList)
{
   VERIFY(pOutArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pOutArgList->addArg<RasterElement>("Data Element"));
   VERIFY(pOutArgList->addArg<SpatialDataView>("View"));
   return true;
}

bool Rescale::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (pInArgList == NULL || pOutArgList == NULL)
   {
      return false;
   }
   if (!extractInputArgs(pInArgList))
   {
      return false;
   }

   mProgress.report("Begin rescale conversion.", 1, NORMAL);

   { // scope the lifetime
      RasterElement *pResult = static_cast<RasterElement*>(
         Service<ModelServices>()->getElement(mResultName, TypeConverter::toString<RasterElement>(), NULL));
      if (pResult != NULL)
      {
         Service<ModelServices>()->destroyElement(pResult);
      }
   }
   unsigned int resultRows = static_cast<unsigned int>(mInput.mpDescriptor->getRowCount() * mInput.mRowFactor + 0.5);
   unsigned int resultCols = static_cast<unsigned int>(mInput.mpDescriptor->getColumnCount() * mInput.mColFactor + 0.5);
   ModelResource<RasterElement> pResult(RasterUtilities::createRasterElement(mResultName,
      resultRows, resultCols, mInput.mpDescriptor->getBandCount(), mInput.mpDescriptor->getDataType(),
      mInput.mpDescriptor->getInterleaveFormat(), mInput.mpDescriptor->getProcessingLocation() == IN_MEMORY));
   mInput.mpResult = pResult.get();
   if (mInput.mpResult == NULL)
   {
      mProgress.report("Unable to create result data set.", 0, ERRORS, true);
      return false;
   }
   mInput.mpResultDescriptor = static_cast<const RasterDataDescriptor*>(mInput.mpResult->getDataDescriptor());
   mInput.mpAbortFlag = &mAbortFlag;
   RescaleThreadOutput outputData;
   mta::ProgressObjectReporter reporter("Rescaling", mProgress.getCurrentProgress());
   mta::MultiThreadedAlgorithm<RescaleThreadInput, RescaleThreadOutput, RescaleThread>     
          alg(Service<ConfigurationSettings>()->getSettingThreadCount(), mInput, outputData, &reporter);
   switch(alg.run())
   {
   case mta::SUCCESS:
      if (!mAbortFlag)
      {
         mProgress.report("Rescale complete.", 100, NORMAL);
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
      mProgress.report("Rescale aborted.", 0, ABORT, true);
      return false;
   case mta::FAILURE:
      mProgress.report("Rescale failed.", 0, ERRORS, true);
      return false;
   }
   return true; // make the compiler happy
}

bool Rescale::extractInputArgs(PlugInArgList* pInArgList)
{
   VERIFY(pInArgList);
   mProgress = ProgressTracker(pInArgList->getPlugInArgValue<Progress>(ProgressArg()),
      "Executing " + getName(), "app", "{f51180e1-f7c3-4f9e-80cc-d1cd8f4e622d}");
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

   pInArgList->getPlugInArgValue("Result Name", mResultName);
   if (mResultName.empty())
   {
      mResultName = mInput.mpRaster->getName() + ":" + getName();
   }

   if (!pInArgList->getPlugInArgValue("Row Factor", mInput.mRowFactor) ||
       !pInArgList->getPlugInArgValue("Column Factor", mInput.mColFactor))
   {
      if (isBatch())
      {
         mProgress.report("No scale factors specified.", 0, ERRORS, true);
         return false;
      }
      RescaleInputDialog dlg;
      dlg.setRowFactor(mInput.mRowFactor);
      dlg.setColFactor(mInput.mColFactor);
      dlg.setInterpType(mInput.mInterp);
      if (dlg.exec() != QDialog::Accepted)
      {
         mProgress.report("User aborted.", 0, ABORT, true);
         return false;
      }
      mInput.mRowFactor = dlg.getRowFactor();
      mInput.mColFactor = dlg.getColFactor();
      mInput.mInterp = dlg.getInterpType();
   }

   return true;
}

bool Rescale::displayResult()
{
   if (isBatch())
   {
      return true;
   }
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

   if (mInput.mpRaster->isGeoreferenced())
   {
      ModelResource<GcpList> gcps("Corner Coordinates", mInput.mpResult, TypeConverter::toString<GcpList>());
      if (gcps.get())
      {
         unsigned int lastRow = mInput.mpDescriptor->getRowCount() - 1;
         unsigned int lastCol = mInput.mpDescriptor->getColumnCount() - 1;
         unsigned int lastResRow = mInput.mpResultDescriptor->getRowCount() - 1;
         unsigned int lastResCol = mInput.mpResultDescriptor->getColumnCount() - 1;

         std::list<GcpPoint> points;
         GcpPoint ul;
         ul.mPixel = LocationType(0, 0);
         ul.mCoordinate = mInput.mpRaster->convertPixelToGeocoord(ul.mPixel);
         points.push_back(ul);
         GcpPoint ur;
         ur.mPixel = LocationType(lastCol, 0);
         ur.mCoordinate = mInput.mpRaster->convertPixelToGeocoord(ur.mPixel);
         ur.mPixel = LocationType(lastResCol, 0);
         points.push_back(ur);
         GcpPoint ll;
         ll.mPixel = LocationType(0, lastRow);
         ll.mCoordinate = mInput.mpRaster->convertPixelToGeocoord(ll.mPixel);
         ll.mPixel = LocationType(0, lastResRow);
         points.push_back(ll);
         GcpPoint lr;
         lr.mPixel = LocationType(lastCol, lastRow);
         lr.mCoordinate = mInput.mpRaster->convertPixelToGeocoord(lr.mPixel);
         lr.mPixel = LocationType(lastResCol, lastResRow);
         points.push_back(lr);
         GcpPoint center;
         center.mPixel = LocationType(lastCol / 2, lastRow / 2);
         center.mCoordinate = mInput.mpRaster->convertPixelToGeocoord(center.mPixel);
         center.mPixel = LocationType(lastResCol / 2, lastResRow / 2);
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

   return true;
}

Rescale::RescaleThread::RescaleThread(const RescaleThreadInput &input, int threadCount, int threadIndex, mta::ThreadReporter &reporter) :
               mta::AlgorithmThread(threadIndex, reporter),
               mInput(input),
               mRowRange(getThreadRange(threadCount, input.mpResultDescriptor->getRowCount()))
{
}

void Rescale::RescaleThread::run()
{
   if (mInput.mpResult == NULL)
   {
      getReporter().reportError("No result data element.");
      return;
   }

   EncodingType encoding = mInput.mpDescriptor->getDataType();
   int numCols = mInput.mpResultDescriptor->getColumnCount();

   int oldPercentDone = 0;

   int startRow = mRowRange.mFirst;
   int stopRow = mRowRange.mLast;
   bool isBip = (mInput.mpResultDescriptor->getInterleaveFormat() == BIP);
   unsigned int numBandsInLoop = isBip ? 1 : mInput.mpResultDescriptor->getBandCount();
   unsigned int numBandsPerElement = isBip ? mInput.mpResultDescriptor->getBandCount() : 1;

   for(unsigned int band = 0; band < numBandsInLoop; band++)
   {
      FactoryResource<DataRequest> pResultRequest;
      pResultRequest->setRows(mInput.mpResultDescriptor->getActiveRow(mRowRange.mFirst),
         mInput.mpResultDescriptor->getActiveRow(mRowRange.mLast));
      pResultRequest->setColumns(mInput.mpResultDescriptor->getActiveColumn(0),
         mInput.mpResultDescriptor->getActiveColumn(numCols - 1));
      if (!isBip)
      {
         pResultRequest->setBands(mInput.mpResultDescriptor->getActiveBand(band), mInput.mpResultDescriptor->getActiveBand(band));
      }
      pResultRequest->setWritable(true);
      DataAccessor resultAccessor = mInput.mpResult->getDataAccessor(pResultRequest.release());
      if (!resultAccessor.isValid())
      {
         getReporter().reportError("Invalid data access.");
         return;
      }

      FactoryResource<DataRequest> pRequest;
      if (!isBip)
      {
         pRequest->setBands(mInput.mpResultDescriptor->getActiveBand(band), mInput.mpResultDescriptor->getActiveBand(band));
      }
      DataAccessor accessor = mInput.mpRaster->getDataAccessor(pRequest.release());
      if (!accessor.isValid())
      {
         getReporter().reportError("Invalid data access.");
         return;
      }

      for (int row_index = startRow; row_index <= stopRow; row_index++)
      {
         int percentDone = mRowRange.computePercent(row_index / numBandsInLoop);
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
            if (!resultAccessor.isValid())
            {
               getReporter().reportError("Invalid data access.");
               return;
            }

            switch(mInput.mInterp)
            {
            case NEAREST:
               switchOnEncoding(encoding, nearestNeighbor,
                  resultAccessor->getColumn(), row_index, col_index, accessor, numBandsPerElement);
               break;
            case BILINEAR:
               switchOnEncoding(encoding, bilinear,
                  resultAccessor->getColumn(), row_index, col_index, accessor, numBandsPerElement);
               break;
            case BICUBIC:
               switchOnEncoding(encoding, bicubic,
                  resultAccessor->getColumn(), row_index, col_index, accessor, numBandsPerElement);
               break;
            default:
               getReporter().reportError("Invalid interpolation type.");
               return;
            }

            resultAccessor->nextColumn();
         }
         resultAccessor->nextRow();
      }
   }
   getReporter().reportCompletion(getThreadIndex());
}

template<typename T>
void Rescale::RescaleThread::nearestNeighbor(T* pData, int row, int col, DataAccessor& accessor, unsigned int bands)
{
   int sourceRow = static_cast<int>(row / mInput.mRowFactor + 0.5);
   int sourceCol = static_cast<int>(col / mInput.mColFactor + 0.5);
   accessor->toPixel(sourceRow, sourceCol);
   if (accessor.isValid())
   {
      memcpy(pData, accessor->getColumn(), bands * sizeof(T));
   }
}

template<typename T>
void Rescale::RescaleThread::bilinear(T* pData, int row, int col, DataAccessor& accessor, unsigned int bands)
{
}

template<typename T>
void Rescale::RescaleThread::bicubic(T* pData, int row, int col, DataAccessor& accessor, unsigned int bands)
{
}

bool Rescale::RescaleThreadOutput::compileOverallResults(const std::vector<RescaleThread*>& threads)
{
   return true;
}
