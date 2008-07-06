/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "BackgroundSuppressionShell.h"
#include "DataRequest.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "Progress.h"
#include "ProgressTracker.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "SpatialDataView.h"
#include "StringUtilities.h"

BackgroundSuppressionShell::BackgroundSuppressionShell() : mCurrentFrame(0), mpRaster(NULL), mSingleForegroundMask(false)
{
   setSubtype("Background Estimation");
   setAbortSupported(true);
   setWizardSupported(true);
}

BackgroundSuppressionShell::~BackgroundSuppressionShell()
{
}

bool BackgroundSuppressionShell::getInputSpecification(PlugInArgList *&pArgList)
{
   VERIFY(pArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg(), NULL));
   VERIFY(pArgList->addArg<RasterElement>(Executable::DataElementArg()));
   VERIFY(pArgList->addArg<SpatialDataView>(Executable::ViewArg()));
   return true;
}

bool BackgroundSuppressionShell::getOutputSpecification(PlugInArgList *&pArgList)
{
   pArgList = NULL;
   return true;
}

bool BackgroundSuppressionShell::execute(PlugInArgList *pInArgList, PlugInArgList *pOutArgList)
{
   if(pInArgList == NULL)
   {
      return false;
   }
   mProgress = ProgressTracker(pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg()),
      "Suppressing background", "temporal", "{01F54B12-0323-4ac4-8C04-C89F1C45EAD9}");
   mpRaster = pInArgList->getPlugInArgValue<RasterElement>(Executable::DataElementArg());
   if(mpRaster == NULL)
   {
      mProgress.report("No raster element specified.", 0, ERRORS, true);
      return false;
   }
   mpView = pInArgList->getPlugInArgValue<SpatialDataView>(Executable::ViewArg());
   RasterDataDescriptor *pDescriptor = static_cast<RasterDataDescriptor*>(mpRaster->getDataDescriptor());
   VERIFY(pDescriptor != NULL);
   unsigned int totalFrames = pDescriptor->getBandCount();
   mCurrentFrame = 0;
   for(InitializeReturnType rval = INIT_CONTINUE; rval == INIT_CONTINUE; mCurrentFrame++)
   {
      rval = initializeModel();
      if(rval == INIT_ERROR)
      {
         return false;
      }
   }
   const double progressStep = 25.0 / (totalFrames - mCurrentFrame);
   double currentProgress = progressStep;
   for(; mCurrentFrame < totalFrames; mCurrentFrame++)
   {
      if(isAborted())
      {
         try
         {
            mProgress.abort();
         }
         catch(const AlgorithmAbort&)
         {
         }
         cleanup(false);
         return false;
      }
      std::string frameMessage = " frame " + StringUtilities::toDisplayString(mCurrentFrame);
      mProgress.report("Preprocess" + frameMessage, static_cast<int>(currentProgress), NORMAL);
      if(!preprocess())
      {
         cleanup(false);
         return false;
      }
      currentProgress += progressStep;
      mProgress.report("Update background model for" + frameMessage, static_cast<int>(currentProgress), NORMAL);
      if(!updateBackgroundModel())
      {
         cleanup(false);
         return false;
      }
      currentProgress += progressStep;
      mProgress.report("Extract foreground from" + frameMessage, static_cast<int>(currentProgress), NORMAL);
      if(!extractForeground())
      {
         cleanup(false);
         return false;
      }
      currentProgress += progressStep;
      mProgress.report("Validate" + frameMessage, static_cast<int>(currentProgress), NORMAL);
      if(!validate())
      {
         cleanup(false);
         return false;
      }
      currentProgress += progressStep;
   }
   if(!displayResults())
   {
      cleanup(false);
      mProgress.report("Unable to display results.", 0, ERRORS, true);
      return false;
   }

   cleanup(true);
   mProgress.report("Extraction complete", 100, NORMAL);
   mProgress.upALevel();
   return true;
}

void BackgroundSuppressionShell::cleanup(bool success)
{
}

bool BackgroundSuppressionShell::displayResults()
{
   return true;
}

void BackgroundSuppressionShell::setSingleForegroundMask(bool single)
{
   mSingleForegroundMask = single;
}

unsigned int BackgroundSuppressionShell::getCurrentFrame() const
{
   return mCurrentFrame;
}

RasterElement *BackgroundSuppressionShell::getRasterElement() const
{
   return mpRaster;
}

DataAccessor BackgroundSuppressionShell::getCurrentFrameAccessor() const
{
   if(mpRaster == NULL)
   {
      return DataAccessor(NULL, NULL);
   }
   RasterDataDescriptor *pDesc = static_cast<RasterDataDescriptor*>(mpRaster->getDataDescriptor());
   VERIFYRV(pDesc != NULL, DataAccessor(NULL, NULL));
   FactoryResource<DataRequest> request;
   VERIFYRV(request.get() != NULL, DataAccessor(NULL, NULL));
   request->setInterleaveFormat(BSQ);
   DimensionDescriptor band = pDesc->getBands()[mCurrentFrame];
   request->setBands(band, band, 1);
   return mpRaster->getDataAccessor(request.release());
}

SpatialDataView *BackgroundSuppressionShell::getView() const
{
   return mpView;
}