/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef BACKGROUNDRUNNINGAVERAGE_H__
#define BACKGROUNDRUNNINGAVERAGE_H__

#include "AlgorithmShell.h"

class BackgroundRunningAverage : public AlgorithmShell
{
public:
   BackgroundRunningAverage();
   virtual ~BackgroundRunningAverage();

   virtual bool getInputSpecification(PlugInArgList *&pArgList);
   virtual bool getOutputSpecification(PlugInArgList *&pArgList);
   virtual bool execute(PlugInArgList *pInArgList, PlugInArgList *pOutArgList);
};

#endif