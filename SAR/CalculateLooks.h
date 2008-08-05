/**
* The information in this file is
* Copyright(c) 2007 Ball Aerospace & Technologies Corporation
* and is subject to the terms and conditions of the
* Opticks Limited Open Source License Agreement Version 1.0
* The license text is available from https://www.ballforge.net/
* 
* This material (software and user documentation) is subject to export
* controls imposed by the United States Export Administration Act of 1979,
* as amended and the International Traffic In Arms Regulation (ITAR),
* 22 CFR 120-130
*/

#ifndef CALCULATELOOKS_H__
#define CALCULATELOOKS_H__

#include "AlgorithmShell.h"
#include "DataAccessor.h"
#include "DimensionDescriptor.h"
#include "ProgressTracker.h"

class AoiElement;
class RasterDataDescriptor;
class RasterElement;

class CalculateLooks : public AlgorithmShell
{
public:
   CalculateLooks();
   virtual ~CalculateLooks();

   virtual bool getInputSpecification(PlugInArgList *&pInArgList);
   virtual bool getOutputSpecification(PlugInArgList *&pOutArgList);
   virtual bool execute(PlugInArgList *pInArgList, PlugInArgList *pOutArgList);

protected:
   bool extractInputArgs(PlugInArgList *pInArgList);
   virtual bool initialize();
   virtual bool process();
   virtual bool display(PlugInArgList *pOutArgList);
   
   bool nextPixel();
   DimensionDescriptor getCurrentRow() const;
   DimensionDescriptor getCurrentColumn() const;
   DimensionDescriptor getCurrentBand() const;
   void *getCurrentPixel();

   ProgressTracker mProgress;
   RasterElement *mpRaster;
   RasterDataDescriptor *mpDescriptor;
   AoiElement *mpAoi;

   unsigned int mRow;
   unsigned int mColumn;
   unsigned int mBand;
   unsigned int mStartRow;
   unsigned int mEndRow;
   unsigned int mStartColumn;
   unsigned int mEndColumn;
   DataAccessor mpAccessor;

private:
   double mLooks;
};

#endif