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

#include "CachedPager.h"
#include "RasterElementImporterShell.h"
#pragma warning(push, 1)
#pragma warning(disable: 4244)
#include <avformat.h>
#include <avutil.h>
#pragma warning(pop)

class FfmpegRasterPager : public CachedPager
{
public:
   FfmpegRasterPager();
   virtual ~FfmpegRasterPager();

   virtual bool getInputSpecification(PlugInArgList *&pArgList);
   virtual bool parseInputArgs(PlugInArgList *pInputArgList);

private:
   virtual bool openFile(const std::string& filename);
   virtual CachedPage::UnitPtr fetchUnit(DataRequest *pOriginalRequest);

   bool getNextFrame();

   AVFrame mNextFrame;
   AVPacket mPacket;
   int mBytesRemaining;
   uint8_t *mpRawPacketData;

   AVFormatContext *mpFormatCtx;
   AVCodecContext *mpCodecCtx;
   unsigned int mStreamId;

   std::vector<double> mFrameTimes;
};

class VideoImporter : public RasterElementImporterShell
{
public:
   VideoImporter();
   virtual ~VideoImporter();

   virtual std::vector<ImportDescriptor*> getImportDescriptors(const std::string &filename);
   virtual unsigned char getFileAffinity(const std::string &filename);

   virtual bool execute(PlugInArgList*pInArgList, PlugInArgList *pOutArgList);

protected:
   virtual bool validateDefaultOnDiskReadOnly(const DataDescriptor *pDescriptor, std::string &errorMessage) const;
   virtual bool createRasterPager(RasterElement *pRaster) const;

private:
   mutable AVFormatContext *mpFormatCtx;
   mutable AVCodecContext *mpCodecCtx;
};

#endif