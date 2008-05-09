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
#include "BackgroundRunningAverage.h"
#include "Timeline.h"

const char *ModuleManager::mspName = "Temporal";
const char *ModuleManager::mspVersion = "1";
const char *ModuleManager::mspDescription = "Temporal processing";
const char *ModuleManager::mspValidationKey = "none";
const char *ModuleManager::mspUniqueId = "{a6bf5d0a-80aa-48e4-8139-bd3cb71a203d}";

unsigned int ModuleManager::getTotalPlugIns()
{
   return 2;
}

PlugIn* ModuleManager::getPlugIn(unsigned int plugInNumber)
{
   PlugIn *pPlugIn = NULL;

   switch(plugInNumber)
   {
   case 0:
      pPlugIn = new BackgroundRunningAverage();
      break;
   case 1:
      pPlugIn = new Timeline();
   }
   return pPlugIn;
}
