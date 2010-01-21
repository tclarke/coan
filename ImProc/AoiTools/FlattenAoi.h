/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef FLATTENAOI_H__
#define FLATTENAOI_H__

#include "AlgorithmShell.h"

class FlattenAoi : public AlgorithmShell
{
public:
   FlattenAoi();
   virtual ~FlattenAoi();

   virtual bool getInputSpecification(PlugInArgList*& pInArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pOutArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
};

#endif
