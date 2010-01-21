/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "GeointVersion.h"
#include "FlattenAoi.h"
#include "AoiToolsFactory.h"

AOITOOLSFACTORY(FlattenAoi);

FlattenAoi::FlattenAoi()
{
   setName("FlattenAoi");
   setDescription("Flatten an AOI into a bilevel raster cube.");
   setDescriptorId("{CE84BE92-12A2-4D7F-8A80-86D2569FBD7F}");
   setCopyright(GEOINT_COPYRIGHT);
   setVersion(GEOINT_VERSION_NUMBER);
   setProductionStatus(GEOINT_IS_PRODUCTION_RELEASE);
   setSubtype("AOI");
   addMenuLocation("[General Algorithms]/Flatten AOI");
}

FlattenAoi::~FlattenAoi()
{
}

bool FlattenAoi::getInputSpecification(PlugInArgList *&pInArgList)
{
   return false;
}

bool FlattenAoi::getOutputSpecification(PlugInArgList *&pOutArgList)
{
   return false;
}

bool FlattenAoi::execute(PlugInArgList *pInArgList, PlugInArgList *pOutArgList)
{
   return false;
}