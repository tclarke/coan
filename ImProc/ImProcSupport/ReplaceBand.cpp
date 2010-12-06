/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "ImProcVersion.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "ReplaceBand.h"
#include "ReplaceBandInputWizard.h"
#include "PlugInArgList.h"
#include "PlugInRegistration.h"
#include "PlugInManagerServices.h"

REGISTER_PLUGIN_BASIC(ImProcSupport, ReplaceBand);

ReplaceBand::ReplaceBand() : mAbortFlag(false)
{
   setName("ReplaceBand");
   setDescription("Replace a band in one data element with a band from another.");
   setDescriptorId("{8E70F646-8C97-41CC-8653-E050F12961C6}");
   setCopyright(IMPROC_COPYRIGHT);
   setVersion(IMPROC_VERSION_NUMBER);
   setProductionStatus(IMPROC_IS_PRODUCTION_RELEASE);
   setAbortSupported(true);
   setMenuLocation("[General Algorithms]/Replace Band");
}

ReplaceBand::~ReplaceBand()
{
}

bool ReplaceBand::getInputSpecification(PlugInArgList*& pInArgList)
{
   VERIFY(pInArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pInArgList->addArg<Progress>(ProgressArg(), NULL));
   VERIFY(pInArgList->addArg<RasterElement>(DataElementArg()));
   VERIFY(pInArgList->addArg<RasterElement>("Source Element"));
   VERIFY(pInArgList->addArg<unsigned int>("Destination Band", "Active band number"));
   VERIFY(pInArgList->addArg<unsigned int>("Source Band", "Active band number"));
   return true;
}

bool ReplaceBand::getOutputSpecification(PlugInArgList*& pOutArgList)
{
   pOutArgList = NULL;
   return true;
}

bool ReplaceBand::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (pInArgList == NULL)
   {
      return false;
   }
   if (!extractInputArgs(pInArgList))
   {
      return false;
   }

   mProgress.report("Begin band replacement conversion.", 1, NORMAL);

   mInput.mpAbortFlag = &mAbortFlag;
   ReplaceBandThreadOutput outputData;
   mta::ProgressObjectReporter reporter("Rescaling", mProgress.getCurrentProgress());
   mta::MultiThreadedAlgorithm<ReplaceBandThreadInput, ReplaceBandThreadOutput, ReplaceBandThread>     
          alg(Service<ConfigurationSettings>()->getSettingThreadCount(), mInput, outputData, &reporter);
   switch(alg.run())
   {
   case mta::SUCCESS:
      if (!mAbortFlag)
      {
         mInput.mpDest->updateData();
         mProgress.report("Band replacement complete.", 100, NORMAL);
         mProgress.upALevel();
         return true;
      }
      // fall through
   case mta::ABORT:
      mProgress.report("Band replacement aborted.", 0, ABORT, true);
      return false;
   case mta::FAILURE:
      mProgress.report("Band replacement failed.", 0, ERRORS, true);
      return false;
   }
   return true; // make the compiler happy
}

bool ReplaceBand::extractInputArgs(PlugInArgList* pInArgList)
{
   VERIFY(pInArgList);
   mProgress = ProgressTracker(pInArgList->getPlugInArgValue<Progress>(ProgressArg()),
      "Executing " + getName(), "app", "{6cc7599f-9403-4d8e-92bb-06a57a5b2dd4}");
   if ((mInput.mpDest = pInArgList->getPlugInArgValue<RasterElement>(DataElementArg())) == NULL)
   {
      mProgress.report("No destination raster element.", 0, ERRORS, true);
      return false;
   }
   mInput.mpSource = pInArgList->getPlugInArgValue<RasterElement>("Source Element");
   unsigned int destBand = 0;
   bool destBandGood = pInArgList->getPlugInArgValue("Destination Band", destBand);
   unsigned int sourceBand = 0;
   bool sourceBandGood = pInArgList->getPlugInArgValue("Source Band", sourceBand);

   if (!isBatch())
   {
      ReplaceBandInputWizard wiz(mInput.mpDest);
      if (wiz.exec() != QDialog::Accepted)
      {
         mProgress.report("User aborted.", 0, ABORT, true);
         return false;
      }
      mInput.mpSource = wiz.getSourceElement();
      destBandGood = sourceBandGood = true;
      destBand = wiz.getDestBand();
      sourceBand = wiz.getSourceBand();
   }

   if (mInput.mpSource == NULL)
   {
      mProgress.report("No source raster element.", 0, ERRORS, true);
      return false;
   }
   mInput.mDestBand = static_cast<const RasterDataDescriptor*>(mInput.mpDest->getDataDescriptor())->getActiveBand(destBand);
   mInput.mSourceBand = static_cast<const RasterDataDescriptor*>(mInput.mpSource->getDataDescriptor())->getActiveBand(sourceBand);
   if (!mInput.mDestBand.isValid())
   {
      mProgress.report("Invalid destination band specification.", 0, ERRORS, true);
      return false;
   }
   if (!mInput.mSourceBand.isValid())
   {
      mProgress.report("Invalid source band specification.", 0, ERRORS, true);
      return false;
   }

   return true;
}

ReplaceBand::ReplaceBandThread::ReplaceBandThread(
   const ReplaceBandThreadInput &input, int threadCount, int threadIndex, mta::ThreadReporter &reporter) :
               mta::AlgorithmThread(threadIndex, reporter),
               mInput(input),
               mRowRange(getThreadRange(threadCount, static_cast<const RasterDataDescriptor*>(
                                 input.mpDest->getDataDescriptor())->getRowCount()))
{
}

void ReplaceBand::ReplaceBandThread::run()
{
   if (mInput.mpSource == NULL || mInput.mpDest == NULL)
   {
      getReporter().reportError("No data elements specified.");
      return;
   }

   const RasterDataDescriptor* pSourceDesc = static_cast<const RasterDataDescriptor*>(mInput.mpSource->getDataDescriptor());
   const RasterDataDescriptor* pDestDesc = static_cast<const RasterDataDescriptor*>(mInput.mpDest->getDataDescriptor());

   EncodingType encoding = pDestDesc->getDataType();
   int numCols = pDestDesc->getColumnCount();

   int oldPercentDone = 0;

   int startRow = mRowRange.mFirst;
   int stopRow = mRowRange.mLast;

   bool isBip = (pDestDesc->getInterleaveFormat() == BIP);
   size_t offset = isBip ? (pDestDesc->getBytesPerElement() * mInput.mDestBand.getActiveNumber()) : 0;
   size_t cpySize = isBip ? pSourceDesc->getBytesPerElement() : (pSourceDesc->getBytesPerElement() * numCols);

   FactoryResource<DataRequest> pSourceRequest;
   pSourceRequest->setRows(pSourceDesc->getActiveRow(mRowRange.mFirst), pSourceDesc->getActiveRow(mRowRange.mLast));
   pSourceRequest->setColumns(pSourceDesc->getActiveColumn(0), pSourceDesc->getActiveColumn(numCols - 1));
   pSourceRequest->setBands(mInput.mSourceBand, mInput.mSourceBand);
   pSourceRequest->setInterleaveFormat(BSQ);
   DataAccessor sourceAccessor = mInput.mpSource->getDataAccessor(pSourceRequest.release());

   FactoryResource<DataRequest> pDestRequest;
   pDestRequest->setRows(pDestDesc->getActiveRow(mRowRange.mFirst), pDestDesc->getActiveRow(mRowRange.mLast));
   pDestRequest->setColumns(pDestDesc->getActiveColumn(0), pDestDesc->getActiveColumn(numCols - 1));
   if (!isBip)
   {
      pDestRequest->setBands(mInput.mDestBand, mInput.mDestBand);
   }
   pDestRequest->setWritable(true);
   DataAccessor destAccessor = mInput.mpDest->getDataAccessor(pDestRequest.release());

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

      if (isBip)
      {
         for (int col_index = 0; col_index < numCols; col_index++)
         {
            if (!sourceAccessor.isValid() || !destAccessor.isValid())
            {
               getReporter().reportError("Invalid data access.");
               return;
            }

            memcpy(reinterpret_cast<char*>(destAccessor->getColumn()) + offset, sourceAccessor->getColumn(), cpySize);

            sourceAccessor->nextColumn();
            destAccessor->nextColumn();
         }
      }
      else
      {
         memcpy(destAccessor->getRow(), sourceAccessor->getRow(), cpySize);
      }
      sourceAccessor->nextRow();
      destAccessor->nextRow();
   }

   getReporter().reportCompletion(getThreadIndex());
}

bool ReplaceBand::ReplaceBandThreadOutput::compileOverallResults(const std::vector<ReplaceBandThread*>& threads)
{
   return true;
}
