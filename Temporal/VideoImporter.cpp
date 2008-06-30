/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "Animation.h"
#include "AnimationController.h"
#include "AnimationServices.h"
#include "AppVerify.h"
#include "DataRequest.h"
#include "DynamicObject.h"
#include "FileResource.h"
#include "ImportDescriptor.h"
#include "LayerList.h"
#include "MessageLogResource.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInResource.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "RasterLayer.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "TimelineWidget.h"
#include "TypeConverter.h"
#include "VideoImporter.h"
#include <boost/rational.hpp>
#include <QtCore/QString>
#include <string>
#include <vector>

namespace
{
   QString logBuffer;
};

extern "C" static void av_log_callback(void *pPtr, int level, const char *pFmt, va_list vl)
{
   if(level > av_log_level)
   {
      return;
   }
   logBuffer.vsprintf(pFmt, vl);
}

FfmpegRasterPager::FfmpegRasterPager() : mBytesRemaining(0), mpRawPacketData(NULL),
                                         mpFormatCtx(NULL), mpCodecCtx(NULL), mStreamId(-1)
{
   mPacket.data = NULL;
   setName("FfmpegRasterPager");
   setCopyright("Copyright 2008 BATC");
   setCreator("Ball Aerospace & Technologies Corp.");
   setDescription("Provides access to on-disk video data via ffmpeg");
   setDescriptorId("{e7293f1b-354e-4fc1-836f-5ca5df6d193a}");
   setVersion("0.1");
   setProductionStatus(false);
   setShortDescription("On-disk video");
}

FfmpegRasterPager::~FfmpegRasterPager()
{
   if(mpFormatCtx != NULL || mpCodecCtx != NULL)
   {
      avcodec_close(mpCodecCtx);
      av_close_input_file(mpFormatCtx);
   }
}

bool FfmpegRasterPager::getInputSpecification(PlugInArgList *&pArgList)
{
   if(!CachedPager::getInputSpecification(pArgList))
   {
      return false;
   }
   pArgList->addArg("Format Context", NULL);
   pArgList->addArg("Codec Context", NULL);
   return true;
}

bool FfmpegRasterPager::parseInputArgs(PlugInArgList *pInputArgList)
{
   if(!CachedPager::parseInputArgs(pInputArgList))
   {
      return false;
   }
   mpFormatCtx = pInputArgList->getPlugInArgValueUnsafe<AVFormatContext>("Format Context");
   mpCodecCtx = pInputArgList->getPlugInArgValueUnsafe<AVCodecContext>("Codec Context");
   mStreamId = QString::fromStdString(getRasterElement()->getDataDescriptor()->getFileDescriptor()->getDatasetLocation()).toInt();
   return (mpFormatCtx != NULL) && (mpCodecCtx != NULL);
}

bool FfmpegRasterPager::openFile(const std::string& filename)
{
   return true;
}

CachedPage::UnitPtr FfmpegRasterPager::fetchUnit(DataRequest *pOriginalRequest)
{
   unsigned int startRow = pOriginalRequest->getStartRow().getOriginalNumber();
   unsigned int startColumn = pOriginalRequest->getStartColumn().getOriginalNumber();
   int frameNum = pOriginalRequest->getStartBand().getOriginalNumber();
   int64_t frameOffset = startRow * mpCodecCtx->width + startColumn;
   int64_t fullFrameSize = mpCodecCtx->width * mpCodecCtx->height;
   int64_t frameSize = fullFrameSize - frameOffset;
   boost::shared_ptr<CachedPage::CacheUnit> pUnit;

   if(frameNum != mpCodecCtx->frame_number)
   {
      if(mFrameTimes.empty())
      {
         const DynamicObject *pMetadata = getRasterElement()->getDataDescriptor()->getMetadata();
         mFrameTimes = dv_cast<std::vector<double> >(pMetadata->getAttributeByPath(FRAME_TIMES_METADATA_PATH));
      }
      if(mFrameTimes.empty())
      {
         return pUnit;
      }
      double timeInSeconds = mFrameTimes[frameNum];
      AVRational bq = AV_TIME_BASE_Q;
      int64_t seekTarget = av_rescale_q(timeInSeconds * AV_TIME_BASE, bq, mpFormatCtx->streams[mStreamId]->time_base);
      if(av_seek_frame(mpFormatCtx, mStreamId, seekTarget, AVSEEK_FLAG_BACKWARD) < 0)
      {
         return pUnit;
      }
   }
   do
   {
      if(!getNextFrame())
      {
         return pUnit;
      }
   }
   while(mpCodecCtx->frame_number <= frameNum);
   char *pData = new char[frameSize];

   AVPicture *pFrameGray = reinterpret_cast<AVPicture*>(avcodec_alloc_frame());
   uint8_t *pBuf = new uint8_t[avpicture_get_size(PIX_FMT_GRAY8, mpCodecCtx->width, mpCodecCtx->height)];
   avpicture_fill(pFrameGray, pBuf, PIX_FMT_GRAY8, mpCodecCtx->width, mpCodecCtx->height);
   img_convert(pFrameGray, PIX_FMT_GRAY8, reinterpret_cast<AVPicture*>(&mNextFrame),
         mpCodecCtx->pix_fmt, mpCodecCtx->width, mpCodecCtx->height);

   // copy the data to the page
   memcpy(pData, pFrameGray->data[0] + frameOffset, frameSize);

   av_free(pFrameGray);
   pFrameGray = NULL;

   pUnit.reset(new CachedPage::CacheUnit(pData,
                                         pOriginalRequest->getStartRow(),
                                         mpCodecCtx->height - startRow,
                                         frameSize,
                                         pOriginalRequest->getStartBand()));
   return pUnit;
}

bool FfmpegRasterPager::getNextFrame()
{
   int bytesDecoded = 0;
   int frameFinished = 0;
   for(bool streamFinished = false; !streamFinished; ) // decode packets until we have a complete frame
   {
      while(mBytesRemaining > 0) // decode a packet
      {
         bytesDecoded = avcodec_decode_video(mpCodecCtx, &mNextFrame, &frameFinished, mpRawPacketData, mBytesRemaining);
         if(bytesDecoded < 0)
         {
            MessageResource msg("FFMPEG Raster Pager", "temporal", "c83d182b-4e54-49e5-83fa-beff11f8c99c");
            msg->addProperty("detail", "Unable to decode video frame.");
            return false;
         }
         mBytesRemaining -= bytesDecoded;
         mpRawPacketData += bytesDecoded;
         if(frameFinished != 0)
         {
            return true;
         }
      }

      do // read the next packet in the stream
      {
         if(mPacket.data != NULL)
         {
            av_free_packet(&mPacket);
         }
         if(av_read_packet(mpFormatCtx, &mPacket) < 0)
         {
            streamFinished = true;
            break;
         }
      }
      while(mPacket.stream_index != mStreamId);

      if(!streamFinished)
      {
         mBytesRemaining = mPacket.size;
         mpRawPacketData = mPacket.data;
      }
   }

   // decode the remainder of the last frame
   bytesDecoded = avcodec_decode_video(mpCodecCtx, &mNextFrame, &frameFinished, mpRawPacketData, mBytesRemaining);
   if(mPacket.data != NULL)
   {
      av_free_packet(&mPacket);
   }
   return frameFinished != 0;
}

VideoImporter::VideoImporter() : mpFormatCtx(NULL), mpCodecCtx(NULL)
{
   logBuffer.clear();
   av_log_set_callback(av_log_callback);
   av_register_all();

   setDescriptorId("{4d4d06b4-fa58-4cb2-b01b-9a027876258f}");
   setName("Video Importer");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright("Copyright 2008, BATC");
   setVersion("0.1");
   setProductionStatus(false);
   setExtensions("Video Files (*.mpg *.avi)");
}

VideoImporter::~VideoImporter()
{
   if(mpFormatCtx != NULL || mpCodecCtx != NULL)
   {
      avcodec_close(mpCodecCtx);
      av_close_input_file(mpFormatCtx);
   }
}

std::vector<ImportDescriptor*> VideoImporter::getImportDescriptors(const std::string &filename)
{
   std::vector<ImportDescriptor*> descriptors;
   if(mpFormatCtx != NULL || mpCodecCtx != NULL)
   {
      avcodec_close(mpCodecCtx);
      av_close_input_file(mpFormatCtx);
      mpFormatCtx = NULL;
      mpCodecCtx = NULL;
   }

   if(av_open_input_file(&mpFormatCtx, filename.c_str(), NULL, 0, NULL) != 0 ||
      av_find_stream_info(mpFormatCtx) < 0)
   {
      return descriptors;
   }
   for(int streamId = 0; streamId < mpFormatCtx->nb_streams; streamId++)
   {
      if(mpFormatCtx->streams[streamId]->codec->codec_type == CODEC_TYPE_VIDEO)
      {
         mpCodecCtx = mpFormatCtx->streams[streamId]->codec;
         AVCodec *pCodec = avcodec_find_decoder(mpCodecCtx->codec_id);
         VERIFYRV(pCodec != NULL, descriptors);
         if(pCodec->capabilities & CODEC_CAP_TRUNCATED)
         {
            mpCodecCtx->flags |= CODEC_FLAG_TRUNCATED;
         }
         if(avcodec_open(mpCodecCtx, pCodec) < 0)
         {
            return descriptors;
         }

         ImportDescriptorResource pImportDescriptor(filename, TypeConverter::toString<RasterElement>());
         VERIFYRV(pImportDescriptor.get() != NULL, descriptors);

         std::vector<double> frameTimes;
         boost::rational<int> minFrameTime(mpFormatCtx->streams[streamId]->r_frame_rate.den,
                                           mpFormatCtx->streams[streamId]->r_frame_rate.num);
         boost::rational<int> timeBase(mpFormatCtx->streams[streamId]->time_base.num,
                                       mpFormatCtx->streams[streamId]->time_base.den);
         unsigned int frameCnt = mpFormatCtx->streams[streamId]->nb_frames;
         if(frameCnt == 0)
         {
            // need to estimate the number of frames
            // this might be too many but it should be sufficient
            boost::rational<int> duration = mpFormatCtx->streams[streamId]->duration * timeBase;
            boost::rational<int> minFrameRate = 1 / minFrameTime;
            frameCnt = static_cast<unsigned int>(boost::rational_cast<double>(minFrameRate * duration) + 0.5);
         }
         frameTimes.reserve(frameCnt);
         double startTime = boost::rational_cast<double>(mpFormatCtx->streams[streamId]->start_time * timeBase);
         for(unsigned int frameNum = 0; frameNum < frameCnt; frameNum++)
         {
            double offset = boost::rational_cast<double>(frameNum * minFrameTime);
            frameTimes.push_back(startTime + offset);
         }

         RasterDataDescriptor *pDesc = 
            RasterUtilities::generateRasterDataDescriptor(filename + QString(":%1").arg(streamId).toStdString(),
                     NULL, mpCodecCtx->height, mpCodecCtx->width, frameCnt,
                     BSQ, INT1UBYTE, ON_DISK_READ_ONLY);
         VERIFYRV(pDesc != NULL, descriptors);
         pImportDescriptor->setDataDescriptor(pDesc);
         RasterUtilities::generateAndSetFileDescriptor(pDesc, filename, QString::number(streamId).toStdString(), LITTLE_ENDIAN);
         FactoryResource<DynamicObject> pMetadata;
         VERIFYRV(pMetadata.get() != NULL, descriptors);
         pMetadata->setAttributeByPath(FRAME_TIMES_METADATA_PATH, frameTimes);
         pDesc->setMetadata(pMetadata.release());
         descriptors.push_back(pImportDescriptor.release());
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

bool VideoImporter::validateDefaultOnDiskReadOnly(const DataDescriptor *pDescriptor, std::string &errorMessage) const
{
   const RasterDataDescriptor *pRasterDescriptor = dynamic_cast<const RasterDataDescriptor*>(pDescriptor);
   if(pRasterDescriptor == NULL)
   {
      errorMessage = "The data descriptor is invalid.";
      return false;
   }

   const RasterFileDescriptor *pFileDescriptor =
      dynamic_cast<const RasterFileDescriptor*>(pRasterDescriptor->getFileDescriptor());
   if(pFileDescriptor == NULL)
   {
      errorMessage = "The file descriptor is invalid.";
      return false;
   }

   ProcessingLocation loc = pDescriptor->getProcessingLocation();
   if (loc == ON_DISK_READ_ONLY)
   {
      // Invalid filename
      const Filename &filename = pFileDescriptor->getFilename();
      if(filename.getFullPathAndName().empty())
      {
         errorMessage = "The filename is invalid.";
         return false;
      }

      // Existing file
      LargeFileResource file;
      if(!file.open(filename.getFullPathAndName(), O_RDONLY | O_BINARY, S_IREAD))
      {
         errorMessage = "The file: " + std::string(filename) + " does not exist.";
         return false;
      }

      // Interleave conversions
      InterleaveFormatType dataInterleave = pRasterDescriptor->getInterleaveFormat();
      InterleaveFormatType fileInterleave = pFileDescriptor->getInterleaveFormat();
      if(dataInterleave != fileInterleave)
      {
         errorMessage = "Interleave format conversions are not supported with on-disk read-only processing.";
         return false;
      }

      //Subset
      unsigned int loadedBands = pRasterDescriptor->getBandCount();
      unsigned int fileBands = pFileDescriptor->getBandCount();

      if(loadedBands != fileBands)
      {
         errorMessage = "Band subsets are not supported with on-disk read-only processing.";
         return false;
      }

      unsigned int skipFactor = 0;
      if(!RasterUtilities::determineSkipFactor(pRasterDescriptor->getRows(), skipFactor) || skipFactor != 0)
      {
         errorMessage = "Skip factors are not supported for rows or columns with on-disk read-only processing.";
         return false;
      }
      if(!RasterUtilities::determineSkipFactor(pRasterDescriptor->getColumns(), skipFactor) || skipFactor != 0)
      {
         errorMessage = "Skip factors are not supported for rows or columns with on-disk read-only processing.";
         return false;
      }
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
   StepResource pStep("Execute Video Importer", "temporal", "2d4f3f82-08f1-4f04-acc1-105dca4e1dcf", "Execute failed");

   if(!parseInputArgList(pInArgList))
   {
      return false;
   }

   // Update the log and progress with the start of the import
   Progress *pProgress = getProgress();
   if(pProgress != NULL)
   {
      pProgress->updateProgress("Video Import Started", 1, NORMAL);
   }

   if(!performImport())
   {
      return false;
   }

   RasterElement *pElement = getRasterElement();
   VERIFY(pElement != NULL);

   // Create the view
   RasterLayer *pLayer = NULL;
   if(!isBatch() && !Service<SessionManager>()->isSessionLoading())
   {
      SpatialDataView *pView = createView();
      if(pView == NULL)
      {
         pStep->finalize(Message::Failure, "The view could not be created.");
         return false;
      }

      // Add the view to the output arg list
      if(pOutArgList != NULL)
      {
         pOutArgList->setPlugInArgValue("View", pView);
      }
      pLayer = static_cast<RasterLayer*>(pView->getLayerList()->getLayer(RASTER, pElement));
   }

   if(pLayer != NULL)
   {
      // create an animation
      RasterDataDescriptor *pDescriptor = static_cast<RasterDataDescriptor*>(pElement->getDataDescriptor());
      VERIFY(pDescriptor != NULL);
      RasterFileDescriptor *pFileDescriptor = static_cast<RasterFileDescriptor*>(pDescriptor->getFileDescriptor());
      VERIFY(pFileDescriptor != NULL);
      std::string fname = pElement->getFilename();
      AnimationController *pController = Service<AnimationServices>()->getAnimationController(fname);
      if(pController == NULL)
      {
         pController = Service<AnimationServices>()->createAnimationController(fname, FRAME_TIME);
      }
      if(pController == NULL)
      {
         pStep->finalize(Message::Failure, "Unable to create animation.");
         return false;
      }
      if(!TimelineUtils::createAnimationForRasterLayer(pLayer, pController))
      {
         pStep->finalize(Message::Failure, "Unable to create animation.");
         return false;
      }
      AnimationToolBar *pToolBar = static_cast<AnimationToolBar*>(Service<DesktopServices>()->getWindow("Animation", TOOLBAR));
      VERIFY(pToolBar != NULL);
      pToolBar->setAnimationController(pController);
   }

   if(pProgress != NULL)
   {
      pProgress->updateProgress("Video Import Complete.", 100, NORMAL);
   }

   pStep->finalize(Message::Success);
   return true;
}

bool VideoImporter::createRasterPager(RasterElement *pRaster) const
{
   VERIFY(pRaster != NULL);
   DataDescriptor *pDescriptor = pRaster->getDataDescriptor();
   VERIFY(pDescriptor != NULL);
   FileDescriptor *pFileDescriptor = pDescriptor->getFileDescriptor();
   VERIFY(pFileDescriptor != NULL);

   std::string filename = pRaster->getFilename();
   Progress *pProgress = getProgress();

   StepResource pStep("Create pager for Video Importer", "temporal", "813e8530-23a0-4a44-b0b5-20c39cd3c3a8");

   FactoryResource<Filename> pFilename;
   pFilename->setFullPathAndName(filename);

   ExecutableResource pagerPlugIn("FfmpegRasterPager", std::string(), pProgress);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedElementArg(), pRaster);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedFilenameArg(), pFilename.get());
   PlugInArg *pArg = NULL;
   pagerPlugIn->getInArgList().getArg("Format Context", pArg);
   if(pArg == NULL)
   {
      return false;
   }
   pArg->setActualValue(mpFormatCtx, false);
   pagerPlugIn->getInArgList().getArg("Codec Context", pArg);
   if(pArg == NULL)
   {
      return false;
   }
   pArg->setActualValue(mpCodecCtx, false);

   bool success = pagerPlugIn->execute();

   RasterPager *pPager = dynamic_cast<RasterPager*>(pagerPlugIn->getPlugIn());
   if(!success || pPager == NULL)
   {
      std::string message = "Execution of FfmpegRasterPager failed!";
      if (pProgress != NULL) pProgress->updateProgress(message, 0, ERRORS);
      pStep->finalize(Message::Failure, message);
      return false;
   }

   pRaster->setPager(pPager);
   pagerPlugIn->releasePlugIn();

   // we've passed ownership onto the pager, so we can reset these to NULL so they are not freed
   mpFormatCtx = NULL;
   mpCodecCtx = NULL;

   pStep->finalize();
   return true;
}