/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU General Public License Version 2
 * The license text is available from   
 * http://www.gnu.org/licenses/gpl.html
 */

#include "DesktopServices.h"
#include "LayerList.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "utilities.h"
#include <octave/oct.h>
#include <octave/dRowVector.h>
#include <octave/ov-struct.h>

DEFUN_DLD(get_data_element, args, nargout,
          "Get a RasterElement data structure which can be passed into other functions.\n"
          "With no arguments, return the active view's element, otherwise specify a view name.")
{
   if (nargout != 1)
   {
      error("Invalid number of output arguments.");
      return octave_value_list();
   }
   if (!initialize_service())
   {
      error("Unable to initialize connection to Opticks.");
      return octave_value_list();
   }
   SpatialDataView* pView = NULL;
   if (args.length() == 0)
   {
      pView = dynamic_cast<SpatialDataView*>(Service<DesktopServices>()->getCurrentWorkspaceWindowView());
   }
   else
   {
      SpatialDataWindow* pWindow = static_cast<SpatialDataWindow*>(Service<DesktopServices>()->getWindow(args(0).string_value(), SPATIAL_DATA_WINDOW));
      pView = (pWindow == NULL) ? NULL : pWindow->getSpatialDataView();
   }
   if (pView == NULL)
   {
      error("Unable to locate view.");
      return octave_value_list();
   }
   RasterElement* pElement = pView->getLayerList()->getPrimaryRasterElement();
   if (pElement == NULL)
   {
      error("Invalid element.");
      return octave_value_list();
   }

   const RasterDataDescriptor* pDesc = static_cast<const RasterDataDescriptor*>(pElement->getDataDescriptor());
   Octave_map rval;
   rval.assign("name", pElement->getName());
   RowVector dims(3);
   dims(0) = pDesc->getRowCount();
   dims(1) = pDesc->getColumnCount();
   dims(2) = pDesc->getBandCount();
   rval.assign("dims", octave_value(dims));
   rval.assign("opaque", octave_value(octave_uint64(reinterpret_cast<uint64_t>(pElement))));
   return octave_value(rval);
}
