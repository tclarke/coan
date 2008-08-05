/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef FFT_H__
#define FFT_H__

#include "AlgorithmShell.h"
#include "ProgressTracker.h"

class AoiElement;
class RasterElement;
namespace NEWMAT { class Matrix; }

class Fft : public AlgorithmShell
{
public:
   Fft();
   virtual ~Fft();

   virtual bool getInputSpecification(PlugInArgList *&pInArgList);
   virtual bool getOutputSpecification(PlugInArgList *&pOutArgList);
   virtual bool execute(PlugInArgList *pInArgList, PlugInArgList *pOutArgList);

protected:
   bool extractInputArgs(PlugInArgList *pInArgList);
   bool fft2(NEWMAT::Matrix &U, NEWMAT::Matrix &V);
   bool ifft2(NEWMAT::Matrix &U, NEWMAT::Matrix &V);

private:
   ProgressTracker mProgress;
   RasterElement *mpRaster;
   RasterElement *mpFft;
   AoiElement *mpAoi;
   unsigned int mPadRows;
   unsigned int mPadColumns;
};

#endif