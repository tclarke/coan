/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef AOILOGICAL_H__
#define AOILOGICAL_H__

#include "AlgorithmShell.h"
#include "ProgressTracker.h"

class BitMask;
class SpatialDataView;

class AoiLogical : public AlgorithmShell
{
public:
   AoiLogical();
   virtual ~AoiLogical();

   virtual bool getInputSpecification(PlugInArgList*& pInArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pOutArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

private:
   bool extractInputArgs(PlugInArgList* pInArgList);
   bool displayResult();

   const BitMask* mpSet1;
   const BitMask* mpSet2;
   BitMask* mpResult;
   std::string mOperation;
   SpatialDataView* mpView;
   std::string mResultName;

   ProgressTracker mProgress;
};

#endif
