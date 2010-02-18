/*
 * The information in this file is
 * Copyright(c) 2010 Trevor R.H. Clarke
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "OpticalFlow.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "ProgressTracker.h"
#include "RasterElement.h"
#include "RasterDataDescriptor.h"
#include "RasterLayer.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "StringUtilities.h"

#include "imgfeatures.h"
#include "kdtree.h"
#include "sift.h"
#include "xform.h"

#include <opencv/cv.h>
#include <stdarg.h>

REGISTER_PLUGIN_BASIC(Tracking, OpticalFlow);

OpticalFlow::OpticalFlow()
{
   setName("OpticalFlow");
   setDescription("Calculate optical flow field.");
   setDescriptorId("{38c907e0-1a57-11df-8a39-0800200c9a66}");
   setMenuLocation("[Tracking]/Optical Flow");
   setType("Algorithm");
   setSubtype("Video");
}

OpticalFlow::~OpticalFlow()
{
}

bool OpticalFlow::getInputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return true;
}

bool OpticalFlow::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return true;
}

bool OpticalFlow::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   return true;
}
