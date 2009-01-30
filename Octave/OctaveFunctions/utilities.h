/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU General Public License Version 2
 * The license text is available from   
 * http://www.gnu.org/licenses/gpl.html
 */


#include "ModuleManager.h"
#include <octave/oct.h>

namespace
{
bool initialize_service()
{
   octave_value osVal = get_global_value("opticksservice", true);
   if (osVal.is_uint64_type())
   {
      ModuleManager::instance()->setService(reinterpret_cast<External*>(osVal.uint64_scalar_value().value()));
      return true;
   }
   return false;
}
}