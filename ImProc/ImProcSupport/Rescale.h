/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef RESCALE_H__
#define RESCALE_H__

#include "AlgorithmShell.h"
#include "EnumWrapper.h"
#include "MultiThreadedAlgorithm.h"
#include "ProgressTracker.h"

class DataAccessor;
class RasterDataDescriptor;
class RasterElement;

enum InterpTypeEnum { NEAREST, BILINEAR, BICUBIC };
typedef EnumWrapper<InterpTypeEnum> InterpType;

class Rescale : public AlgorithmShell
{
public:
   Rescale();
   virtual ~Rescale();

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

   struct RescaleThreadInput
   {
      RescaleThreadInput() : mpRaster(NULL), mpDescriptor(NULL), mpResultDescriptor(NULL), mpResult(NULL),
         mRowFactor(1.0), mColFactor(1.0), mInterp(NEAREST), mpAbortFlag(NULL)
         {}
      const RasterElement* mpRaster;
      const RasterDataDescriptor* mpDescriptor;
      const RasterDataDescriptor* mpResultDescriptor;
      RasterElement* mpResult;
      double mRowFactor;
      double mColFactor;
      InterpType mInterp;
      const bool* mpAbortFlag;
   };

   class RescaleThread : public mta::AlgorithmThread
   {
   public:
      RescaleThread(const RescaleThreadInput& input, int threadCount, int threadIndex, mta::ThreadReporter& reporter);
      void run();

   private:
      template<typename T> void nearestNeighbor(T* pData, int row, int col, DataAccessor& accessor, unsigned int bands);
      template<typename T> void bilinear(T* pData, int row, int col, DataAccessor& accessor, unsigned int bands);
      template<typename T> void bicubic(T* pData, int row, int col, DataAccessor& accessor, unsigned int bands);
      const RescaleThreadInput &mInput;
      mta::AlgorithmThread::Range mRowRange;
   };

   struct RescaleThreadOutput
   {
      bool compileOverallResults(const std::vector<RescaleThread*> &threads);
   };

   ProgressTracker mProgress;
   RescaleThreadInput mInput;
   std::string mResultName;
   SpatialDataView* mpSourceView;
   bool mAbortFlag;
};

#endif
