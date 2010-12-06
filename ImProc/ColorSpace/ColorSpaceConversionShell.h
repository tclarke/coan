/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef COLORSPACECONVERSIONSHELL_H__
#define COLORSPACECONVERSIONSHELL_H__

#include "AlgorithmShell.h"
#include "MultiThreadedAlgorithm.h"
#include "ProgressTracker.h"

class RasterDataDescriptor;
class RasterElement;

class ColorSpaceConversionShell : public AlgorithmShell
{
public:
   ColorSpaceConversionShell();
   virtual ~ColorSpaceConversionShell();

   virtual bool getInputSpecification(PlugInArgList*& pInArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pOutArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
   virtual bool abort()
   {
      mAbortFlag = true;
      return true;
   }

   virtual void colorspaceConvert(double pOutput[3], double pInput[3], double maxComponent, double minComponent) = 0;

protected:
   void setScaleData(bool scale) { mScaleData = scale; }
   virtual bool extractInputArgs(PlugInArgList* pInArgList);
   virtual bool displayResult();

   struct ColorSpaceConversionShellThreadInput
   {
      ColorSpaceConversionShellThreadInput() : mpRaster(NULL), mpDescriptor(NULL), mpResult(NULL), mpAbortFlag(NULL),
         mRedBand(0), mGreenBand(0), mBlueBand(0), mMaxScale(0.0), mpCaller(NULL) {}
      const RasterElement* mpRaster;
      const RasterDataDescriptor* mpDescriptor;
      RasterElement* mpResult;
      const bool* mpAbortFlag;
      unsigned int mRedBand;
      unsigned int mGreenBand;
      unsigned int mBlueBand;
      double mMaxScale;
      ColorSpaceConversionShell* mpCaller;
   };

   class ColorSpaceConversionShellThread : public mta::AlgorithmThread
   {
   public:
      ColorSpaceConversionShellThread(const ColorSpaceConversionShellThreadInput& input,
         int threadCount, int threadIndex, mta::ThreadReporter& reporter);
      void run();

   private:
      template<typename T> void convertFunctor(const T* pData, double* pResultData);
      const ColorSpaceConversionShellThreadInput &mInput;
      mta::AlgorithmThread::Range mRowRange;
   };

   struct ColorSpaceConversionShellThreadOutput
   {
      bool compileOverallResults(const std::vector<ColorSpaceConversionShellThread*> &threads);
   };

   SpatialDataView* mpSourceView;
   ProgressTracker mProgress;
   ColorSpaceConversionShellThreadInput mInput;
   std::string mResultName;
   bool mAbortFlag;
   bool mScaleData;
};

#endif
