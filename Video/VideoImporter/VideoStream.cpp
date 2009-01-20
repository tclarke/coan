/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AnimationServices.h"
#include "Any.h"
#include "AppAssert.h"
#include "AppVerify.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DynamicObject.h"
#include "KlvItem.h"
#include "ObjectResource.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "StringUtilities.h"
#include "VideoStream.h"
#include <boost/any.hpp>
#include <QtCore/QMutexLocker>

namespace
{
uint64_t videoPktPts = AV_NOPTS_VALUE;

int get_buffer(struct AVCodecContext* pCodec, AVFrame* pPic)
{
   int ret = avcodec_default_get_buffer(pCodec, pPic);
   uint64_t* pPts = reinterpret_cast<uint64_t*>(av_malloc(sizeof(uint64_t)));
   *pPts = videoPktPts;
   pPic->opaque = pPts;
   return ret;
}

void release_buffer(struct AVCodecContext* pCodec, AVFrame* pPic)
{
   if (pPic != NULL)
   {
      av_freep(&pPic->opaque);
   }
   avcodec_default_release_buffer(pCodec, pPic);
}
}

DecodeThread::DecodeThread(AVFormatContext* pFormat, AVCodecContext* pCodec, int streamId) :
         mpFormat(pFormat), mpCodec(pCodec), mStreamId(streamId), mMetaStream(-1), mFillCount(0), mEmptyCount(100), mpKlv(NULL)
{
   size_t dataSize = avpicture_get_size(PIX_FMT_RGB24, mpCodec->width, mpCodec->height);
   for (int i = 0; i < mEmptyCount.available(); i++)
   {
      Entry entry;
      entry.mpPic = reinterpret_cast<AVPicture*>(avcodec_alloc_frame());
      uint8_t* pData = new uint8_t[dataSize];
      avpicture_fill(entry.mpPic, pData, PIX_FMT_RGB24, mpCodec->width, mpCodec->height);
      mBuffer.append(entry);
   }
   mFront = 0;
   mBack = 0;
   for (int s = 0; s < mpFormat->nb_streams; s++)
   {
      if (mpFormat->streams[s]->codec->codec_id == CODEC_ID_KLV_METADATA)
      {
         mMetaStream = s;
         break;
      }
   }
}

DecodeThread::~DecodeThread()
{
   foreach(Entry entry, mBuffer)
   {
      delete entry.mpPic->data[0];
      delete entry.mpPic->data[1];
      delete entry.mpPic->data[2];
      delete entry.mpPic->data[3];
      entry.mpPic->data[0] = entry.mpPic->data[1] = entry.mpPic->data[2] = entry.mpPic->data[3] = NULL;
      av_free(entry.mpPic);
   }
   if (mpCodec != NULL)
   {
      avcodec_close(mpCodec);
   }
   if (mpFormat != NULL)
   {
      av_close_input_file(mpFormat);
   }
}

void DecodeThread::run()
{
   AVFrame* pFrame = avcodec_alloc_frame();
   VERIFYNRV(pFrame);

   double pts = 0;
   AVPacket packet;
   bool endOfFile = false;
   while (!endOfFile)
   {
      // read and decode packets until we have a frame
      int frameFinished = 0;
      while (frameFinished == 0)
      {
         if (av_read_frame(mpFormat, &packet) < 0)
         {
            endOfFile = true;
            break;
         }
         if (packet.stream_index == mStreamId)
         {
            videoPktPts = packet.pts;
            if (avcodec_decode_video(mpCodec, pFrame, &frameFinished, packet.data, packet.size) < 0)
            {
               //endOfFile = true;
               break;
            }
            if (!frameFinished)
            {
               break;
            }
            if (packet.dts == AV_NOPTS_VALUE && pFrame->opaque != NULL && *reinterpret_cast<uint64_t*>(pFrame->opaque) != AV_NOPTS_VALUE)
            {
               pts = static_cast<double>(*reinterpret_cast<uint64_t*>(pFrame->opaque));
            }
            else if (packet.dts != AV_NOPTS_VALUE)
            {
               pts = static_cast<double>(packet.dts);
            }
            else
            {
               pts = 0;
            }
            pts *= av_q2d(mpFormat->streams[mStreamId]->time_base);
         }
         else if(packet.stream_index == mMetaStream)
         {
            QMutexLocker lock(&mMetadataLock);
            QByteArray tmpBuf(QByteArray::fromRawData(reinterpret_cast<const char*>(packet.data), packet.size));
            if (mpKlv.get() == NULL)
            {
               mpKlv.reset(new KlvItem(tmpBuf));
            }
            else
            {
               mpKlv->push_back(tmpBuf);
            }
         }
         av_free_packet(&packet);
      }
      if (frameFinished != 0)
      {
         mEmptyCount.acquire();
         img_convert(mBuffer[mBack].mpPic, PIX_FMT_RGB24, reinterpret_cast<AVPicture*>(pFrame),
            mpCodec->pix_fmt, mpCodec->width, mpCodec->height);
         mBuffer[mBack].mPts = pts;
         mBack = (mBack + 1) & mBuffer.size();
         mFillCount.release();
      }
   }
   av_free(pFrame);
}

void DecodeThread::getMetadata(DynamicObject* pDisplayMetadata)
{
   QMutexLocker lock(&mMetadataLock);
   if (mpKlv.get() == NULL)
   {
      return;
   }
   while (mpKlv.get() != NULL && mpKlv->parseValue())
   {
      // convert KLV to a DynamicObject
      mpKlv->updateDynamicObject(pDisplayMetadata);
      QByteArray tmpBuf(mpKlv->getRemainingData());
      if (tmpBuf.isEmpty())
      {
         mpKlv.reset();
      }
      else
      {
         mpKlv.reset(new KlvItem(tmpBuf));
      }
   }
}

bool DecodeThread::getFrame(double requestedPts, void* pDest)
{
   if (isFinished())
   {
      return false;
   }
   bool found = false;
   while (!found)
   {
      mFillCount.acquire();
      if (mBuffer[mFront].mPts >= requestedPts)
      {
         memcpy(pDest, mBuffer[mFront].mpPic->data[0], avpicture_get_size(PIX_FMT_RGB24, mpCodec->width, mpCodec->height));
         found = true;
      }
      mFront = (mFront + 1) % mBuffer.size();
      mEmptyCount.release();
   }
   return found;
}

VideoStream::VideoStream(Any* pContainer) : mpContainer(pContainer), mpDisplay(NULL),
   mpAnim(NULL), mEstFrameCount(100), mEstFrameDuration(1 / 15.0)
{
   mpAnimationController.addSignal(SIGNAL_NAME(AnimationController, FrameChanged), Slot(this, &VideoStream::currentFrameChanged));
   const DataDescriptor* pDesc = mpContainer->getDataDescriptor();
   ENSURE(pDesc);
   const FileDescriptor* pFile = pDesc->getFileDescriptor();
   ENSURE(pFile);
   
   int streamId = StringUtilities::fromDisplayString<int>(pFile->getDatasetLocation());
   AVFormatContext* pFormat = NULL;
   if (av_open_input_file(&pFormat, pFile->getFilename().getFullPathAndName().c_str(), NULL, 0, NULL) != 0)
   {
      return;
   }
   if (av_find_stream_info(pFormat) < 0)
   {
      return;
   }
   AVCodecContext* pCodecCtx = pFormat->streams[streamId]->codec;
   pCodecCtx->get_buffer = get_buffer;
   pCodecCtx->release_buffer = release_buffer;
   AVCodec* pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
   if (pCodec == NULL)
   {
      return;
   }
   if (avcodec_open(pCodecCtx, pCodec) < 0)
   {
      return;
   }

   AVStream* pStream = pFormat->streams[streamId];
   mEstFrameDuration = 1 / av_q2d(pStream->r_frame_rate);
   mEstFrameCount = static_cast<int>(pStream->nb_frames);
   if (mEstFrameCount <= 0)
   {
      if (pStream->duration > 0)
      {
         mEstFrameCount =  static_cast<int>(av_q2d(pStream->time_base) * pStream->duration * av_q2d(pStream->r_frame_rate) + 0.5);
      }
      else
      {
         mEstFrameCount = 10000;
      }
   }

   mpDecoder = new DecodeThread(pFormat, pCodecCtx, streamId);
   mpDecoder->start(QThread::LowPriority);
}

VideoStream::~VideoStream()
{
   mpDecoder->terminate();
   mpDecoder->wait();
   delete mpDecoder;
   if (mpAnimationController.get() != NULL && mpAnimationController->getNumAnimations() <= 1)
   {
      Service<AnimationServices>()->destroyAnimationController(mpAnimationController.get());
   }
}

bool VideoStream::setAnimation(Animation* pAnim, AnimationController* pController)
{
   mpAnimationController.reset(pController);
   mpAnim = pAnim;
   if (mpAnim != NULL)
   {
      std::vector<AnimationFrame> frames;
      frames.reserve(mEstFrameCount);
      for (int frameNum = 0; frameNum < mEstFrameCount; frameNum++)
      {
         AnimationFrame frame;
         frame.mFrameNumber = frameNum;
         frame.mTime = frameNum * mEstFrameDuration;
         frames.push_back(frame);
      }
      mpAnim->setFrames(frames);
   }
   
   return true;
}

bool VideoStream::setDisplay(RasterElement* pRaster)
{
   mpDisplay = pRaster;
   return true;
}

void VideoStream::drawNextFrame(double requestedPts)
{
   const RasterDataDescriptor* pDesc = static_cast<const RasterDataDescriptor*>(mpDisplay->getDataDescriptor());
   VERIFYNRV(pDesc);
   FactoryResource<DataRequest> pReq;
   VERIFYNRV(pReq.get());
   pReq->setWritable(true);
   pReq->setInterleaveFormat(BIP);
   pReq->setRows(pReq->getStartRow(), pReq->getStopRow(), pDesc->getRowCount());
   DataAccessor acc = mpDisplay->getDataAccessor(pReq.release());
   if (!acc.isValid())
   {
      return;
   }
   size_t dataSize = pDesc->getRowCount() * pDesc->getColumnCount() * pDesc->getBandCount();

   mpDecoder->getMetadata(mpDisplay->getMetadata());
   if (mpDecoder->getFrame(requestedPts, acc->getRow()))
   {
      mpDisplay->updateData();
   }
}

void VideoStream::currentFrameChanged(Subject &subject, const std::string &signal, const boost::any &v)
{
   if (mpDisplay == NULL)
   {
      return;
   }
   double curTime = boost::any_cast<double>(v);
   drawNextFrame(curTime);
}