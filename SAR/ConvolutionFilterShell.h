/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef CONVOLUTIONFILTERSHELL_H__
#define CONVOLUTIONFILTERSHELL_H__

#include "AlgorithmShell.h"
#include "MultiThreadedAlgorithm.h"
#include "ProgressTracker.h"

#include <ossim/matrix/newmat.h>

class AoiElement;
class RasterDataDescriptor;
class RasterElement;
struct CheckWithBitMask;

class ConvolutionFilterShell : public AlgorithmShell
{
public:
   ConvolutionFilterShell();
   virtual ~ConvolutionFilterShell();

   virtual bool getInputSpecification(PlugInArgList *&pInArgList);
   virtual bool getOutputSpecification(PlugInArgList *&pOutArgList);
   virtual bool execute(PlugInArgList *pInArgList, PlugInArgList *pOutArgList);

protected:
   virtual bool extractInputArgs(PlugInArgList *pInArgList);
   /**
    * Populate the kernel.
    *
    * This method should set mKernel to the convolution kernel in row major format.
    * mKernelRows should indicate the number of rows in the kernel.
    * mKernel.size() / mKernelRows indicates the number of columns.
    *
    * @return false on error, true on success
    */
   virtual bool populateKernel() = 0;

   RasterElement *mpRaster;
   const RasterDataDescriptor *mpDescriptor;
   unsigned int mBand;
   NEWMAT::Matrix mKernel;
   NEWMAT::Matrix mKernelImag;

   ProgressTracker mProgress;
   AoiElement *mpAoi;
   std::string mResultName;
};

#endif