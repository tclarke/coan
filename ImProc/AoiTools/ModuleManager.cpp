/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

class AoiToolsFactory;

#include "ModuleManager.h"
#include "AoiToolsFactory.h"
#include <algorithm>
#include <vector>

const char *ModuleManager::mspName = "AoiTools";
const char *ModuleManager::mspVersion = "0.1";
const char *ModuleManager::mspDescription = "Tools to manipulate AOIs";
const char *ModuleManager::mspValidationKey = "none";
const char *ModuleManager::mspUniqueId = "{235E5BD9-0779-4223-BE42-1FA8087E91B8}";

std::vector<AoiToolsFactory*>& factories()
{
   static std::vector<AoiToolsFactory*> sFactories;
   return sFactories;
}

unsigned int ModuleManager::getTotalPlugIns()
{
   return static_cast<unsigned int>(factories().size());
}

void addFactory(AoiToolsFactory* pFactory)
{
   if (pFactory != NULL)
   {
      factories().push_back(pFactory);
   }
}

struct FactoryPtrComparator
{
   bool operator()(AoiToolsFactory* pLhs, AoiToolsFactory* pRhs)
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
