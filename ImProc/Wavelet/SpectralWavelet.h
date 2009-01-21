/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SPECTRALWAVELET_H__
#define SPECTRALWAVELET_H__

#include "AlgorithmShell.h"
#include "EnumWrapper.h"
#include "MultiThreadedAlgorithm.h"
#include "ProgressTracker.h"

enum WaveletBasisEnum {
   BATTLE_LEMARIE,
   BURT_ADELSON,
   COIFLET_2,
   COIFLET_4,
   COIFLET_6,
   DAUBECHIES_4,
   DAUBECHIES_6,
   DAUBECHIES_8,
   DAUBECHIES_10,
   DAUBECHIES_12,
   DAUBECHIES_20,
   HAAR,
   PSEUDOCOIFLET_4_4,
   SPLINE_2_2,
   SPLINE_2_4,
   SPLINE_3_3,
   SPLINE_3_7
};

typedef EnumWrapper<WaveletBasisEnum> WaveletBasis;

class SpectralWavelet : public AlgorithmShell
{
public:
   SpectralWavelet();
   virtual ~SpectralWavelet();

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

   struct SpectralWaveletThreadInput
   {
      SpectralWaveletThreadInput() : mpRaster(NULL), mpDescriptor(NULL), mpResultDescriptor(NULL), mpResult(NULL), mForward(true), mpAbortFlag(NULL) {}
      const RasterElement* mpRaster;
      const RasterDataDescriptor* mpDescriptor;
      const RasterDataDescriptor* mpResultDescriptor;
      RasterElement* mpResult;
      bool mForward;
      WaveletBasis mBasis;
      const bool* mpAbortFlag;
   };

   class SpectralWaveletThread : public mta::AlgorithmThread
   {
   public:
      SpectralWaveletThread(const SpectralWaveletThreadInput& input, int threadCount, int threadIndex, mta::ThreadReporter& reporter);
      void run();

   private:
      const SpectralWaveletThreadInput &mInput;
      mta::AlgorithmThread::Range mRowRange;
   };

   struct SpectralWaveletThreadOutput
   {
      bool compileOverallResults(const std::vector<SpectralWaveletThread*> &threads);
   };

   ProgressTracker mProgress;
   SpectralWaveletThreadInput mInput;
   std::string mResultName;
   bool mAbortFlag;
};

#endif
