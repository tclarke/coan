/*
 * The information in this file is
 * Copyright(c) 2012 Trevor R.H. Clarke
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "CachedPager.h"
#include "DateTime.h"
#include "DynamicObject.h"
#include "Endian.h"
#include "FileResource.h"
#include "ImportDescriptor.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "RAWPhotoImporter.h"
#include "RAWPhotoVersion.h"
#include "SpecialMetadata.h"
#include "StringUtilities.h"

#include <libraw/libraw.h>

REGISTER_PLUGIN_BASIC(RAWPhotoImporterModule, RAWPhotoImporter);

RAWPhotoImporter::RAWPhotoImporter()
{
   setDescriptorId("{cb8fc80b-4fcd-4c13-8183-61ec61e95cb2}");
   setName("RAWPhoto Importer");
   setCreator("Trevor R.H. Clarke");
   setCopyright(RAWPHOTO_COPYRIGHT);
   setVersion(RAWPHOTO_VERSION_NUMBER);
   setProductionStatus(RAWPHOTO_IS_PRODUCTION_RELEASE);
   setExtensions("Raw Files (*)");
}

RAWPhotoImporter::~RAWPhotoImporter()
{
}

std::vector<ImportDescriptor*> RAWPhotoImporter::getImportDescriptors(const std::string &filename)
{
   std::vector<ImportDescriptor*> descriptors;

   libraw_data_t* pRaw = libraw_init(0);
   VERIFYRV(pRaw, descriptors);
   if (libraw_open_file(pRaw, filename.c_str()) != LIBRAW_SUCCESS || pRaw->idata.raw_count == 0)
   {
      return descriptors;
   }
   RasterDataDescriptor* pDesc = RasterUtilities::generateRasterDataDescriptor(
      filename, NULL, pRaw->sizes.raw_height, pRaw->sizes.raw_width, INT2UBYTES, IN_MEMORY);
   DynamicObject* pMeta = pDesc->getMetadata();
   pMeta->setAttributeByPath("RAWPhoto/Camera Make", std::string(pRaw->idata.make));
   pMeta->setAttributeByPath("RAWPhoto/Camera Model", std::string(pRaw->idata.model));
   pMeta->setAttributeByPath("RAWPhoto/Colors", pRaw->idata.colors);
   pMeta->setAttributeByPath("RAWPhoto/Color Description", std::string(pRaw->idata.cdesc));
   pMeta->setAttributeByPath("RAWPhoto/ISO", pRaw->other.iso_speed);
   pMeta->setAttributeByPath("RAWPhoto/Shutter speed", pRaw->other.shutter);
   pMeta->setAttributeByPath("RAWPhoto/Aperture", pRaw->other.aperture);
   pMeta->setAttributeByPath("RAWPhoto/Focal Length", pRaw->other.focal_len);
   pMeta->setAttributeByPath("RAWPhoto/Shot Order", pRaw->other.shot_order);
   pMeta->setAttributeByPath("RAWPhoto/GPS", std::vector<unsigned int>(pRaw->other.gpsdata, pRaw->other.gpsdata+32));
   pMeta->setAttributeByPath("RAWPhoto/Description", std::string(pRaw->other.desc));
   pMeta->setAttributeByPath("RAWPhoto/Artist", std::string(pRaw->other.artist));
   FactoryResource<DateTime> pDate;
   pDate->setStructured(pRaw->other.timestamp);
   pMeta->setAttributeByPath(COLLECTION_DATE_TIME_METADATA_PATH, *(pDate.get()));
   
   ImportDescriptorResource pId(pDesc);
   RasterUtilities::generateAndSetFileDescriptor(pDesc, filename, std::string(), Endian::getSystemEndian());
   descriptors.push_back(pId.release());
   libraw_close(pRaw);
   return descriptors;
}

unsigned char RAWPhotoImporter::getFileAffinity(const std::string &filename)
{
   libraw_data_t* pRaw = libraw_init(0);
   if (pRaw != NULL)
   {
      if (libraw_open_file(pRaw, filename.c_str()) == LIBRAW_SUCCESS &&
          pRaw->idata.raw_count > 0)
      {
         libraw_close(pRaw);
         return CAN_LOAD + 10;
      }
   }
   return CAN_NOT_LOAD;
}

bool RAWPhotoImporter::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (!parseInputArgList(pInArgList))
   {
      return false;
   }
   RasterElement* pRaster = getRasterElement();
   const RasterDataDescriptor* pDesc = static_cast<const RasterDataDescriptor*>(pRaster->getDataDescriptor());
   const std::string filename = pDesc->getFileDescriptor()->getFilename().getFullPathAndName();

   libraw_data_t* pRaw = libraw_init(0);
   VERIFY(pRaw);
   if (libraw_open_file(pRaw, filename.c_str()) != LIBRAW_SUCCESS || pRaw->idata.raw_count == 0)
   {
      return false;
   }
   if (libraw_unpack(pRaw) != LIBRAW_SUCCESS)
   {
      return false;
   }
   unsigned short* pData = reinterpret_cast<unsigned short*>(pRaster->getRawData());
   if (pData == NULL)
   {
      libraw_close(pRaw);
      return NULL;
   }
   memcpy(pData, pRaw->rawdata.raw_image, sizeof(unsigned short) * pRaw->sizes.raw_height * pRaw->sizes.raw_width);
   libraw_close(pRaw);
   if (createView() == NULL)
   {
      return false;
   }

   return true;
}
