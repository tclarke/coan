#ifndef OPTICALFLOW_H__
#define OPTICALFLOW_H__
#include "ExecutableShell.h"

class OpticalFlow : public ExecutableShell
{
public:
   OpticalFlow();
   virtual ~OpticalFlow();
   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
};

#endif