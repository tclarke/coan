/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "ImProcVersion.h"
#include "SpectralWavelet.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterLayer.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "Statistics.h"
#include "StringUtilitiesMacros.h"
#include "switchOnEncoding.h"
#include "Undo.h"
#include "WaveletDialog.h"
extern "C" {
#include <local.h>
};

REGISTER_PLUGIN_BASIC(WaveletModule, SpectralWavelet);

namespace StringUtilities
{
BEGIN_ENUM_MAPPING(WaveletBasis)
ADD_ENUM_MAPPING(BATTLE_LEMARIE, "Battle-Lemarie", "BL")
ADD_ENUM_MAPPING(BURT_ADELSON, "Burt-Adelson", "BA")
ADD_ENUM_MAPPING(COIFLET_2, "Coiflet 2nd order", "C2")
ADD_ENUM_MAPPING(COIFLET_4, "Coiflet 4th order", "C4")
ADD_ENUM_MAPPING(COIFLET_6, "Coiflet 6th order", "C6")
ADD_ENUM_MAPPING(DAUBECHIES_4, "Daubechies 4th order", "D4")
ADD_ENUM_MAPPING(DAUBECHIES_6, "Daubechies 6th order", "D6")
ADD_ENUM_MAPPING(DAUBECHIES_8, "Daubechies 8th order", "D8")
ADD_ENUM_MAPPING(DAUBECHIES_10, "Daubechies 10th order", "D10")
ADD_ENUM_MAPPING(DAUBECHIES_12, "Daubechies 12th order", "D12")
ADD_ENUM_MAPPING(DAUBECHIES_20, "Daubechies 20th order", "D20")
ADD_ENUM_MAPPING(HAAR, "Haar", "D2")
ADD_ENUM_MAPPING(PSEUDOCOIFLET_4_4, "Pseudo Coiflet", "PC")
ADD_ENUM_MAPPING(SPLINE_2_2, "Spline 2,2", "S2-2")
ADD_ENUM_MAPPING(SPLINE_2_4, "Spline 2,4", "S2-4")
ADD_ENUM_MAPPING(SPLINE_3_3, "Spline 3,3", "S3-3")
ADD_ENUM_MAPPING(SPLINE_3_7, "Spline 3,7", "S3-7")
END_ENUM_MAPPING()
}

SpectralWavelet::SpectralWavelet() :
   mAbortFlag(false)
{
   setName("SpectralWavelet");
   setDescription("Spectral wavelet decomposition");
   setDescriptorId("{5DFFFD82-262C-43A8-B835-26B1F2C913F7}");
   setCopyright(IMPROC_COPYRIGHT);
   setVersion(IMPROC_VERSION_NUMBER);
   setProductionStatus(IMPROC_IS_PRODUCTION_RELEASE);
   setAbortSupported(true);
   setMenuLocation("[General Algorithms]/Spectral Wavelet Decomposition");
}

SpectralWavelet::~SpectralWavelet()
{
}

bool SpectralWavelet::getInputSpecification(PlugInArgList*& pInArgList)
{
   VERIFY(pInArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pInArgList->addArg<Progress>(ProgressArg(), NULL));
   VERIFY(pInArgList->addArg<RasterElement>(DataElementArg()));
   VERIFY(pInArgList->addArg<std::string>("Result Name"));
   VERIFY(pInArgList->addArg<bool>("Forward", true));
   std::string defBasis = StringUtilities::toXmlString<WaveletBasis>(DAUBECHIES_4);
   std::string basisHelp = "The basis function for the wavelet. Valid values and their interpretation are:";
   std::vector<std::string> xmls = StringUtilities::getAllEnumValuesAsXmlString<WaveletBasis>();
   std::vector<std::string> vals = StringUtilities::getAllEnumValuesAsDisplayString<WaveletBasis>();
   for (unsigned int idx = 0; idx < xmls.size(); idx++)
   {
      basisHelp += "\n" + xmls[idx] + " = " + vals[idx];
   }
   VERIFY(pInArgList->addArg<std::string>("Wavelet Basis", defBasis, basisHelp));
   return true;
}

bool SpectralWavelet::getOutputSpecification(PlugInArgList*& pOutArgList)
{
   VERIFY(pOutArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pOutArgList->addArg<RasterElement>("Data Element"));
   VERIFY(pOutArgList->addArg<SpatialDataView>("View"));
   return true;
}

bool SpectralWavelet::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (pInArgList == NULL || pOutArgList == NULL)
   {
      return false;
   }
   if (!extractInputArgs(pInArgList))
   {
      return false;
   }

   mProgress.report("Begin spectral wavelet decomposition.", 1, NORMAL);

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
      FLT8BYTES, BIP, mInput.mpDescriptor->getProcessingLocation() == IN_MEMORY));
   mInput.mpResult = pResult.get();
   if (mInput.mpResult == NULL)
   {
      mProgress.report("Unable to create result data set.", 0, ERRORS, true);
      return false;
   }
   mInput.mpResultDescriptor = static_cast<const RasterDataDescriptor*>(mInput.mpResult->getDataDescriptor());
   mInput.mpAbortFlag = &mAbortFlag;
   SpectralWaveletThreadOutput outputData;
   mta::ProgressObjectReporter reporter("Decomposing", mProgress.getCurrentProgress());
   mta::MultiThreadedAlgorithm<SpectralWaveletThreadInput, SpectralWaveletThreadOutput, SpectralWaveletThread>     
          alg(Service<ConfigurationSettings>()->getSettingThreadCount(), mInput, outputData, &reporter);
   switch(alg.run())
   {
   case mta::SUCCESS:
      if (!mAbortFlag)
      {
         mProgress.report("Decomposition complete.", 100, NORMAL);
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
      mProgress.report("Decomposition aborted.", 0, ABORT, true);
      return false;
   case mta::FAILURE:
      mProgress.report("Decomposition failed.", 0, ERRORS, true);
      return false;
   }
   return true; // make the compiler happy
}

bool SpectralWavelet::extractInputArgs(PlugInArgList* pInArgList)
{
   VERIFY(pInArgList);
   mProgress = ProgressTracker(pInArgList->getPlugInArgValue<Progress>(ProgressArg()),
      "Executing " + getName(), "app", "{6ae5bbf0-8cec-46dd-a1e4-2f45e5b618af}");
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

   pInArgList->getPlugInArgValue("Forward", mInput.mForward);
   std::string basisStr;
   pInArgList->getPlugInArgValue("Wavelet Basis", basisStr);
   mInput.mBasis = StringUtilities::fromXmlString<WaveletBasis>(basisStr);
   if (!isBatch())
   {
      WaveletDialog dlg;
      dlg.setBasis(mInput.mBasis);
      dlg.setForward(mInput.mForward);
      if (dlg.exec() != QDialog::Accepted)
      {
         mProgress.report("User aborted.", 0, ABORT, true);
         return false;
      }
      mInput.mBasis = dlg.getBasis();
      mInput.mForward = dlg.getForward();
   }

   return true;
}

bool SpectralWavelet::displayResult()
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

SpectralWavelet::SpectralWaveletThread::SpectralWaveletThread(
   const SpectralWaveletThreadInput &input, int threadCount, int threadIndex, mta::ThreadReporter &reporter) :
               mta::AlgorithmThread(threadIndex, reporter),
               mInput(input),
               mRowRange(getThreadRange(threadCount, input.mpResultDescriptor->getRowCount()))
{
}

void SpectralWavelet::SpectralWaveletThread::run()
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
   unsigned int numBands = mInput.mpResultDescriptor->getBandCount();

   FactoryResource<DataRequest> pResultRequest;
   pResultRequest->setRows(mInput.mpResultDescriptor->getActiveRow(mRowRange.mFirst),
      mInput.mpResultDescriptor->getActiveRow(mRowRange.mLast));
   pResultRequest->setColumns(mInput.mpResultDescriptor->getActiveColumn(0),
      mInput.mpResultDescriptor->getActiveColumn(numCols - 1));
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
   pRequest->setInterleaveFormat(BIP);
   DataAccessor accessor = mInput.mpRaster->getDataAccessor(pRequest.release());
   if (!accessor.isValid())
   {
      getReporter().reportError("Invalid data access.");
      return;
   }

   waveletfilter* pFlt = &wfltrDaubechies_4;
   std::auto_ptr<double> pInput(new double[numBands]);
   Service<ModelServices> pModel;
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

         // do the work
         for (unsigned int band = 0; band < numBands; band++)
         {
            pInput.get()[band] = pModel->getDataValue(encoding, accessor->getColumn(), band);
         }
         wxfrm_da1d(pInput.get(), numBands, mInput.mForward ? 1 : 0, pFlt, reinterpret_cast<double*>(resultAccessor->getColumn()));

         resultAccessor->nextColumn();
         accessor->nextColumn();
      }
      resultAccessor->nextRow();
      accessor->nextRow();
   }
   getReporter().reportCompletion(getThreadIndex());
}

bool SpectralWavelet::SpectralWaveletThreadOutput::compileOverallResults(const std::vector<SpectralWaveletThread*>& threads)
{
   return true;
}