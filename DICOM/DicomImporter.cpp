/**
* The information in this file is
* Copyright(c) 2007 Ball Aerospace & Technologies Corporation
* and is subject to the terms and conditions of the
* Opticks Limited Open Source License Agreement Version 1.0
* The license text is available from https://www.ballforge.net/
* 
* This material (software and user documentation) is subject to export
* controls imposed by the United States Export Administration Act of 1979,
* as amended and the International Traffic In Arms Regulation (ITAR),
* 22 CFR 120-130
*/

#include "AppVerify.h"
#include "DataRequest.h"
#include "DicomImporter.h"
#include "DynamicObject.h"
#include "FileResource.h"
#include "ImportDescriptor.h"
#include "PlugInArgList.h"
#include "PlugInResource.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "StringUtilities.h"
#include "TypeConverter.h"

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmdata/dcrledrg.h>
#include <dcmtk/dcmimgle/dcmimage.h>
#include <dcmtk/dcmimage/diregist.h>
#include <dcmtk/dcmjpeg/djdecode.h>
#include <QtCore/QString>

namespace
{
   DataVariant dcmElementToDataVariant(DcmElement &elmnt)
   {
      if(elmnt.getLength() < 256)
      {
         switch(elmnt.ident())
         {
         case EVR_AE: // application entry
         case EVR_AS: // age string
         case EVR_AT: // attribute tag
         case EVR_CS: // code string
         case EVR_DA: // date
         case EVR_DS: // decimal string
         case EVR_DT: // date/time
         case EVR_IS: // integer string
         case EVR_LO: // long string
         case EVR_LT: // long text
         case EVR_OB: // other byte
         case EVR_OF: // ??
         case EVR_OW: // other word
         case EVR_PN: // patient name
         case EVR_SH: // short string
         case EVR_ST: // short text
         case EVR_TM: // time
         case EVR_UI: // unique id
         case EVR_UT: // unlimited text
            {
               char *pVal = NULL;
               if(elmnt.getString(pVal).good() && pVal != NULL)
               {
                  return DataVariant(std::string(pVal));
               }
               break;
            }
         case EVR_FL: // float
            {
               Float32 val;
               if(elmnt.getFloat32(val).good())
               {
                  return DataVariant(val);
               }
               break;
            }
         case EVR_FD: // double
            {
               Float64 val;
               if(elmnt.getFloat64(val).good())
               {
                  return DataVariant(val);
               }
               break;
            }
         case EVR_SL: // signed long
            {
               Sint32 val;
               if(elmnt.getSint32(val).good())
               {
                  return DataVariant(val);
               }
               break;
            }
         case EVR_SS: // signed short
            {
               Sint16 val;
               if(elmnt.getSint16(val).good())
               {
                  return DataVariant(val);
               }
               break;
            }
         case EVR_UL: // unsigned long
            {
               Uint32 val;
               if(elmnt.getUint32(val).good())
               {
                  return DataVariant(val);
               }
               break;
            }
         case EVR_US: // unsigned short
            {
               Uint16 val;
               if(elmnt.getUint16(val).good())
               {
                  return DataVariant(val);
               }
               break;
            }
         case EVR_SQ: // sequence of items
         default:
            break; // do nothing
         }
      }
      return DataVariant(std::string("Type: ") +
         elmnt.getTag().getVR().getVRName() + " Len: " +
         StringUtilities::toDisplayString(elmnt.getLength()));
   }
}

DicomImporter::DicomImporter()
{
   setDescriptorId("{A680AF67-0304-4e12-9B5F-BEC86CA5EC66}");
   setName("DICOM Importer");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright("Copyright 2008, BATC");
   setVersion("0.1");
   setExtensions("DICOM Files (*.dcm)");
   DJDecoderRegistration::registerCodecs();
   DcmRLEDecoderRegistration::registerCodecs();
}

DicomImporter::~DicomImporter()
{
}

std::vector<ImportDescriptor*> DicomImporter::getImportDescriptors(const std::string &filename)
{
   mErrors.clear();
   mWarnings.clear();
   std::vector<ImportDescriptor*> descriptors;

   DcmFileFormat fileformat;
   OFCondition status;
   if((status = fileformat.loadFile(filename.c_str())).bad())
   {
      return descriptors;
   }

   ImportDescriptorResource pImportDescriptor(filename, TypeConverter::toString<RasterElement>());
   VERIFYRV(pImportDescriptor.get() != NULL, descriptors);
   descriptors.push_back(pImportDescriptor.release());

   DicomImage img(fileformat.getDataset(), fileformat.getDataset()->getOriginalXfer());
   if(img.getStatus() != EIS_Normal)
   {
      mErrors.push_back(std::string("Unable to decode image: ") + DicomImage::getString(img.getStatus()));
      pImportDescriptor->setDataDescriptor(RasterUtilities::generateRasterDataDescriptor(filename, NULL, 0, 0, INT1UBYTE, IN_MEMORY));
      return descriptors;
   }
   InterleaveFormatType interleave(BSQ);
   EncodingType encoding(INT4UBYTES);
   bool rgb = false;
   unsigned long rows = img.getHeight(), columns = img.getWidth(), frames = img.getFrameCount();
   int imgDepth = img.getDepth();
   switch(img.getPhotometricInterpretation())
   {
   case EPI_Monochrome1:
   case EPI_Monochrome2:
      // do nothing special....this is single or multi-frame grayscale, 1 or 2 byte
      break;
   case EPI_RGB:
   case EPI_PaletteColor:
      // supported if there's only 1 frame
      if(frames == 1)
      {
         frames = 3;
         rgb = true;
      }
      else
      {
         mWarnings.push_back("RGB and palette color not supported when multiple frames are present. Converting to grayscale.");
      }
      break;
   default:
      mWarnings.push_back(std::string(DicomImage::getString(img.getPhotometricInterpretation())) +
         " not supported. Attempting to convert to grayscale.");
   }

   if(imgDepth <= 8)
   {
      encoding = INT1UBYTE;
   }
   else if(imgDepth <= 16)
   {
      encoding = INT2UBYTES;
   }
   else if(imgDepth <= 32)
   {
      encoding = INT4UBYTES;
   }
   else
   {
      mWarnings.push_back("Bit depth " + StringUtilities::toDisplayString(imgDepth) + " not supported. Downgrading to 32 bits.");
      encoding = INT4UBYTES;
   }

   RasterDataDescriptor *pDescriptor = RasterUtilities::generateRasterDataDescriptor(
      filename, NULL, rows, columns, frames, interleave, encoding, IN_MEMORY);
   if(pDescriptor != NULL)
   {
      if(rgb)
      {
         pDescriptor->setDisplayBand(RED, pDescriptor->getBands()[0]);
         pDescriptor->setDisplayBand(GREEN, pDescriptor->getBands()[1]);
         pDescriptor->setDisplayBand(BLUE, pDescriptor->getBands()[2]);
         pDescriptor->setDisplayMode(RGB_MODE);
      }
      pImportDescriptor->setDataDescriptor(pDescriptor);
      FactoryResource<DynamicObject> pMeta;
      int idx = 0;
      DcmElement *pElmnt;
      while((pElmnt = fileformat.getDataset()->getElement(idx++)) != NULL)
      {
         if(pElmnt->error().bad())
         {
            continue;
         }
         const DcmTag &tag = pElmnt->getTag();
         std::string name = const_cast<DcmTag&>(tag).getTagName();
         if(name.empty())
         {
            name = QString("(%1,%2)").arg(pElmnt->getGTag(), 4, 16, QChar('0')).arg(pElmnt->getETag(), 4, 16, QChar('0')).toStdString();
         }
         pMeta->setAttributeByPath(std::string("DICOM/") + name, dcmElementToDataVariant(*pElmnt));
      }
      pImportDescriptor->getDataDescriptor()->setMetadata(pMeta.release());
   }
   RasterUtilities::generateAndSetFileDescriptor(pImportDescriptor->getDataDescriptor(), filename, std::string(), LITTLE_ENDIAN);

   return descriptors;
}

unsigned char DicomImporter::getFileAffinity(const std::string &filename)
{
   if(DcmFileFormat().loadFile(filename.c_str()).bad())
   {
      return CAN_NOT_LOAD;
   }
   return CAN_LOAD;
}

bool DicomImporter::validate(const DataDescriptor *pDescriptor, std::string &errorMessage) const
{
   errorMessage = "";
   if(!mErrors.empty())
   {
      errorMessage = StringUtilities::join(mErrors, "\n");
      return false;
   }
   std::string baseErrorMessage;
   bool valid = RasterElementImporterShell::validate(pDescriptor, baseErrorMessage);
   if(!mWarnings.empty())
   {
      if(!baseErrorMessage.empty())
      {
         errorMessage += baseErrorMessage + "\n";
      }
      errorMessage += StringUtilities::join(mWarnings, "\n");
   }
   else
   {
      errorMessage = baseErrorMessage;
   }
   return valid;
}

bool DicomImporter::validateDefaultOnDiskReadOnly(const DataDescriptor *pDescriptor, std::string &errorMessage) const
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

      // Data can be decoded
      DicomImage img(filename.getFullPathAndName().c_str());
      if(img.getStatus() != EIS_Normal)
      {
         errorMessage = std::string("Unable to decode dataset: ") + DicomImage::getString(img.getStatus());
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

bool DicomImporter::createRasterPager(RasterElement *pRaster) const
{
   VERIFY(pRaster != NULL);
   DataDescriptor *pDescriptor = pRaster->getDataDescriptor();
   VERIFY(pDescriptor != NULL);
   FileDescriptor *pFileDescriptor = pDescriptor->getFileDescriptor();
   VERIFY(pFileDescriptor != NULL);

   std::string filename = pRaster->getFilename();
   Progress *pProgress = getProgress();

   FactoryResource<Filename> pFilename;
   pFilename->setFullPathAndName(filename);

   ExecutableResource pagerPlugIn("DicomRasterPager", std::string(), pProgress);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedElementArg(), pRaster);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedFilenameArg(), pFilename.get());

   bool success = pagerPlugIn->execute();

   RasterPager *pPager = dynamic_cast<RasterPager*>(pagerPlugIn->getPlugIn());
   if(!success || pPager == NULL)
   {
      std::string message = "Execution of DicomRasterPager failed!";
      if (pProgress != NULL) pProgress->updateProgress(message, 0, ERRORS);
      return false;
   }

   pRaster->setPager(pPager);
   pagerPlugIn->releasePlugIn();

   return true;
}

DicomRasterPager::DicomRasterPager() : mpImage(NULL), mConvertRgb(false)
{
   setName("DicomRasterPager");
   setCopyright("Copyright 2008 BATC");
   setCreator("Ball Aerospace & Technologies Corp.");
   setDescription("Provides access to on-disk DICOM data");
   setDescriptorId("{27C35133-CBA7-41a6-AEBE-3B570FFC2E9F}");
   setVersion("0.1");
   setProductionStatus(false);
   setShortDescription("DICOM pager");
   DJDecoderRegistration::registerCodecs();
   DcmRLEDecoderRegistration::registerCodecs();
}

DicomRasterPager::~DicomRasterPager()
{
   DcmRLEDecoderRegistration::cleanup();
   DJDecoderRegistration::cleanup();
}

bool DicomRasterPager::openFile(const std::string &filename)
{
   mpImage.reset(new DicomImage(filename.c_str()));
   if(mpImage->getStatus() != EIS_Normal)
   {
      VERIFY_MSG(false, DicomImage::getString(mpImage->getStatus()));
   }
   if((mpImage->getPhotometricInterpretation() == EPI_RGB ||
       mpImage->getPhotometricInterpretation() == EPI_PaletteColor) &&
       static_cast<const RasterDataDescriptor*>(getRasterElement()->getDataDescriptor())->getBandCount() != mpImage->getFrameCount())
   {
      mConvertRgb = true;
   }
   return true;
}

CachedPage::UnitPtr DicomRasterPager::fetchUnit(DataRequest *pOriginalRequest)
{
   VERIFYRV(pOriginalRequest != NULL, CachedPage::UnitPtr());
   unsigned int frame = pOriginalRequest->getStartBand().getOriginalNumber();
   if(mConvertRgb)
   {
      frame /= 3;
   }
   const RasterDataDescriptor *pDesc = static_cast<const RasterDataDescriptor*>(getRasterElement()->getDataDescriptor());
   unsigned int bpp = pDesc->getBytesPerElement() * 8;
   size_t bufsize = mpImage->getOutputDataSize(bpp);
   char *pBuffer = new char[bufsize];
   if(!mpImage->getOutputData(pBuffer, bufsize, bpp, frame, 1))
   {
      delete pBuffer;
      return CachedPage::UnitPtr();
   }
   if(mConvertRgb)
   {
      unsigned int band = pOriginalRequest->getStartBand().getOriginalNumber();
      size_t bandsize = bufsize / 3;
      char *pNewBuffer = new char[bandsize];
      memcpy(pNewBuffer, pBuffer + (band * bandsize), bandsize);
      delete pBuffer;
      pBuffer = pNewBuffer;
      bufsize = bandsize;
   }
   return CachedPage::UnitPtr(new CachedPage::CacheUnit(pBuffer, pDesc->getRows()[0], pDesc->getRowCount(), bufsize));
}
