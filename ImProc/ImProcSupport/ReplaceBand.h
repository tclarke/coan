/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef REPLACEBAND_H__
#define REPLACEBAND_H__

#include "AlgorithmShell.h"
#include "DimensionDescriptor.h"
#include "MultiThreadedAlgorithm.h"
#include "ProgressTracker.h"

class RasterElement;

class ReplaceBand : public AlgorithmShell
{
public:
   ReplaceBand();
   virtual ~ReplaceBand();

   virtual bool getInputSpecification(PlugInArgList*& pInArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pOutArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
   virtual bool abort()
   {
      mAbortFlag = true;
      return true;
   }

protected:
   virtual bool extractInputArgs(PlugInArgList* pInArgList);
   struct ReplaceBandThreadInput
   {
      ReplaceBandThreadInput() : mpSource(NULL), mpDest(NULL), mpAbortFlag(NULL) {}
      RasterElement* mpSource;
      RasterElement* mpDest;
      DimensionDescriptor mSourceBand;
      DimensionDescriptor mDestBand;
      const bool* mpAbortFlag;
   };

   class ReplaceBandThread : public mta::AlgorithmThread
   {
   public:
      ReplaceBandThread(const ReplaceBandThreadInput& input, int threadCount, int threadIndex, mta::ThreadReporter& reporter);
      void run();

   private:
      const ReplaceBandThreadInput &mInput;
      mta::AlgorithmThread::Range mRowRange;
   };

   struct ReplaceBandThreadOutput
   {
      bool compileOverallResults(const std::vector<ReplaceBandThread*> &threads);
   };

   ProgressTracker mProgress;
   ReplaceBandThreadInput mInput;
   bool mAbortFlag;
};

#endif
