/*
 * The information in this file is
 * Copyright(c) 2010 Trevor R.H. Clarke
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "DynamicObject.h"
#include "Endian.h"
#include "FileResource.h"
#include "ImportDescriptor.h"
#include "NefImporter.h"
#include "NefVersion.h"
#include "ObjectResource.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "RasterDataDescriptor.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "StringUtilities.h"

REGISTER_PLUGIN_BASIC(NefImporterModule, NefImporter);

namespace
{
   template<typename T>
   T getVal(FILE* pFp, Endian& e)
   {
      T tmp;
      fread(&tmp, sizeof(T), 1, pFp);
      e.swapValue(tmp);
      return tmp;
   }
}

NefImporter::NefImporter()
{
   setDescriptorId("{cb8fc80b-4fcd-4c13-8183-61ec61e95cb2}");
   setName("NEF Importer");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(NEF_COPYRIGHT);
   setVersion(NEF_VERSION_NUMBER);
   setProductionStatus(NEF_IS_PRODUCTION_RELEASE);
   setExtensions("NEF Files (*.nef)");
}

NefImporter::~NefImporter()
{
}

std::vector<ImportDescriptor*> NefImporter::getImportDescriptors(const std::string &filename)
{
   std::vector<ImportDescriptor*> descriptors;

   try
   {
      Endian e;
      DynamicObject* pMeta = NULL;
      { // scope the geotiff importer
         ImporterResource geotiff("GeoTIFF Importer", filename);
         if (geotiff.get() == NULL)
         {
            return descriptors;
         }
         std::vector<ImportDescriptor*> tiffDescriptors = geotiff->getImportDescriptors();
         if (tiffDescriptors.size() != 1)
         {
            return descriptors;
         }
         e = Endian(tiffDescriptors.front()->getDataDescriptor()->getFileDescriptor()->getEndian());
         pMeta = tiffDescriptors.front()->getDataDescriptor()->getMetadata();
      }
      if (dv_cast<std::string>(pMeta->getAttributeByPath("TIFF/Make")) != "NIKON CORPORATION")
      {
         return descriptors;
      }

      // Reload the file and parse the RAW IFD
      FileResource pNef(filename.c_str(), "rb");
      if (pNef.get() == NULL)
      {
         return descriptors;
      }
      // JpegImageOffset, RawOffset
      std::vector<unsigned int> ifds = dv_cast<std::vector<unsigned int> >(pMeta->getAttributeByPath("TIFF/SubIFD"));
      if (ifds.size() != 2)
      {
         return descriptors;
      }
      fseek(pNef, ifds[1], SEEK_SET);
      
      unsigned int rows = 0;
      unsigned int cols = 0;
      unsigned int bpp = 0;
      // parse the entries
      size_t entryCount = getVal<uint16_t>(pNef, e);
      while (--entryCount >= 0)
      {
         uint16_t tag = getVal<uint16_t>(pNef, e);
         uint16_t type = getVal<uint16_t>(pNef, e);
         uint16_t count = getVal<uint32_t>(pNef, e);
         bool compressed = false;
         switch(tag)
         {
         case 254: // SubfileType == 0 (full resolution)
            if (type != 4 && count != 1 && getVal<uint32_t>(pNef, e)  != 0)
            {
               return descriptors;
            }
            break;
         case 256: // ImageWidth
            if (type != 4 && count != 1)
            {
               return descriptors;
            }
            cols = getVal<uint32_t>(pNef, e);
            break;
         case 257: // ImageHight
            if (type != 4 && count != 1)
            {
               return descriptors;
            }
            rows = getVal<uint32_t>(pNef, e);
            break;
         case 258: // BitsPerSample
            if (type != 1 && count != 1)
            {
               return descriptors;
            }
            bpp = getVal<unsigned char>(pNef, e);
            fseek(pNef, 3, SEEK_CUR);
            break;
         case 259: // Compression
            if (type != 3 && count != 1)
            {
               return descriptors;
            }
            {
               uint16_t comp = getVal<uint16_t>(pNef, e);
               fseek(pNef, 2, SEEK_CUR);
               if (comp == 1)
               {
                  compressed = false;
               }
               else if (comp == 34713)
               {
                  compressed = true;
               }
               else
               {
                  return descriptors;
               }
            }
            break;
         default:
            fseek(pNef, 4, SEEK_CUR);
            break;
         }
      }
   }
   catch (const std::bad_cast&)
   {
      // metadata not present, wrong kind of file
   }
   return descriptors;
}

unsigned char NefImporter::getFileAffinity(const std::string &filename)
{
   try
   {
      ImporterResource geotiff("GeoTIFF Importer", filename);
      if (geotiff.get() == NULL)
      {
         return CAN_NOT_LOAD;
      }
      std::vector<ImportDescriptor*> tiffDescriptors = geotiff->getImportDescriptors();
      if (tiffDescriptors.size() != 1)
      {
         return CAN_NOT_LOAD;
      }
      DynamicObject* pMeta = tiffDescriptors.front()->getDataDescriptor()->getMetadata();
      if (dv_cast<std::string>(pMeta->getAttributeByPath("TIFF/Make")) != "NIKON CORPORATION")
      {
         return CAN_NOT_LOAD;
      }
      return CAN_LOAD + 10;
   }
   catch (const std::bad_cast&)
   {
      // metadata not present, wrong kind of file
   }
   return CAN_NOT_LOAD;
}
