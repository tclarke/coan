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
#include "FitsImporter.h"

const char *ModuleManager::mspName = "Fits";
const char *ModuleManager::mspVersion = "1";
const char *ModuleManager::mspDescription = "FITS I/O";
const char *ModuleManager::mspValidationKey = "none";
const char *ModuleManager::mspUniqueId = "{AE612813-2172-40D3-9828-95B77D6206D4}";

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
         pPlugIn = static_cast<PlugIn*>(new FitsImporter);
         break;
      case 1:
         pPlugIn = static_cast<PlugIn*>(new FitsRasterPager);
         break;
   }
   return pPlugIn;
}
