/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AnimationController.h"
#include "AppVerify.h"
#include "BackgroundSuppressionShell.h"
#include "DataAccessorImpl.h"
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
#include "switchOnEncoding.h"

namespace
{
   template<typename T>
   void performBoxFilter(T *pData, int rowCount, int colCount, int slide, BackgroundSuppressionShell::WrapTypeEnum wrap)
   {
      T divisor = static_cast<T>(slide * 2 + 1);
      divisor *= divisor;
      for(int row = 0; row < rowCount; row++)
      {
         for(int col = 0; col < colCount; col++)
         {
            T accum = static_cast<T>(0);
            for(int boxRow = row - slide; boxRow <= row + slide; boxRow++)
            {
               for(int boxCol = col - slide; boxCol <= col + slide; boxCol++)
               {
                  if(boxRow >= 0 && boxCol >= 0 && boxRow < rowCount && boxCol < colCount)
                  {
                     accum += pData[boxRow * colCount + boxCol] / divisor;
                     continue;
                  }
                  switch(wrap)
                  {
                  case BackgroundSuppressionShell::CLAMP:
                     // treat as 0
                     break;
                  case BackgroundSuppressionShell::WRAP:
                     // wrap to other side
                     {
                        int tmpRow = (boxRow < 0) ? (rowCount + boxRow) : (boxRow - rowCount);
                        int tmpCol = (boxCol < 0) ? (colCount + boxCol) : (boxCol - colCount);
                        accum += pData[tmpRow * colCount + tmpCol] / divisor;
                        break;
                     }
                  case BackgroundSuppressionShell::EXTEND:
                     // treat as the closest data point
                     {
                        int tmpRow = std::min(std::max(boxRow, 0), rowCount - 1);
                        int tmpCol = std::min(std::max(boxCol, 0), colCount - 1);
                        accum += pData[tmpRow * colCount + tmpCol] / divisor;
                        break;
                     }
                  }
               }
            }
            pData[row * colCount + col] = accum;
         }
      }
   }
}

BackgroundSuppressionShell::BackgroundSuppressionShell() :
            mCurrentFrame(0), mCurrentProgress(0.0), mProgressStep(1.0), mpRaster(NULL), mSingleForegroundMask(false), mpTemporaryBuffer(NULL)
{
   setSubtype("Background Estimation");
   setAbortSupported(true);
   setWizardSupported(true);
   mpAnimation.addSignal(SIGNAL_NAME(Animation, FrameChanged), Slot(this, &BackgroundSuppressionShell::processNextStreamingFrame));
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
   if(!isBatch())
   {
      VERIFY(pArgList->addArg<Animation>("animation", NULL));
   }
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
   mProgressStep = 25.0 / (totalFrames - mCurrentFrame);
   mCurrentProgress = mProgressStep;
   if(isBatch())
   {
      mIsStreaming = true;
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
         if(!processFrame())
         {
            cleanup(false);
            return false;
         }
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
   }
   else
   {
      mIsStreaming = true;
      // streaming mode
      mpAnimation.reset(pInArgList->getPlugInArgValue<Animation>("animation"));
      if(mpAnimation.get() == NULL)
      {
         AnimationController *pController = mpView->getAnimationController();
         if(pController != NULL)
         {
            const std::vector<Animation*> &animations = pController->getAnimations();
            for(std::vector<Animation*>::const_iterator animation = animations.begin(); animation != animations.end(); ++animation)
            {
               if((*animation)->getName() == mpView->getName())
               {
                  mpAnimation.reset(*animation);
                  break;
               }
            }
         }
      }
      if(mpAnimation.get() == NULL)
      {
         mProgress.report("Not animation specified, unable to perform streaming execution. Provide and animation or run in batch mode.",
            0, ERRORS, true);
         return false;
      }
      mCurrentFrame = 0;
      const AnimationFrame *pFrame = mpAnimation->getCurrentFrame();
      if(pFrame != NULL)
      {
         mCurrentFrame = pFrame->mFrameNumber;
      }
      if(!processFrame() || !displayResults())
      {
         mProgress.report("Unable to initialize streaming mode.", 0, ERRORS, true);
         return false;
      }
      mProgress.report("Streaming mode setup complete.", 100, NORMAL);
      mProgress.upALevel();
      mProgress.initialize(NULL, "Suppressing background", "temporal", "{01F54B12-0323-4ac4-8C04-C89F1C45EAD9}");
      destroyAfterExecute(false);
   }
   return true;
}

void BackgroundSuppressionShell::processNextStreamingFrame(Subject &subject, const std::string &signal, const boost::any &data)
{
   AnimationFrame *pFrame = boost::any_cast<AnimationFrame*>(data);
   if(pFrame != NULL)
   {
      mCurrentFrame = pFrame->mFrameNumber;
      processFrame() && displayResults();
   }
}

bool BackgroundSuppressionShell::processFrame()
{
   std::string frameMessage = " frame " + StringUtilities::toDisplayString(mCurrentFrame);
   mProgress.report("Preprocess" + frameMessage, static_cast<int>(mCurrentProgress), NORMAL);
   if(!preprocess())
   {
      return false;
   }
   mCurrentProgress += mProgressStep;
   mProgress.report("Update background model for" + frameMessage, static_cast<int>(mCurrentProgress), NORMAL);
   if(!updateBackgroundModel())
   {
      return false;
   }
   mCurrentProgress += mProgressStep;
   mProgress.report("Extract foreground from" + frameMessage, static_cast<int>(mCurrentProgress), NORMAL);
   if(!extractForeground())
   {
      return false;
   }
   mCurrentProgress += mProgressStep;
   mProgress.report("Validate" + frameMessage, static_cast<int>(mCurrentProgress), NORMAL);
   if(!validate())
   {
      return false;
   }
   mCurrentProgress += mProgressStep;
   return true;
}

bool BackgroundSuppressionShell::preprocess()
{
   if(mpRaster == NULL)
   {
      return false;
   }
   RasterDataDescriptor *pDesc = static_cast<RasterDataDescriptor*>(mpRaster->getDataDescriptor());
   if(mpTemporaryBuffer.get() == NULL)
   {
      mpTemporaryBuffer.reset(new char[pDesc->getRowCount() * pDesc->getColumnCount() * pDesc->getBytesPerElement()]);
      if(mpTemporaryBuffer.get() == NULL)
      {
         return false;
      }
   }
   DataAccessor frameAccessor = getCurrentFrameAccessor();
   size_t rowSize = pDesc->getColumnCount() * pDesc->getBytesPerElement();
   for(unsigned int row = 0; row < pDesc->getRowCount(); row++)
   {
      if(!frameAccessor.isValid())
      {
         return false;
      }
      memcpy(mpTemporaryBuffer.get() + row * rowSize, frameAccessor->getRow(), rowSize);
      frameAccessor->nextColumn();
   }
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

bool BackgroundSuppressionShell::boxFilter(int size, BackgroundSuppressionShell::WrapTypeEnum wrap)
{
   if(size <= 1 || size > 9 || size % 2 == 0)
   {
      return false;
   }
   if(mpTemporaryBuffer.get() == NULL)
   {
      return false;
   }
   RasterDataDescriptor *pDesc = static_cast<RasterDataDescriptor*>(mpRaster->getDataDescriptor());
   switchOnEncoding(pDesc->getDataType(), performBoxFilter,
      mpTemporaryBuffer.get(), static_cast<int>(pDesc->getRowCount()),
      static_cast<int>(pDesc->getColumnCount()), static_cast<int>((size - 1) / 2), wrap);
   return true;
}

void *BackgroundSuppressionShell::getTemporaryFrameBuffer()
{
   return mpTemporaryBuffer.get();
}