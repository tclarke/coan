/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef MULTILOOKDESPECKLE_H__
#define MULTILOOKDESPECKLE_H__

#include "ConvolutionFilterShell.h"

class MultilookDespeckle : public ConvolutionFilterShell
{
public:
   MultilookDespeckle();
   virtual ~MultilookDespeckle();

protected:
   virtual bool populateKernel();
};

#endif