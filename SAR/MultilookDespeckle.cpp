/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "MultilookDespeckle.h"

#include <ossim/matrix/newmat.h>
#include <QtGui/QInputDialog>

MultilookDespeckle::MultilookDespeckle()
{
   setName("Multilook");
   setDescriptorId("{024c27db-bfd5-49c4-b765-e7b336a0da93}");
   setDescription("Multilook despeckle using a low-pass filter.");
   setMenuLocation("[SAR]/Despeckle/Multilook");
   setProductionStatus(false);
}

MultilookDespeckle::~MultilookDespeckle()
{
}

bool MultilookDespeckle::populateKernel()
{
   int row_size = 11;
   int col_size = 11;
   if(!isBatch())
   {
      bool ok = true;
      row_size = QInputDialog::getInteger(Service<DesktopServices>()->getMainWidget(), "Filter size",
         "Select low-pass filter size (rows).", 11, 1, 17, 2, &ok);
      if(ok)
      {
         col_size = QInputDialog::getInteger(Service<DesktopServices>()->getMainWidget(), "Filter size",
            "Select low-pass filter size (columns).", row_size, 1, 17, 2, &ok);
      }
      if(!ok)
      {
         mProgress.report("User aborted.", 0, ABORT, true);
         return false;
      }
   }
   mKernel = NEWMAT::Matrix(row_size, col_size);
   mKernel *= 1.0;
   mKernelImag = NEWMAT::Matrix(row_size, col_size);
   return true;
}