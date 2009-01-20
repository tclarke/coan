/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AnimationController.h"
#include "AnimationServices.h"
#include "AnimationToolBar.h"
#include "AppVerify.h"
#include "DesktopServices.h"
#include "ImportDescriptor.h"
#include "LayerList.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInFactory.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "RasterLayer.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "StreamManager.h"
#include "StringUtilities.h"
#include "TypeConverter.h"
#include "Undo.h"
#include "UtilityServices.h"
#include "VideoImporter.h"
#include "VideoStream.h"
#include "VideoUtils.h"
#include "VideoVersion.h"

#include <string>
#include <vector>

PLUGINFACTORY(VideoImporter);

VideoImporter::VideoImporter()
{
   av_register_all();

   setDescriptorId("{4d4d06b4-fa58-4cb2-b01b-9a027876258f}");
   setName("Video Importer");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(VIDEO_COPYRIGHT);
   setVersion(VIDEO_VERSION_NUMBER);
   setProductionStatus(VIDEO_IS_PRODUCTION_RELEASE);
   setExtensions("Video Files (*.mpg *.avi)");
}

VideoImporter::~VideoImporter()
{
}

class AVFormatResource
{
public:
   AVFormatResource() : mpFormat(NULL) {}
   ~AVFormatResource() { reset(NULL); }
   void reset(AVFormatContext* pFormat)
   {
      if (mpFormat != NULL)
      {
         av_close_input_file(mpFormat);
      }
      mpFormat = pFormat;
   }
   AVFormatContext* get() { return mpFormat; }
   AVFormatContext* operator->() { return get(); }
   operator AVFormatContext*() { return get(); }

private:
   AVFormatContext* mpFormat;
};

std::vector<ImportDescriptor*> VideoImporter::getImportDescriptors(const std::string &filename)
{
   std::vector<ImportDescriptor*> descriptors;
   AVFormatResource pFormatCtx;
   { // scope
      AVFormatContext* pTmp = NULL;
      if (av_open_input_file(&pTmp, filename.c_str(), NULL, 0, NULL) != 0)
      {
         return descriptors;
      }
      pFormatCtx.reset(pTmp);
   }
   if (av_find_stream_info(pFormatCtx) < 0)
   {
      return descriptors;
   }
   for(int streamId = 0; streamId < pFormatCtx->nb_streams; streamId++)
   {
      if(pFormatCtx->streams[streamId]->codec->codec_type == CODEC_TYPE_VIDEO)
      {
         AVCodecContext* pCodecCtx = pFormatCtx->streams[streamId]->codec;
         AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
         VERIFYRV(pCodec != NULL, descriptors);
         if(pCodec->capabilities & CODEC_CAP_TRUNCATED)
         {
            pCodecCtx->flags |= CODEC_FLAG_TRUNCATED;
         }
         if(avcodec_open(pCodecCtx, pCodec) < 0)
         {
            return descriptors;
         }

         ImportDescriptorResource pStreamDescriptor(filename, "VideoStream");
         VERIFYRV(pStreamDescriptor.get() != NULL, descriptors);
         pStreamDescriptor->getDataDescriptor()->setProcessingLocation(ON_DISK_READ_ONLY);
         RasterUtilities::generateAndSetFileDescriptor(pStreamDescriptor->getDataDescriptor(), filename,
            StringUtilities::toDisplayString(streamId), LITTLE_ENDIAN);
         std::string rasterName = filename + QString(":%1").arg(streamId).toStdString();
         ImportDescriptorResource pRasterDescriptor(rasterName,
                                                    TypeConverter::toString<RasterElement>(),
                                                    std::vector<std::string>(1, filename));
         VERIFYRV(pRasterDescriptor.get() != NULL, descriptors);

         RasterDataDescriptor* pDesc = static_cast<RasterDataDescriptor*>(pRasterDescriptor->getDataDescriptor());

         std::vector<DimensionDescriptor> rowDims = RasterUtilities::generateDimensionVector(pCodecCtx->height);
         pDesc->setRows(rowDims);
         std::vector<DimensionDescriptor> colDims = RasterUtilities::generateDimensionVector(pCodecCtx->width);
         pDesc->setColumns(colDims);
         std::vector<DimensionDescriptor> bandDims = RasterUtilities::generateDimensionVector(3);
         pDesc->setBands(bandDims);
         pDesc->setInterleaveFormat(BIP);
         pDesc->setDataType(INT1UBYTE);
         pDesc->setProcessingLocation(IN_MEMORY);
         pDesc->setDisplayMode(RGB_MODE);
         pDesc->setDisplayBand(RED, pDesc->getActiveBand(0));
         pDesc->setDisplayBand(GREEN, pDesc->getActiveBand(1));
         pDesc->setDisplayBand(BLUE, pDesc->getActiveBand(2));
         RasterUtilities::generateAndSetFileDescriptor(pDesc, filename, StringUtilities::toDisplayString(streamId), LITTLE_ENDIAN);

         descriptors.push_back(pStreamDescriptor.release());
         descriptors.push_back(pRasterDescriptor.release());
      }
   }
   return descriptors;
}

unsigned char VideoImporter::getFileAffinity(const std::string &filename)
{
   if(getImportDescriptors(filename).empty())
   {
      return Importer::CAN_NOT_LOAD;
   }
   return Importer::CAN_LOAD;
}

bool VideoImporter::validate(const DataDescriptor* pDescriptor, std::string& errorMessage) const
{
   if (pDescriptor == NULL || !ImporterShell::validate(pDescriptor, errorMessage))
   {
      return false;
   }
   if (pDescriptor->getType() == TypeConverter::toString<RasterElement>())
   {
      const RasterDataDescriptor* pRasterDescriptor = dynamic_cast<const RasterDataDescriptor*>(pDescriptor);
      if (pRasterDescriptor == NULL)
      {
         errorMessage = "Invalid raster data descriptor.";
         return false;
      }
      if (pDescriptor->getProcessingLocation() == ON_DISK_READ_ONLY)
      {
         errorMessage = "Video stream raster displays can not be loaded on-disk read-only.";
         return false;
      }
      else if (pDescriptor->getProcessingLocation() == IN_MEMORY)
      {
         uint64_t dataSize = pRasterDescriptor->getRowCount() * pRasterDescriptor->getColumnCount()
            * pRasterDescriptor->getBandCount() * pRasterDescriptor->getBytesPerElement();
         uint64_t maxMemoryAvail = Service<UtilityServices>()->getMaxMemoryBlockSize();
#if PTR_SIZE > 4
         uint64_t totalRam = Service<UtilityServices>()->getTotalPhysicalMemory();
         if (totalRam < maxMemoryAvail)
         {
            maxMemoryAvail = totalRam;
         }
#endif

         if (dataSize > maxMemoryAvail)
         {
            errorMessage = "Cube cannot be loaded into memory, use a different processing location.";
            return false;
         }
      }
   }
   else if (pDescriptor->getType() == "VideoStream")
   {
      if (pDescriptor->getProcessingLocation() != ON_DISK_READ_ONLY)
      {
         errorMessage = "Video streams can only be loaded on-disk read-only.";
         return false;
      }
   }
   else
   {
      errorMessage = "Invalid data type.";
      return false;
   }
   return true;
}

bool VideoImporter::getInputSpecification(PlugInArgList*& pArgList)
{
   VERIFY((pArgList = Service<PlugInManagerServices>()->getPlugInArgList()) != NULL);
   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg(), NULL));
   VERIFY(pArgList->addArg<DataElement>(Importer::ImportElementArg()));
   VERIFY(pArgList->addArg(Importer::SessionLoadArg(), false));
   return true;
}

bool VideoImporter::getOutputSpecification(PlugInArgList*& pArgList)
{
   VERIFY((pArgList = Service<PlugInManagerServices>()->getPlugInArgList()) != NULL);
   if (!isBatch())
   {
      VERIFY(pArgList->addArg<SpatialDataView>("View"));
   }
   return true;
}

bool VideoImporter::execute(PlugInArgList *pInArgList, PlugInArgList *pOutArgList)
{
   if(pInArgList == NULL)
   {
      return false;
   }
   // Create a message log step
   mProgress = ProgressTracker(pInArgList->getPlugInArgValue<Progress>(ProgressArg()),
      "Execute video importer", "coan", "{6385aa93-ff7f-4ee7-9308-b3476aca544b}");

   DataElement* pElmnt = pInArgList->getPlugInArgValue<DataElement>(ImportElementArg());
   mpRasterElement = dynamic_cast<RasterElement*>(pElmnt);
   mpStreamElement = dynamic_cast<Any*>(pElmnt);
   if (mpRasterElement == NULL && mpStreamElement == NULL)
   {
      mProgress.report("The data element input value is invalid.", 0, ERRORS, true);
      return false;
   }

   if (mpRasterElement != NULL)
   {
      mProgress.report("Initialize display.", 1, NORMAL);
      if (!mpRasterElement->createDefaultPager())
      {
         //return checkAbortOrError("Could not allocate resources for new RasterElement", pStep.get());
         return false;
      }
      // schedule load of first frame
      Any* pParent = dynamic_cast<Any*>(mpRasterElement->getParent());
      VERIFY(pParent != NULL);
      VideoStream* pStream = dynamic_cast<VideoStream*>(pParent->getData());
      VERIFY(pStream != NULL);
      if (!pStream->setDisplay(mpRasterElement))
      {
         mProgress.report("Unable to display the stream.", 0, ERRORS, true);
         return false;
      }

      // Create the view
      if(!isBatch() && !Service<SessionManager>()->isSessionLoading())
      {
         SpatialDataView *pView = createView();
         if(pView == NULL)
         {
            mProgress.report("The view could not be created.", 0, ERRORS, true);
            return false;
         }

         // Add the view to the output arg list
         if(pOutArgList != NULL)
         {
            pOutArgList->setPlugInArgValue("View", pView);
         }
      }
      mProgress.report("Display initialization complete.", 100, NORMAL);
   }
   else if (mpStreamElement != NULL)
   {
      mProgress.report("Initialize video stream.", 1, NORMAL);
      // initialize the stream data
      VideoStream* pStream = NULL;
      std::vector<PlugIn*> instances = Service<PlugInManagerServices>()->getPlugInInstances("Video Stream Manager");
      StreamManager* pStreamManager = instances.empty() ? NULL : dynamic_cast<StreamManager*>(instances.front());
      if (pStreamManager != NULL)
      {
         if (pStreamManager->initializeStream(mpStreamElement))
         {
            pStream = dynamic_cast<VideoStream*>(mpStreamElement->getData());
         }
      }
      if (pStream == NULL)
      {
         mProgress.report("Unable to initialize the video stream.", 0, ERRORS, true);
         return false;
      }
      
      // create animation
      AnimationController* pController = Service<AnimationServices>()->getAnimationController(mpStreamElement->getName());
      if (pController == NULL)
      {
         pController = Service<AnimationServices>()->createAnimationController(mpStreamElement->getName(), FRAME_TIME);
      }
      if (pController == NULL)
      {
         mProgress.report("Unable to create animation.", 0, ERRORS, true);
         return false;
      }
      if (!isBatch())
      {
         AnimationToolBar* pToolBar = static_cast<AnimationToolBar*>(Service<DesktopServices>()->getWindow("Animation", TOOLBAR));
         VERIFY(pToolBar);
         pToolBar->setAnimationController(pController);
      }
      Animation* pAnim = pController->createAnimation(mpStreamElement->getName());
      VERIFY(pAnim != NULL);
      if (!pStream->setAnimation(pAnim, pController))
      {
         mProgress.report("Unable to create animation.", 0, ERRORS, true);
         return false;
      }

      mProgress.report("Video stream initialization complete.", 100, NORMAL);
   }

   mProgress.upALevel();
   return true;
}

SpatialDataView* VideoImporter::createView() const
{
   if ((isBatch()) || (mpRasterElement == NULL))
   {
      return NULL;
   }

   mProgress.report("Creating view...", 85, NORMAL);

   // Get the data set name
   std::string name = mpRasterElement->getName();
   if (name.empty())
   {
      mProgress.report("The data set name is invalid!  A view cannot be created.", 0, ERRORS, true);
      return NULL;
   }

   // Create the spatial data window
   SpatialDataWindow* pWindow = static_cast<SpatialDataWindow*>(Service<DesktopServices>()->createWindow(name, SPATIAL_DATA_WINDOW));
   SpatialDataView* pView = (pWindow == NULL) ? NULL : pWindow->getSpatialDataView();
   if (pView == NULL)
   {
      mProgress.report("Could not create the view window!", 0, ERRORS, true);
      return NULL;
   }

   // Set the spatial data in the view
   pView->setPrimaryRasterElement(mpRasterElement);

   // Block undo actions when creating the layers
   UndoLock lock(pView);

   // Add the cube layer
   RasterLayer* pLayer = static_cast<RasterLayer*>(pView->createLayer(RASTER, mpRasterElement));
   if (pLayer == NULL)
   {
      mProgress.report("Could not create the cube layer!", 0, ERRORS, true);
      return NULL;
   }
   pLayer->setStretchUnits(RED, RAW_VALUE);
   pLayer->setStretchUnits(GREEN, RAW_VALUE);
   pLayer->setStretchUnits(BLUE, RAW_VALUE);
   pLayer->setStretchValues(RED, 0, 255);
   pLayer->setStretchValues(GREEN, 0, 255);
   pLayer->setStretchValues(BLUE, 0, 255);
   if (pLayer->isGpuImageSupported())
   {
      pLayer->enableGpuImage(true);
   }
   return pView;
}
