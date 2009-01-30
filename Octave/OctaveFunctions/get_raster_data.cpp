/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU General Public License Version 2
 * The license text is available from   
 * http://www.gnu.org/licenses/gpl.html
 */

#include "ComplexData.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DesktopServices.h"
#include "LayerList.h"
#include "ObjectResource.h"
#include "RasterElement.h"
#include "RasterDataDescriptor.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "switchOnEncoding.h"
#include "utilities.h"
#include <complex>
#include <octave/oct.h>
#include <octave/CMatrix.h>
#include <octave/dMatrix.h>
#include <octave/ov-struct.h>

namespace
{
   template<typename T>
   void populate_array(T* pData, size_t row, size_t cols, Matrix& arr)
   {
      for (size_t col = 0; col < cols; col++)
      {
         arr(row, col) = pData[col];
      }
   }
}

DEFUN_DLD(get_raster_data, args, nargout,
          "A = get_raster_data(element, band, subcube)\n"
          "Get a data from a RasterElement.\n"
          "arg0 should be a data structure from get_data_element.\n"
          "arg1 specifies the requested band number (1 indexed).\n"
          "arg2 is optional and specifies a sub-cube. It is a 2x2 matrix with start and end values for rows and columns."
          "Indicate 0, 0 for the entire dimension.")
{
   if (nargout != 1)
   {
      error("Invalid number of output arguments.");
      return octave_value_list();
   }
   int nargin = args.length();
   if (nargin < 2 || nargin > 3)
   {
      error("Invalid number of input arguments.");
      return octave_value_list();
   }
   if (!initialize_service())
   {
      error("Unable to initialize connection to Opticks.");
      return octave_value_list();
   }
   RasterElement* pElement = NULL;
   Octave_map elementSpec = args(0).map_value();
   if (elementSpec.contains("opaque"))
   {
      Octave_map::const_iterator opit = elementSpec.seek("opaque");
      pElement = reinterpret_cast<RasterElement*>(elementSpec.contents(opit)(0).uint64_scalar_value().value());
   }
   if (pElement == NULL)
   {
      error("Invalid RasterElement.");
      return octave_value_list();
   }

   const RasterDataDescriptor* pDesc = static_cast<const RasterDataDescriptor*>(pElement->getDataDescriptor());
   DimensionDescriptor startRow;
   DimensionDescriptor endRow;
   DimensionDescriptor startCol;
   DimensionDescriptor endCol;
   int rows = pDesc->getRowCount();
   int cols = pDesc->getColumnCount();
   if (nargin == 3)
   {
      Matrix subcube = args(2).matrix_value();
      startRow = pDesc->getActiveRow(subcube(0, 0));
      endRow = pDesc->getActiveRow(subcube(0, 1));
      rows = endRow.getActiveNumber() - startRow.getActiveNumber() + 1;
      startCol = pDesc->getActiveColumn(subcube(1, 0));
      endCol = pDesc->getActiveColumn(subcube(1, 1));
      cols = endCol.getActiveNumber() - startCol.getActiveNumber() + 1;
   }
   DimensionDescriptor band = pDesc->getActiveBand(args(1).scalar_value() - 1);
   FactoryResource<DataRequest> pReq;
   pReq->setRows(startRow, endRow);
   pReq->setColumns(startCol, endCol);
   pReq->setBands(band, band);
   pReq->setInterleaveFormat(BSQ);
   DataAccessor acc(pElement->getDataAccessor(pReq.release()));

   dim_vector dim(2);
   dim(0) = rows;
   dim(1) = cols;
   Matrix arr(dim);
   for (int row = 0; row < rows; row++)
   {
      if (!acc.isValid())
      {
         error("Unable to copy data. Try sub-cubing.");
         return octave_value_list();
      }
      switchOnEncoding(pDesc->getDataType(), populate_array, acc->getRow(), row, cols, arr);
      acc->nextRow();
   }

   return octave_value(arr);
}
