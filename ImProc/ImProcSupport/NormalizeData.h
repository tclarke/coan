/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef NORMALIZEDATA_H__
#define NORMALIZEDATA_H__

#include "AlgorithmShell.h"
#include "MultiThreadedAlgorithm.h"
#include "ProgressTracker.h"

class NormalizeData : public AlgorithmShell
{
public:
   NormalizeData();
   virtual ~NormalizeData();

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
   virtual bool displayResult();

   struct NormalizeDataThreadInput
   {
      NormalizeDataThreadInput() : mpRaster(NULL), mpDescriptor(NULL), mpResultDescriptor(NULL), mpResult(NULL), mpAbortFlag(NULL) {}
      const RasterElement* mpRaster;
      const RasterDataDescriptor* mpDescriptor;
      const RasterDataDescriptor* mpResultDescriptor;
      RasterElement* mpResult;
      const bool* mpAbortFlag;
   };

   class NormalizeDataThread : public mta::AlgorithmThread
   {
   public:
      NormalizeDataThread(const NormalizeDataThreadInput& input, int threadCount, int threadIndex, mta::ThreadReporter& reporter);
      void run();

   private:
      const NormalizeDataThreadInput &mInput;
      mta::AlgorithmThread::Range mRowRange;
   };

   struct NormalizeDataThreadOutput
   {
      bool compileOverallResults(const std::vector<NormalizeDataThread*> &threads);
   };

   ProgressTracker mProgress;
   NormalizeDataThreadInput mInput;
   std::string mResultName;
   bool mAbortFlag;
};
#endif
