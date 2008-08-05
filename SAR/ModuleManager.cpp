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

#include "ModuleManager.h"
#include "CalculateLooks.h"
#include "Fft.h"
#include "MultilookDespeckle.h"

const char *ModuleManager::mspName = "SAR";
const char *ModuleManager::mspVersion = "1";
const char *ModuleManager::mspDescription = "Basic SAR algorithms.";
const char *ModuleManager::mspValidationKey = "none";
const char *ModuleManager::mspUniqueId = "{CB4BCC2A-DAB6-49c7-AC62-419468A5F1B6}";

unsigned int ModuleManager::getTotalPlugIns()
{
   return 3;
}

PlugIn* ModuleManager::getPlugIn(unsigned int plugInNumber)
{
   PlugIn *pPlugIn = NULL;

   switch(plugInNumber)
   {
      case 0:
         pPlugIn = new CalculateLooks;
         break;
      case 1:
         pPlugIn = new MultilookDespeckle;
         break;
      case 2:
         pPlugIn = new Fft;
         break;
   }
   return pPlugIn;
}
