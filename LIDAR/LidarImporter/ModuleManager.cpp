/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

class LidarImporterFactory;

#include "ModuleManager.h"
#include "LidarImporterFactory.h"
#include <algorithm>
#include <vector>

const char *ModuleManager::mspName = "LidarImporter";
const char *ModuleManager::mspVersion = "1.0";
const char *ModuleManager::mspDescription = "LIDAR Data Importers";
const char *ModuleManager::mspValidationKey = "none";
const char *ModuleManager::mspUniqueId = "{5E3012F0-294E-4D2B-9F13-A8008DF3D7E8}";

std::vector<LidarImporterFactory*>& factories()
{
   static std::vector<LidarImporterFactory*> sFactories;
   return sFactories;
}

unsigned int ModuleManager::getTotalPlugIns()
{
   return static_cast<unsigned int>(factories().size());
}

void addFactory(LidarImporterFactory* pFactory)
{
   if (pFactory != NULL)
   {
      factories().push_back(pFactory);
   }
}

struct FactoryPtrComparator
{
   bool operator()(LidarImporterFactory* pLhs, LidarImporterFactory* pRhs)
   {
      if (pLhs == NULL || pRhs == NULL)
      {
         return false;
      }
      size_t leftCount = std::count(pLhs->mName.begin(), pLhs->mName.end(), '_');
      size_t rightCount = std::count(pRhs->mName.begin(), pRhs->mName.end(), '_');
      if (leftCount != rightCount)
      {
         return leftCount > rightCount;
      }
      return pLhs->mName < pRhs->mName;
   }
};

PlugIn *ModuleManager::getPlugIn(unsigned int plugInNumber)
{
   static bool factoriesSorted = false;
   if (!factoriesSorted)
   {
      factoriesSorted = true;
      FactoryPtrComparator comp;
      std::sort(factories().begin(), factories().end(), comp);
   }
   if (plugInNumber >= factories().size())
   {
      return NULL;
   }
   return (*(factories()[plugInNumber]))();
}
