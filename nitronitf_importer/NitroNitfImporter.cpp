#if !defined(_WIN64)
#include "AppVersion.h"
#include "NitroNitfImporter.h"
#include "PlugInRegistration.h"

#include <string>
#include <vector>

using namespace std;

REGISTER_PLUGIN_BASIC(OpticksNitf, NitroNitfImporter);

NitroNitfImporter::NitroNitfImporter()
{
   setName("Nitro NITF Importer");
   setExtensions("NITF Files (*.ntf *.NTF *.nitf *.NITF *.r0 *.R0)");
   setDescriptorId("{C892E0EB-09FF-47a0-BE96-1C5D15034244}");
   setProductionStatus(false);
}

NitroNitfImporter::~NitroNitfImporter()
{
}

#include "RasterUtilities.h"
#include "RasterFileDescriptor.h"
#define THROWIF(_X_) if (_X_) throw std::logic_error("Nitro Nitf Importer error");
#define GIMME(_X_, _Y_) \
   int _Y_; \
   THROWIF(!nitf_Field_get(_X_->_Y_, &_Y_, NITF_CONV_INT, NITF_INT32_SZ, &e));

vector<ImportDescriptor*> NitroNitfImporter::getImportDescriptors(const string& filename)
{
   vector<ImportDescriptor*> descriptors;

   // SWIPED FROM NITRO CSAMPLES test_image_loading.c

   nitf_Error e;
   nitf_IOHandle io = NITF_INVALID_HANDLE_VALUE;
   nitf_Reader *reader = 0;
   nitf_Record *record = 0;

   try
   {
      THROWIF(NITF_INVALID_HANDLE(io = nitf_IOHandle_create(filename.c_str(), NITF_ACCESS_READONLY, NITF_OPEN_EXISTING, &e)));
      THROWIF(!(reader = nitf_Reader_construct(&e)));
      THROWIF(!(record = nitf_Reader_read(reader, io, &e)));

      GIMME(record->header, numImages);

      nitf_ListIterator iter = nitf_List_begin(record->images);
      for (int i = 0; i < numImages; ++i)
      {
         nitf_ImageSubheader *&ish = ((nitf_ImageSegment *) nitf_ListIterator_get(&iter))->subheader;
         GIMME(ish, numRows);
         GIMME(ish, numCols);
         GIMME(ish, numImageBands);
         GIMME(ish, numMultispectralImageBands);

         EncodingType dataType;
         GIMME(ish, numBitsPerPixel);
         switch(numBitsPerPixel)
         {
         case 8:
            dataType = INT1UBYTE;
            break;
         case 16:
            dataType = INT2UBYTES;
            break;
         case 32:
            dataType = INT4UBYTES;
            break;
         default:
            // doesn't handle floats...
            THROWIF(0);
         }

         stringstream imageNameStream;
         imageNameStream << i;

         RasterDataDescriptor* rdd = RasterUtilities::generateRasterDataDescriptor(filename + imageNameStream.str(), NULL, numRows, numCols,
            numImageBands + numMultispectralImageBands, BSQ, dataType, IN_MEMORY);
         THROWIF(!rdd);
         RasterFileDescriptor* rfd = dynamic_cast<RasterFileDescriptor*>(
            RasterUtilities::generateAndSetFileDescriptor(reinterpret_cast<DataDescriptor*>(rdd), filename, imageNameStream.str(), LITTLE_ENDIAN_ORDER));
         THROWIF(!rfd);

         ImportDescriptorResource id(reinterpret_cast<DataDescriptor*>(rdd), dataType.isValid());
         descriptors.push_back(id.release());
         nitf_ListIterator_increment(&iter);
      }
   }
   catch (...)
   {}

   if (record) nitf_Record_destruct(&record);
   if (reader) nitf_Reader_destruct(&reader);
   if (!NITF_INVALID_HANDLE(io)) nitf_IOHandle_close(io);

   return descriptors;
}

unsigned char NitroNitfImporter::getFileAffinity(const string& filename)
{
   return getImportDescriptors(filename).empty() ? CAN_NOT_LOAD : CAN_LOAD + 1; // For now, override NitfImporter.
}

#include "RasterElement.h"
#include "DataDescriptor.h"
#include "PlugInResource.h"
#include "PlugInArgList.h"
bool NitroNitfImporter::createRasterPager(RasterElement *pRaster) const
{
   VERIFY(pRaster != NULL);

   DataDescriptor* pDd = pRaster->getDataDescriptor();
   VERIFY(pDd != NULL);
   FileDescriptor* pFd = pDd->getFileDescriptor();
   VERIFY(pFd != NULL);

   const string& datasetLocation = pFd->getDatasetLocation();
   if (datasetLocation.empty() == true)
   {
      return false;
   }

   stringstream imageNameStream(datasetLocation);
   int imageSegment;
   imageNameStream >> imageSegment;

   FactoryResource<Filename> pFn;
   pFn->setFullPathAndName(pFd->getFilename());

   ExecutableResource pPlugIn("Nitro Nitf Pager");
   pPlugIn->getInArgList().setPlugInArgValue("Segment Number", &imageSegment);
   pPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedElementArg(), pRaster);
   pPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedFilenameArg(), pFn.get());

   if (pPlugIn->execute() == true)
   {
      RasterPager* pPager = dynamic_cast<RasterPager*>(pPlugIn->getPlugIn());
      if (pPager != NULL)
      {
         pRaster->setPager(pPager);
         pPlugIn->releasePlugIn();
         return true;
      }
   }

   return false;
}






REGISTER_PLUGIN_BASIC(OpticksNitf, NitroNitfPager);

NitroNitfPager::NitroNitfPager() :
   mDataset(NITF_INVALID_HANDLE_VALUE),
   mpReader(NULL),
   mpRecord(NULL),
   mpDeserializer(NULL),
   mSegment(1)
{
   setName("Nitro Nitf Pager");
   setCopyright(APP_COPYRIGHT);
   setCreator("Ball Aerospace & Technologies Corp.");
   setDescription("Provides access to on-disk NITF data");
   setDescriptorId("{C74FF146-68D1-4e42-8D96-A7232745073E}");
   setVersion(APP_VERSION_NUMBER);
   setProductionStatus(APP_IS_PRODUCTION_RELEASE);
   setShortDescription("Nitro Nitf Pager");
}

NitroNitfPager::~NitroNitfPager()
{
   if (mpDeserializer) nitf_ImageReader_destruct(&mpDeserializer);
   if (mpRecord) nitf_Record_destruct(&mpRecord);
   if (mpReader) nitf_Reader_destruct(&mpReader);
   if (!NITF_INVALID_HANDLE(mDataset)) nitf_IOHandle_close(mDataset);
}

bool NitroNitfPager::getInputSpecification(PlugInArgList*& pArgList)
{
   Service<PlugInManagerServices> pPims;
   VERIFY(pPims.get() != NULL);

   if (CachedPager::getInputSpecification(pArgList) == true)
   {
      VERIFY(pArgList != NULL);

      if (pArgList->addArg<int>("Segment Number", 1) == false)
      {
         pPims->destroyPlugInArgList(pArgList);
         pArgList = NULL;
         return false;
      }

      return true;
   }

   return false;
}

bool NitroNitfPager::parseInputArgs(PlugInArgList *pInputArgList)
{
   bool success = CachedPager::parseInputArgs(pInputArgList) == true;
   if (success)
   {
      int* pSegment = pInputArgList->getPlugInArgValue<int>("Segment Number");
      if (pSegment == NULL)
      {
         return false;
      }
      mSegment = *pSegment; // guaranteed to be good since has a default value set
   }

   return success;
}

bool NitroNitfPager::openFile(const std::string& filename)
{
   if (mpDeserializer) nitf_ImageReader_destruct(&mpDeserializer);
   if (mpRecord) nitf_Record_destruct(&mpRecord);
   if (mpReader) nitf_Reader_destruct(&mpReader);
   if (!NITF_INVALID_HANDLE(mDataset)) nitf_IOHandle_close(mDataset);

   return !NITF_INVALID_HANDLE(mDataset = nitf_IOHandle_create(filename.c_str(), NITF_ACCESS_READONLY, NITF_OPEN_EXISTING, &mError)) &&
      (mpReader = nitf_Reader_construct(&mError)) &&
      (mpRecord = nitf_Reader_read(mpReader, mDataset, &mError)) &&
      (mpDeserializer = nitf_Reader_newImageReader(mpReader, mSegment, &mError));
}

#include "DataRequest.h"
CachedPage::UnitPtr NitroNitfPager::fetchUnit(DataRequest* pOriginalRequest)
{
   CachedPage::UnitPtr pUnit;
   VERIFYRV(!NITF_INVALID_HANDLE(mDataset), pUnit);

   nitf_Record *record = 0;
   nitf_SubWindow *subimage = 0;
   nitf_Uint8 *buffer = NULL;
   nitf_Uint32 *bandList = NULL;
   nitf_DownSampler *pixelSkip = 0;

   try
   {
      const nitf_Uint32 nBands = pOriginalRequest->getConcurrentBands();
      VERIFYRV(nBands == 1, pUnit);

      THROWIF(!(subimage = nitf_SubWindow_construct(&mError)));
      //THROWIF(!(buffer = (nitf_Uint8 **) NITF_MALLOC(nBands * sizeof(nitf_Uint8*))));
      THROWIF(!(bandList = (nitf_Uint32 *) NITF_MALLOC(sizeof(nitf_Uint32 *) * nBands)));
      subimage->startCol = pOriginalRequest->getStartColumn().getActiveNumber();
      subimage->startRow = pOriginalRequest->getStartRow().getActiveNumber();
      subimage->numRows = min(pOriginalRequest->getConcurrentRows(), pOriginalRequest->getStopRow().getActiveNumber() - pOriginalRequest->getStartRow().getActiveNumber() + 1);;
      subimage->numCols = pOriginalRequest->getConcurrentColumns();
      THROWIF(!(pixelSkip = nitf_PixelSkip_construct(1, 1, &mError)));
      THROWIF(!nitf_SubWindow_setDownSampler(subimage, pixelSkip, &mError));

      size_t subimageSize = subimage->numRows * subimage->numCols * getBytesPerBand();
      buffer = new nitf_Uint8[subimageSize];
      for (nitf_Uint32 band = 0; band < nBands; band++)
      {
         bandList[band] = pOriginalRequest->getStartBand().getActiveNumber() + band;
         //buffer[band] = (nitf_Uint8 *) NITF_MALLOC(subimageSize);
      }
      subimage->bandList = bandList;
      subimage->numBands = nBands;

      int padded;
      THROWIF(!nitf_ImageReader_read(mpDeserializer, subimage, &buffer, &padded, &mError));

      pUnit = CachedPage::UnitPtr(new CachedPage::CacheUnit((char*)buffer, pOriginalRequest->getStartRow(), pOriginalRequest->getConcurrentRows(), nBands * sizeof(nitf_Uint8*)));
   }
   catch (...)
   {}

   //if (buffer) NITF_FREE(buffer); // leaks buffer[i] -- but wait, what about pUnit?? // commented out for testing only -- this is a leak!
   if (bandList) NITF_FREE(bandList);
   if (subimage) nitf_SubWindow_destruct(&subimage);
   if (pixelSkip) nitf_DownSampler_destruct(&pixelSkip);

   return pUnit;
}

#endif