/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "ImProcVersion.h"
#include "NormalizeData.h"
#include "PlugInArgList.h"
#include "PlugInFactory.h"
#include "PlugInManagerServices.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterLayer.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "Statistics.h"
#include "switchOnEncoding.h"
#include "Undo.h"

PLUGINFACTORY(NormalizeData);

NormalizeData::NormalizeData() :
   mAbortFlag(false)
{
   setName("NormalizeData");
   setDescription("Convert to double and normalize.");
   setDescriptorId("{94E9A2F4-35FA-44E5-B6EF-EB47634EA64E}");
   setCopyright(IMPROC_COPYRIGHT);
   setVersion(IMPROC_VERSION_NUMBER);
   setProductionStatus(IMPROC_IS_PRODUCTION_RELEASE);
   setAbortSupported(true);
   setMenuLocation("[General Algorithms]/Normalize Data");
}

NormalizeData::~NormalizeData()
{
}

bool NormalizeData::getInputSpecification(PlugInArgList*& pInArgList)
{
   VERIFY(pInArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pInArgList->addArg<Progress>(ProgressArg(), NULL));
   VERIFY(pInArgList->addArg<RasterElement>(DataElementArg()));
   VERIFY(pInArgList->addArg<std::string>("Result Name"));
   return true;
}

bool NormalizeData::getOutputSpecification(PlugInArgList*& pOutArgList)
{
   VERIFY(pOutArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pOutArgList->addArg<RasterElement>("Data Element"));
   VERIFY(pOutArgList->addArg<SpatialDataView>("View"));
   return true;
}

bool NormalizeData::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (pInArgList == NULL || pOutArgList == NULL)
   {
      return false;
   }
   if (!extractInputArgs(pInArgList))
   {
      return false;
   }

   mProgress.report("Begin normalization conversion.", 1, NORMAL);

   { // scope the lifetime
      RasterElement *pResult = static_cast<RasterElement*>(
         Service<ModelServices>()->getElement(mResultName, TypeConverter::toString<RasterElement>(), NULL));
      if (pResult != NULL)
      {
         Service<ModelServices>()->destroyElement(pResult);
      }
   }
   ModelResource<RasterElement> pResult(RasterUtilities::createRasterElement(mResultName,
      mInput.mpDescriptor->getRowCount(), mInput.mpDescriptor->getColumnCount(), mInput.mpDescriptor->getBandCount(),
      FLT8BYTES, mInput.mpDescriptor->getInterleaveFormat(), mInput.mpDescriptor->getProcessingLocation() == IN_MEMORY));
   mInput.mpResult = pResult.get();
   if (mInput.mpResult == NULL)
   {
      mProgress.report("Unable to create result data set.", 0, ERRORS, true);
      return false;
   }
   mInput.mpResultDescriptor = static_cast<const RasterDataDescriptor*>(mInput.mpResult->getDataDescriptor());
   mInput.mpAbortFlag = &mAbortFlag;
   NormalizeDataThreadOutput outputData;
   mta::ProgressObjectReporter reporter("Normalizing", mProgress.getCurrentProgress());
   mta::MultiThreadedAlgorithm<NormalizeDataThreadInput, NormalizeDataThreadOutput, NormalizeDataThread>     
          alg(Service<ConfigurationSettings>()->getSettingThreadCount(), mInput, outputData, &reporter);
   switch(alg.run())
   {
   case mta::SUCCESS:
      if (!mAbortFlag)
      {
         mProgress.report("Normalization complete.", 100, NORMAL);
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
      mProgress.report("Normalization aborted.", 0, ABORT, true);
      return false;
   case mta::FAILURE:
      mProgress.report("Normalization failed.", 0, ERRORS, true);
      return false;
   }
   return true; // make the compiler happy
}

bool NormalizeData::extractInputArgs(PlugInArgList* pInArgList)
{
   VERIFY(pInArgList);
   mProgress = ProgressTracker(pInArgList->getPlugInArgValue<Progress>(ProgressArg()),
      "Executing " + getName(), "app", "{f5be0bf3-5fce-4d64-9a9b-89f7e37047dc}");
   if ((mInput.mpRaster = pInArgList->getPlugInArgValue<RasterElement>(DataElementArg())) == NULL)
   {
      mProgress.report("No raster element.", 0, ERRORS, true);
      return false;
   }
   mInput.mpDescriptor = static_cast<const RasterDataDescriptor*>(mInput.mpRaster->getDataDescriptor());
   
   pInArgList->getPlugInArgValue("Result Name", mResultName);
   if (mResultName.empty())
   {
      mResultName = mInput.mpRaster->getName() + ":" + getName();
   }

   return true;
}

bool NormalizeData::displayResult()
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

   return true;
}

NormalizeData::NormalizeDataThread::NormalizeDataThread(
   const NormalizeDataThreadInput &input, int threadCount, int threadIndex, mta::ThreadReporter &reporter) :
               mta::AlgorithmThread(threadIndex, reporter),
               mInput(input),
               mRowRange(getThreadRange(threadCount, input.mpResultDescriptor->getRowCount()))
{
}

void NormalizeData::NormalizeDataThread::run()
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
   std::vector<double> bandMaxValues;
   bandMaxValues.reserve(mInput.mpDescriptor->getBandCount());
   for (unsigned int band = 0; band < mInput.mpDescriptor->getBandCount(); band++)
   {
      bandMaxValues.push_back(mInput.mpRaster->getStatistics(mInput.mpDescriptor->getActiveBand(band))->getMax());
   }

   Service<ModelServices> pModel;

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
      pRequest->setRows(mInput.mpDescriptor->getActiveRow(mRowRange.mFirst),
         mInput.mpDescriptor->getActiveRow(mRowRange.mLast));
      pRequest->setColumns(mInput.mpDescriptor->getActiveColumn(0),
         mInput.mpDescriptor->getActiveColumn(numCols - 1));
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

            for (unsigned int inner = 0; inner < numBandsPerElement; inner++)
            {
               double val = pModel->getDataValue(encoding, accessor->getColumn(), inner);
               val /= bandMaxValues[std::max(band, inner)];
               reinterpret_cast<double*>(resultAccessor->getColumn())[inner] = val;
            }

            resultAccessor->nextColumn();
            accessor->nextColumn();
         }
         resultAccessor->nextRow();
         accessor->nextRow();
      }
   }
   getReporter().reportCompletion(getThreadIndex());
}

bool NormalizeData::NormalizeDataThreadOutput::compileOverallResults(const std::vector<NormalizeDataThread*>& threads)
{
   return true;
}