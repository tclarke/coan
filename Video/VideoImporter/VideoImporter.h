/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef VIDEOIMPORTER_H__
#define VIDEOIMPORTER_H__

#include "AppAssert.h"
#include "ImporterShell.h"
#include "ProgressTracker.h"

#if 0
   int mPrevFrameNum;
   AVFrame mNextFrame;
   AVPacket mPacket;
   int mBytesRemaining;
   uint8_t *mpRawPacketData;

   AVFormatContext *mpFormatCtx;
   AVCodecContext *mpCodecCtx;
   unsigned int mStreamId;

   QMutex mLock;
};
#endif

class VideoImporter : public ImporterShell
{
public:
   VideoImporter();
   virtual ~VideoImporter();

   virtual std::vector<ImportDescriptor*> getImportDescriptors(const std::string &filename);
   virtual unsigned char getFileAffinity(const std::string &filename);

   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual bool validate(const DataDescriptor* pDescriptor, std::string& errorMessage) const;
   virtual bool execute(PlugInArgList*pInArgList, PlugInArgList *pOutArgList);

protected:
   SpatialDataView* createView() const;

private:
   mutable ProgressTracker mProgress;
   RasterElement* mpRasterElement;
   Any* mpStreamElement;
};

#endif