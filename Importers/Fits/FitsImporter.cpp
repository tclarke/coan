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

#include "ComplexData.h"
#include "DataRequest.h"
#include "DynamicObject.h"
#include "Filename.h"
#include "FitsImporter.h"
#include "ImportDescriptor.h"
#include "ImportersVersion.h"
#include "MessageLogResource.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInFactory.h"
#include "PlugInResource.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "SessionManager.h"
#include "Signature.h"
#include "SignatureLibrary.h"
#include "SpecialMetadata.h"
#include "StringUtilities.h"
#include "switchOnEncoding.h"
#include "TypeConverter.h"

#include <ast.h>
#include <fitsio.h>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

PLUGINFACTORY(FitsImporter);
PLUGINFACTORY(FitsRasterPager);

#define METADATA_SIG_LENGTH "FITS_TEMP/length"
#define METADATA_SIG_ENCODING "FITS_TEMP/DataType"

namespace
{
   int encodingToDtype(EncodingType encoding)
   {
      switch(encoding)
      {
      case INT1SBYTE:
         return TSBYTE;
      case INT1UBYTE:
         return TBYTE;
      case INT2SBYTES:
         return TSHORT;
      case INT2UBYTES:
         return TUSHORT;
      case INT4SBYTES:
         return TINT;
      case INT4UBYTES:
         return TUINT;
      case FLT4BYTES:
         return TFLOAT;
      case FLT8BYTES:
         return TDOUBLE;
      default:
         return TBYTE;
      }
   }
   template<typename T>
   void addReflectancePoint(T *pBuffer, long idx, std::vector<double> &reflectance)
   {
      reflectance.push_back(static_cast<double>(pBuffer[idx]));
   }
}

FitsFileResource::FitsFileResource() : mpFile(NULL), mStatus(0)
{
}

FitsFileResource::FitsFileResource(const std::string &fname) : mStatus(0)
{
   if(fits_open_file(&mpFile, fname.c_str(), READONLY, &mStatus))
   {
      mpFile = NULL;
   }
}

FitsFileResource::~FitsFileResource()
{
   if(mpFile != NULL)
   {
      fits_close_file(mpFile, &mStatus);
      mpFile = NULL;
   }
}

bool FitsFileResource::isValid() const
{
   return mpFile != NULL;
}

std::string FitsFileResource::getStatus() const
{
   char pBuf[31];
   fits_get_errstatus(mStatus, pBuf);
   return std::string(pBuf);
}

FitsFileResource::operator fitsfile*()
{
   return mpFile;
}

void FitsFileResource::reset(const std::string &fname)
{
   if(mpFile != NULL)
   {
      fits_close_file(mpFile, &mStatus);
      mpFile = NULL;
   }
   if(fits_open_file(&mpFile, fname.c_str(), READONLY, &mStatus))
   {
      mpFile = NULL;
   }
}

FitsImporter::FitsImporter()// : mImportOptionsWidget(NULL)
{
   setDescriptorId("{299E1A6D-F80B-45D2-910D-0E318303CF88}");
   setName("FITS Importer");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(IMPORTERS_COPYRIGHT);
   setVersion(IMPORTERS_VERSION_NUMBER);
   setProductionStatus(IMPORTERS_IS_PRODUCTION_RELEASE);
   setExtensions("FITS Files (*.fit *.fts *.fiz *.fits)");
}

FitsImporter::~FitsImporter()
{
}

#define CHECK_FITS(call__, warn__, onerr__) if(call__) \
   { \
      char pBuf[31]; \
      fits_get_errstatus(status, pBuf); \
      if(warn__) \
      { \
         mWarnings.push_back(std::string(pBuf)); \
         status = 0; \
         onerr__; \
      } \
      else \
      { \
         mErrors.push_back(std::string(pBuf)); \
         onerr__; \
      } \
   }

std::vector<ImportDescriptor*> FitsImporter::getImportDescriptors(const std::string &fname)
{
   std::string filename = fname;
   mErrors.clear();
   mWarnings.clear();
   int status=0;
   std::vector<ImportDescriptor*> descriptors;

   FitsFileResource pFile(filename);
   if(!pFile.isValid())
   {
      mErrors.push_back(pFile.getStatus());
      return descriptors;
   }

   int hduCnt = 0;
   CHECK_FITS(fits_get_num_hdus(pFile, &hduCnt, &status), false, return descriptors);

   int specificHdu = 0, hdu = 1;
   std::vector<std::string> hduSplit = StringUtilities::split(filename, '[');
   if(hduSplit.size() > 1)
   {
      bool err = false;
      specificHdu = StringUtilities::fromDisplayString<int>(hduSplit.back().substr(0, hduSplit.back().size() - 1), &err);
      if(err)
      {
         specificHdu = 0;
      }
      else
      {
         hdu = hduCnt = specificHdu;
         filename = hduSplit.front();
      }
   }

   for(; hdu <= hduCnt; hdu++)
   {
      std::list<GcpPoint> gcps;
      std::string datasetName = filename + "[" + StringUtilities::toDisplayString(hdu) + "]";
      int hduType;
      CHECK_FITS(fits_movabs_hdu(pFile, hdu, &hduType, &status), false, continue);
      ImportDescriptorResource pImportDescriptor(static_cast<ImportDescriptor*>(NULL));
      FactoryResource<DynamicObject> pMetadata;
      VERIFYRV(pMetadata.get() != NULL, descriptors);
      { // scope
         std::vector<std::string> comments;
         char pCard[81], pValue[81], pComment[81];
         int nkeys = 0;
         CHECK_FITS(fits_get_hdrspace(pFile, &nkeys, NULL, &status), true, ;);
         for(int keyidx = 1; keyidx <= nkeys; keyidx++)
         {
            CHECK_FITS(fits_read_record(pFile, keyidx, pCard, &status), true, continue);
            std::string name = StringUtilities::toDisplayString(keyidx);
            std::vector<std::string> splitval = StringUtilities::split(std::string(pCard), '=');
            if(splitval.size() > 1)
            {
               name = StringUtilities::stripWhitespace(splitval.front());
            }
            CHECK_FITS(fits_parse_value(pCard, pValue, pComment, &status), true, pValue[0]=0);
            std::string val = StringUtilities::stripWhitespace(std::string(pValue));
            std::string comment = StringUtilities::stripWhitespace(std::string(pComment));
            if(!val.empty())
            {
               pMetadata->setAttributeByPath("FITS/" + name, val);
            }
            else if(!comment.empty())
            {
               comments.push_back(comment);
            }
         }
         if(!comments.empty())
         {
            // ideally, this would add a multi-line string but Opticks doesn't display this properly
            // pMetadata->setAttributeByPath("FITS/COMMENT", StringUtilities::join(comments, "\n"));
            for(unsigned int idx = 0; idx < comments.size(); idx++)
            {
               char num = '0';
               num += idx;
               pMetadata->setAttributeByPath("FITS/COMMENT/" + std::string(&num, 1), comments[idx]);
            }
         }
      }
      switch(hduType)
      {
      case IMAGE_HDU:
         {
            pImportDescriptor = ImportDescriptorResource(datasetName, TypeConverter::toString<RasterElement>());
            VERIFYRV(pImportDescriptor.get() != NULL, descriptors);

            EncodingType encoding;
            InterleaveFormatType interleave(BSQ);
            unsigned int rows=0, cols=0, bands=1;

            int bitpix, naxis;
            long axes[3];
            CHECK_FITS(fits_get_img_param(pFile, 3, &bitpix, &naxis, axes, &status), false, continue);
            switch(bitpix)
            {
            case BYTE_IMG:
               encoding = INT1UBYTE;
               break;
            case SHORT_IMG:
               encoding = INT2SBYTES;
               break;
            case LONG_IMG:
               encoding = INT4SBYTES;
               break;
            case FLOAT_IMG:
               encoding = FLT4BYTES;
               break;
            case DOUBLE_IMG:
               encoding = FLT8BYTES;
               break;
            default:
               mWarnings.push_back("Unsupported BITPIX value " + StringUtilities::toDisplayString(bitpix) + ".");
               continue;
            }
            encoding = checkForOverflow(encoding, pMetadata.get());
            if(naxis == 1)
            {
               // 1-D data is a signature
               pImportDescriptor = ImportDescriptorResource(datasetName, TypeConverter::toString<Signature>());
               pMetadata->setAttributeByPath(METADATA_SIG_ENCODING, encoding);
               pMetadata->setAttributeByPath(METADATA_SIG_LENGTH, axes[0]);
               break;
            }
            else if(naxis == 2)
            {
               cols = axes[0];
               rows = axes[1];
            }
            else if(naxis == 3)
            {
               cols = axes[0];
               rows = axes[1];
               bands = axes[2];
               interleave = BSQ;
            }
            else
            {
#pragma message(__FILE__ "(" STRING(__LINE__) ") : warning : Handle special case where hdu == 1 and naxis == 0. This means no primary IMAGE segment. The metadata should be copied to HDU2. (tclarke)")
               mErrors.push_back(StringUtilities::toDisplayString(naxis) + " axis data not supported.");
            }

            RasterDataDescriptor *pDataDesc = RasterUtilities::generateRasterDataDescriptor(
               datasetName, NULL, rows, cols, bands, interleave, encoding, IN_MEMORY);
            // parse any WCS headers
            {
               astBegin;
               char *pHeader = NULL;
               int numkeys = 0;
               fits_hdr2str(pFile, 0, NULL, 0, &pHeader, &numkeys, &status);
               AstFitsChan *pFitschan = astFitsChan(NULL, NULL, "");
               astPutCards(pFitschan, pHeader);
               free(pHeader);
               pHeader = NULL;
               AstFrameSet *pWcsinfo = reinterpret_cast<AstFrameSet*>(astRead(pFitschan));
               if(pWcsinfo != NULL)
               {
                  double pixcol[5], pixrow[5], lons[5], lats[5];
                  pixcol[0] = 0;      pixrow[0] = 0;
                  pixcol[1] = cols-1; pixrow[1] = 0;
                  pixcol[2] = cols-1; pixrow[2] = rows-1;
                  pixcol[3] = 0;      pixrow[3] = rows-2;
                  pixcol[4] = cols/2; pixrow[4] = rows/2;
                  astTran2(pWcsinfo, 5, pixcol, pixrow, 1, lons, lats);
                  for(int pnt = 0; pnt < 5; pnt++)
                  {
                     GcpPoint gcp;
                     gcp.mPixel = LocationType(pixcol[pnt], pixrow[pnt]);
                     gcp.mCoordinate = LocationType(lons[pnt], lats[pnt]);
                     gcps.push_back(gcp);
                  }
               }
               astEnd;
            }
            pImportDescriptor->setDataDescriptor(pDataDesc);
            if(specificHdu == 0 && naxis == 2 && (axes[0] <= 5 || axes[1] <= 5))
            {
               // There's a good chance this is really a spectrum.
               // We'll create an import descriptor for the spectrum version of this
               // And disable the raster descriptor by default
               pImportDescriptor->setImported(false);
               ImportDescriptorResource pSigDesc(datasetName, TypeConverter::toString<SignatureLibrary>());
               DynamicObject *pSigMetadata = pSigDesc->getDataDescriptor()->getMetadata();
               pSigMetadata->merge(pMetadata.get());
               std::vector<double> centerWavelengths;
               unsigned int cnt = (axes[0] <= 5) ? axes[1] : axes[0];
               double startVal = StringUtilities::fromDisplayString<double>(
                     dv_cast<std::string>(pMetadata->getAttributeByPath("FITS/MINWAVE"), "0.0"));
               double endVal = StringUtilities::fromDisplayString<double>(
                     dv_cast<std::string>(pMetadata->getAttributeByPath("FITS/MAXWAVE"), "0.0"));
               double incr = (endVal == 0.0) ? 1.0 : ((endVal - startVal) / static_cast<double>(cnt));
               centerWavelengths.reserve(cnt);
               for(unsigned int idx = 0; idx < cnt; idx++)
               {
                  centerWavelengths.push_back(startVal + (idx * incr));
               }
               pSigMetadata->setAttributeByPath(CENTER_WAVELENGTHS_METADATA_PATH, centerWavelengths);
               RasterUtilities::generateAndSetFileDescriptor(pSigDesc->getDataDescriptor(),
                  filename, StringUtilities::toDisplayString(hdu), BIG_ENDIAN);
               descriptors.push_back(pSigDesc.release());
            }

            break;
         }
      case ASCII_TBL:
      case BINARY_TBL:
         mWarnings.push_back("Tables not supported. [HDU " + StringUtilities::toDisplayString(hdu) + "]");
         continue;
      default:
         mWarnings.push_back("HDU " + StringUtilities::toDisplayString(hdu) + " is an unknown type.");
         continue;
      }
      RasterFileDescriptor *pFileDesc = 
         dynamic_cast<RasterFileDescriptor*>(RasterUtilities::generateAndSetFileDescriptor(pImportDescriptor->getDataDescriptor(),
         filename, StringUtilities::toDisplayString(hdu), BIG_ENDIAN));
      if(pFileDesc != NULL && !gcps.empty())
      {
         pFileDesc->setGcps(gcps);
      }

      pImportDescriptor->getDataDescriptor()->setMetadata(pMetadata.release());
      descriptors.push_back(pImportDescriptor.release());
   }
   return descriptors;
}

EncodingType FitsImporter::checkForOverflow(EncodingType encoding, DynamicObject *pMetadata)
{
   double bzero = 0.0, bscale = 1.0, datamax = 0.0;
   try
   {
      bzero = StringUtilities::fromDisplayString<double>(dv_cast<std::string>(pMetadata->getAttributeByPath("FITS/BZERO")));
   }
   catch(const std::bad_cast&) {} // empty
   try
   {
      bscale = StringUtilities::fromDisplayString<double>(dv_cast<std::string>(pMetadata->getAttributeByPath("FITS/BSCALE")));
   }
   catch(const std::bad_cast&) {} // empty
   try
   {
      datamax = StringUtilities::fromDisplayString<double>(dv_cast<std::string>(pMetadata->getAttributeByPath("FITS/DATAMAX")));
   }
   catch(const std::bad_cast&) {} // empty

   double intpart;
   if(modf(bscale, &intpart) != 0.0)
   {
      if(encoding == INT4SCOMPLEX)
      {
         mWarnings.push_back("Data scaling will be applied, converting to float complex.");
         return FLT8COMPLEX;
      }
      else if(encoding == FLT8COMPLEX)
      {
         return encoding;
      }
      else if(encoding != FLT8BYTES)
      {
         mWarnings.push_back("Data scaling will be applied, converting to 8-byte float.");
         return FLT8BYTES;
      }
   }
   if(bzero != 0.0 || bscale != 1.0)
   {
      if(datamax == 0.0)
      {
         mWarnings.push_back(
            "Data scaling will be applied, you may need to change the Data Type in the import Data tab or overflow may occur.");
         return encoding;
      }
      double newmax = fabs(datamax * bscale + bzero);
      switch(encoding)
      {
      case INT1SBYTE:
         if(newmax <= std::numeric_limits<char>::max())
         {
            return encoding;
         }
         encoding = INT2SBYTES; // fallthru
      case INT2SBYTES:
         if(newmax <= std::numeric_limits<short>::max())
         {
            return encoding;
         }
         encoding = INT4SBYTES; // fallthru
      case INT4SBYTES:
         if(newmax > std::numeric_limits<int>::max())
         {
            mWarnings.push_back(
               "Data scaling will be applied, you may need to change the Data Type in the import Data tab or overflow may occur.");
         }
         return encoding;
      case INT1UBYTE:
         if(newmax <= std::numeric_limits<unsigned char>::max())
         {
            return encoding;
         }
         encoding = INT2UBYTES; // fallthru
      case INT2UBYTES:
         if(newmax <= std::numeric_limits<unsigned short>::max())
         {
            return encoding;
         }
         encoding = INT4UBYTES; // fallthru
      case INT4UBYTES:
         if(newmax > std::numeric_limits<unsigned int>::max())
         {
            mWarnings.push_back(
               "Data scaling will be applied, you may need to change the Data Type in the import Data tab or overflow may occur.");
         }
         return encoding;
      case FLT4BYTES:
         if(newmax <= std::numeric_limits<float>::max())
         {
            return encoding;
         }
         encoding = FLT8BYTES;
      case FLT8BYTES:
         if(newmax == std::numeric_limits<double>::infinity())
         {
            mWarnings.push_back(
               "Data scaling will be applied, you may need to change the Data Type in the import Data tab or overflow may occur.");
         }
         return encoding;
      case INT4SCOMPLEX:
      case FLT8COMPLEX:
         mWarnings.push_back(
            "Data scaling will be applied, you may need to change the Data Type in the import Data tab or overflow may occur.");
         return encoding;
      }
   }
   return encoding;
}

unsigned char FitsImporter::getFileAffinity(const std::string &filename)
{
   FitsFileResource pFile(filename);
   return pFile.isValid() ? CAN_LOAD : CAN_NOT_LOAD;
}


bool FitsImporter::validate(const DataDescriptor *pDescriptor, std::string &errorMessage) const
{
   errorMessage = "";
   if(!mErrors.empty())
   {
      errorMessage = StringUtilities::join(mErrors, "\n");
      return false;
   }
   std::string baseErrorMessage;
   bool valid = false;
   if(pDescriptor->getType() == TypeConverter::toString<RasterElement>())
   {
      valid = RasterElementImporterShell::validate(pDescriptor, baseErrorMessage);
   }
   else if(pDescriptor->getType() == TypeConverter::toString<Signature>())
   {
      valid = true;
      const DynamicObject *pMetadata = pDescriptor->getMetadata();
      if(pMetadata == NULL)
      {
         baseErrorMessage = "No metadata present.";
         valid = false;
      }
      if(valid && (!pMetadata->getAttributeByPath(METADATA_SIG_LENGTH).isValid() ||
                   !pMetadata->getAttributeByPath(METADATA_SIG_ENCODING).isValid()))
      {
         baseErrorMessage = "Metadata is invalid.";
         valid = false;
      }
      if(valid && pDescriptor->getProcessingLocation() != IN_MEMORY)
      {
         baseErrorMessage = "Signatures can only be loaded in-memory.";
         valid = false;
      }
   }
   else if(pDescriptor->getType() == TypeConverter::toString<SignatureLibrary>())
   {
      valid = true;
      const DynamicObject *pMetadata = pDescriptor->getMetadata();
      if(pMetadata == NULL)
      {
         baseErrorMessage = "No metadata present.";
         valid = false;
      }
   }
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

bool FitsImporter::getInputSpecification(PlugInArgList *&pArgList)
{
   VERIFY((pArgList = mpPlugInManager->getPlugInArgList()) != NULL);
   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg(), NULL));
   VERIFY(pArgList->addArg<DataElement>(Importer::ImportElementArg()));
   VERIFY(pArgList->addArg(Importer::SessionLoadArg(), false));
   return true;
}

bool FitsImporter::execute(PlugInArgList *pInArgList, PlugInArgList *pOutArgList)
{
   if(pInArgList == NULL)
   {
      return false;
   }

   // Create a message log step
   StepResource pStep("Execute FITS Importer", "fits", "be22feec-bab1-40f8-a3a0-15d4131fbaba", "Execute failed");

   // Extract the input args
   Progress *pProgress = pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg());
   DataElement *pElement = pInArgList->getPlugInArgValue<DataElement>(Importer::ImportElementArg());
   RasterElement *pRaster = dynamic_cast<RasterElement*>(pElement);
   SignatureLibrary *pLibrary = dynamic_cast<SignatureLibrary*>(pElement);
   Signature *pSignature = dynamic_cast<Signature*>(pElement);
   if(pRaster != NULL)
   {
      PlugInArg *pArg = NULL;
      if(pInArgList->getArg(Importer::ImportElementArg(), pArg))
      {
         pArg->setType(TypeConverter::toString<RasterElement>());
      }
      pInArgList->setPlugInArgValue(Importer::ImportElementArg(), pRaster);

      if(!parseInputArgList(pInArgList))
      {
         return false;
      }

      // Update the log and progress with the start of the import
      if(getProgress() != NULL)
      {
         getProgress()->updateProgress("Importing raster element", 1, NORMAL);
      }

      if(!performImport())
      {
         return false;
      }

      // Create the view
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
      }
   }
   else if(pLibrary != NULL)
   {
      if(!pLibrary->import(pLibrary->getFilename() + "[" +
               pLibrary->getDataDescriptor()->getFileDescriptor()->getDatasetLocation() + "]", "FITS Importer"))
      {
         std::string message = "Unable to import signature library.";
         if(pProgress != NULL)
         {
            pProgress->updateProgress(message, 0, ERRORS);
         }
         pStep->finalize(Message::Failure, message);
         return false;
      }
      std::string unitsName = dv_cast<std::string>(pLibrary->getMetadata()->getAttributeByPath("FITS/BUNIT"), std::string());
      if(!unitsName.empty())
      {
         std::set<std::string> unitNames = pLibrary->getUnitNames();
         for(std::set<std::string>::const_iterator name = unitNames.begin(); name != unitNames.end(); ++name)
         {
            FactoryResource<Units> units;
            units->setUnitName(unitsName);
            units->setUnitType(RADIANCE);
            pLibrary->setUnits(*name, units.release());
         }
      }
   }
   else if(pSignature != NULL)
   {
      DynamicObject *pMetadata = pSignature->getDataDescriptor()->getMetadata();
      EncodingType encoding;
      long length;
      try
      {
         encoding = dv_cast<EncodingType>(pMetadata->getAttributeByPath(METADATA_SIG_ENCODING));
         length = dv_cast<long>(pMetadata->getAttributeByPath(METADATA_SIG_LENGTH));
      }
      catch(const std::bad_cast&)
      {
         std::string message = "Metadata is invalid.";
         if(pProgress != NULL)
         {
            pProgress->updateProgress(message, 0, ERRORS);
         }
         pStep->finalize(Message::Failure, message);
         return false;
      }
      pMetadata->removeAttribute(METADATA_SIG_ENCODING);
      pMetadata->removeAttribute(METADATA_SIG_LENGTH);
      FitsFileResource pFits(pSignature->getFilename());
      VERIFY(pFits.isValid());
      DataDescriptor *pDesc = pSignature->getDataDescriptor();
      int hdu = StringUtilities::fromDisplayString<int>(pDesc->getFileDescriptor()->getDatasetLocation());
      VERIFY(hdu >= 1);
      int status = 0;
      VERIFY(fits_movabs_hdu(pFits, hdu, NULL, &status) == 0);

      long startPix = 1;
      size_t bufsize = length;
      switch(encoding)
      {
      case INT2SBYTES:
      case INT2UBYTES:
         bufsize *= 2;
         break;
      case INT4SBYTES:
      case INT4UBYTES:
      case INT4SCOMPLEX:
      case FLT4BYTES:
         bufsize *= 4;
         break;
      case FLT8BYTES:
      case FLT8COMPLEX:
         bufsize *= 8;
         break;
      }
      std::auto_ptr<char> pBuffer(new char[bufsize]);
      if(fits_read_pix(pFits, encodingToDtype(encoding), &startPix, length, NULL, pBuffer.get(), NULL, &status))
      {
         if(status == NUM_OVERFLOW)
         {
            std::string message = "Data have overflowed, change the DataType metadata value in the import options dialog and re-import.";
            if(pProgress != NULL)
            {
               pProgress->updateProgress(message, 0, WARNING);
            }
            pStep->addMessage(message, "fits", "eaa10db0-b10a-43c3-a5f5-b61e2505a41c");
         }
         else
         {
            char pBuf[31];
            fits_get_errstatus(status, pBuf);
            std::string message(pBuf);
            if(pProgress != NULL)
            {
               pProgress->updateProgress(message, 0, ERRORS);
            }
            pStep->finalize(Message::Failure, message);
            return false;
         }
      }
      std::vector<double> independent;
      std::vector<double> dependent;
      independent.reserve(length);
      dependent.reserve(length);
      for(int idx = 0; idx < length; idx++)
      {
         independent.push_back(idx);
         switchOnEncoding(encoding, addReflectancePoint, pBuffer.get(), idx, dependent);
      }
      pSignature->setData("Wavelength", independent);
      pSignature->setData("Reflectance", dependent);
   }
   else
   {
      std::string message = "No data element specified";
      if(pElement != NULL)
      {
         message = "Can not import data elements of type " + pElement->getType();
      }
      if(pProgress != NULL)
      {
         pProgress->updateProgress(message, 0, ERRORS);
      }
      pStep->finalize(Message::Failure, message);
      return false;
   }

   if(pProgress != NULL)
   {
      pProgress->updateProgress("FITS import complete.", 100, NORMAL);
   }

   pStep->finalize(Message::Success);
   return true;
}

bool FitsImporter::createRasterPager(RasterElement *pRaster) const
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

   ExecutableResource pagerPlugIn("FitsRasterPager", std::string(), pProgress);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedElementArg(), pRaster);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedFilenameArg(), pFilename.get());

   bool success = pagerPlugIn->execute();

   RasterPager *pPager = dynamic_cast<RasterPager*>(pagerPlugIn->getPlugIn());
   if(!success || pPager == NULL)
   {
      std::string message = "Execution of FitsRasterPager failed!";
      if (pProgress != NULL) pProgress->updateProgress(message, 0, ERRORS);
      return false;
   }

   pRaster->setPager(pPager);
   pagerPlugIn->releasePlugIn();

   return true;
}

FitsRasterPager::FitsRasterPager()
{
   setName("FitsRasterPager");
   setCopyright(IMPORTERS_COPYRIGHT);
   setCreator("Ball Aerospace & Technologies Corp.");
   setDescription("Provides access to on-disk FITS data");
   setDescriptorId("{9CFA503D-4570-4612-9996-5FAE649791B8}");
   setVersion(IMPORTERS_VERSION_NUMBER);
   setProductionStatus(IMPORTERS_IS_PRODUCTION_RELEASE);
   setShortDescription("FITS pager");
}

FitsRasterPager::~FitsRasterPager()
{
}

bool FitsRasterPager::openFile(const std::string &filename)
{
   mpFile.reset(filename);
   if(!mpFile.isValid())
   {
      return false;
   }
   int hdu = StringUtilities::fromDisplayString<int>(
      getRasterElement()->getDataDescriptor()->getFileDescriptor()->getDatasetLocation());
   if(hdu < 1)
   {
      return false;
   }
   int status = 0;
   if(fits_movabs_hdu(mpFile, hdu, NULL, &status))
   {
      return false;
   }
   return true;
}

CachedPage::UnitPtr FitsRasterPager::fetchUnit(DataRequest *pOriginalRequest)
{
   const RasterDataDescriptor *pDesc = static_cast<const RasterDataDescriptor*>(getRasterElement()->getDataDescriptor());
   long maxInRow = std::min(pOriginalRequest->getConcurrentRows(), pDesc->getRowCount() - pOriginalRequest->getStartRow().getOnDiskNumber());
   long pixcnt = maxInRow * pDesc->getColumnCount();
   long pStartPix[3] = {1,
                        pOriginalRequest->getStartRow().getOnDiskNumber() + 1,
                        pOriginalRequest->getStartBand().getOnDiskNumber() + 1};
   int status = 0;

   size_t bufsize = pixcnt * pDesc->getBytesPerElement();
   std::auto_ptr<char> pBuffer(new char[bufsize]);
   if(fits_read_pix(mpFile, encodingToDtype(pDesc->getDataType()), pStartPix, pixcnt, NULL, pBuffer.get(), NULL, &status))
   {
      if(status != NUM_OVERFLOW)
      {
         char pBuf[31];
         fits_get_errstatus(status, pBuf);
         VERIFYRV_MSG(false, CachedPage::UnitPtr(), pBuf);
      }
   }
   return CachedPage::UnitPtr(new CachedPage::CacheUnit(
      pBuffer.release(), pOriginalRequest->getStartRow(), pOriginalRequest->getConcurrentRows(), bufsize));
}
