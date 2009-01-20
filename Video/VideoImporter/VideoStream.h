/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef VIDEOSTREAM_H
#define VIDEOSTREAM_H

#include "Animation.h"
#include "AnimationController.h"
#include "AnyData.h"
#include "AttachmentPtr.h"
#pragma warning(push, 1)
#pragma warning(disable: 4244)
extern "C" {
#include <avcodec.h>
#include <avformat.h>
#include <avutil.h>
};
#pragma warning(pop)
#include <QtCore/QList>
#include <QtCore/QMutex>
#include <QtCore/QSemaphore>
#include <QtCore/QThread>

class KlvItem;
class RasterElement;

class DecodeThread : public QThread
{
public:
   DecodeThread(AVFormatContext* pFormat, AVCodecContext* pCodec, int streamId);
   virtual ~DecodeThread();

   virtual void run();
   void getMetadata(DynamicObject* pDisplayMetadata);
   bool getFrame(double requestedPts, void* pDest);

private:
   struct Entry
   {
      Entry() : mPts(-1), mpPic(NULL) {}
      double mPts;
      AVPicture* mpPic;
   };

   AVFormatContext* mpFormat;
   AVCodecContext* mpCodec;
   int mStreamId;
   int mMetaStream;

   QSemaphore mFillCount;
   QSemaphore mEmptyCount;
   QList<Entry> mBuffer;
   size_t mFront;
   size_t mBack;

   QMutex mMetadataLock;
   std::auto_ptr<KlvItem> mpKlv;
};

class VideoStream : public AnyData
{
public:
   VideoStream(Any* pContainer);
   virtual ~VideoStream();

   bool setAnimation(Animation* pAnim, AnimationController* pController);
   bool setDisplay(RasterElement* pRaster);
   void drawNextFrame(double requestedPts);

   void currentFrameChanged(Subject &subject, const std::string &signal, const boost::any &v);

private:
   Any* mpContainer;
   RasterElement* mpDisplay;
   AttachmentPtr<AnimationController> mpAnimationController;
   Animation* mpAnim;
   int mEstFrameCount;
   double mEstFrameDuration;
   DecodeThread* mpDecoder;
};

#endif