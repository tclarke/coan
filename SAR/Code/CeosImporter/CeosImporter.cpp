/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "CeosImporter.h"
#include "DynamicObject.h"
#include "Endian.h"
#include "ImportDescriptor.h"
#include "ObjectResource.h"
#include "PlugInRegistration.h"
#include "RasterDataDescriptor.h"
#include "RasterFileDescriptor.h"
#include "RasterElement.h"
#include "RasterUtilities.h"
#include "SARVersion.h"
#include "StringUtilities.h"
#include "TypeConverter.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

REGISTER_PLUGIN_BASIC(CeosImporterModule, CeosImporter);

#define CHECK_VAL(load_, check_) if (load_ != check_) return false;
class HeaderParser
{
public:
   class IOError : public std::exception
   {
   public:
      IOError() : std::exception("Error reading file.") {}
      ~IOError() throw() {}
   };

   HeaderParser(const QString& file) : mEndian(BIG_ENDIAN_ORDER), mFile(file)
   {
      mFile.open(QFile::ReadOnly);
   }

   virtual ~HeaderParser()
   {
      mFile.close();
   }

   virtual bool parse() = 0;

   const DynamicObject* getMetadata() const
   {
      return mMeta.get();
   }

   DynamicObject* adoptMetadata()
   {
      if (mMeta.get() == NULL)
      {
         return NULL;
      }
      return mMeta.release();
   }

protected:
   FactoryResource<DateTime> date(bool withTime)
   {
      unsigned short year = I(4);
      unsigned short month = I(2);
      unsigned short day = I(2);
      FactoryResource<DateTime> pDate;
      if (withTime)
      {
         unsigned short hour = I(2);
         unsigned short minute = I(2);
         unsigned short second = I(2);
         A(2); // blank
         pDate->set(year, month, day, hour, minute, second);
      }
      else
      {
         pDate->set(year, month, day);
      }
      return pDate;
   }
   std::string A(int m, bool trim = true)
   {
      QByteArray buf = mFile.read(m);
      if (buf.size() != m)
      {
         throw IOError();
      }
      QString str(buf);
      if (trim)
      {
         str = str.trimmed();
      }
      return str.toStdString();
   }
   int I(int m)
   {
      QByteArray buf = mFile.read(m);
      if (buf.size() != m)
      {
         throw IOError();
      }
      QString str(buf);
      return str.trimmed().toInt();
   }
   float F(int m, int n)
   {
      QByteArray buf = mFile.read(m + n + 1);
      if (buf.size() != (m + n + 1))
      {
         throw IOError();
      }
      QString str(buf);
      return str.trimmed().toFloat();
   }
   float G(int m, int n, int p)
   {
      QByteArray buf = mFile.read(m + n + p + 2);
      if (buf.size() != (m + n + p + 2))
      {
         throw IOError();
      }
      QString str(buf);
      return str.trimmed().toFloat();
   }
   template<typename T>
   T B()
   {
      int m = sizeof(T);
      QByteArray buf = mFile.read(m);
      if (buf.size() != m)
      {
         throw IOError();
      }
      T* pTmp = reinterpret_cast<T*>(buf.data());
      if (!mEndian.swapBuffer(pTmp, 1))
      {
         return false;
      }
      return *pTmp;
   }

   Endian mEndian;
   QFile mFile;
   FactoryResource<DynamicObject> mMeta;
};

class VolumeDirectory : public HeaderParser
{
public:
   VolumeDirectory(const QString& file) : HeaderParser(file), mBandCount(0) {}
   virtual ~VolumeDirectory() {}

   bool parse()
   {
      try
      {
         // Volume Descriptor
         CHECK_VAL(B<unsigned int>(), 1); // must be the first record
         CHECK_VAL(B<unsigned char>(), 0300); // volume descriptor subtype
         CHECK_VAL(B<unsigned char>(), 0300); // volume descriptor record type
         CHECK_VAL(B<unsigned char>(), 022);  // volume descriptor 2nd subtype
         CHECK_VAL(B<unsigned char>(), 022);  // volume descriptor 3rd subtype
         CHECK_VAL(B<unsigned int>(), 360);   // volume descriptor is always 360 bytes
         CHECK_VAL(A(2, false), "A "); // ASCII validity check
         A(2); // blank
         mMeta->setAttributeByPath("CEOS/Volume Descriptor/Specification Number", A(12));
         mMeta->setAttributeByPath("CEOS/Volume Descriptor/Specification Revision", A(2));
         mMeta->setAttributeByPath("CEOS/Volume Descriptor/Record Format Revision", A(2));
         mMeta->setAttributeByPath("CEOS/Volume Descriptor/Software Version", A(12));
         A(16); // blank
         mMeta->setAttributeByPath("CEOS/Volume Descriptor/Logical Volume ID", A(16));
         mMeta->setAttributeByPath("CEOS/Volume Descriptor/Volume Set ID", A(16));
         A(6); // blank
         mMeta->setAttributeByPath("CEOS/Volume Descriptor/Volume Number", I(2));
         mMeta->setAttributeByPath("CEOS/Volume Descriptor/First File Number", I(4));
         mMeta->setAttributeByPath("CEOS/Volume Descriptor/Logical Volume Number", I(4));
         A(4); // blank
         { // scope
            FactoryResource<DateTime> pDate = date(true);
            mMeta->setAttributeByPath("CEOS/Volume Descriptor/Logical Volume Preparation Date", *pDate.get());
         }
         mMeta->setAttributeByPath("CEOS/Volume Descriptor/Logical Volume Preparation Country", A(12));
         mMeta->setAttributeByPath("CEOS/Volume Descriptor/Logical Volume Preparation Agent", A(8));
         mMeta->setAttributeByPath("CEOS/Volume Descriptor/Logical Volume Preparation Facility", A(12));
         int filePointerCount = I(4);
         mMeta->setAttributeByPath("CEOS/Volume Descriptor/Total Record Count", I(4));
         A(192); // blank

         // read the file pointers
         mBandCount = 0;
         for (int filePointer = 0; filePointer < filePointerCount; ++filePointer)
         {
            std::string metaPath = QString("CEOS/File Pointer/%1/").arg(filePointer).toStdString();
            CHECK_VAL(B<unsigned int>(), (filePointer + 2)); // ensure the record read is the record expected
            CHECK_VAL(B<unsigned char>(), 0333); // file pointer subtype
            CHECK_VAL(B<unsigned char>(), 0300); // file pointer record type
            CHECK_VAL(B<unsigned char>(), 022);  // file pointer 2nd subtype
            CHECK_VAL(B<unsigned char>(), 022);  // file pointer 3rd subtype
            CHECK_VAL(B<unsigned int>(), 360);   // file pointer is always 360 bytes
            CHECK_VAL(A(2, false), "A "); // ASCII validity check
            A(2); // blank
            CHECK_VAL(I(4), (filePointer + 1)); // another sanity check to ensure we're on the right record
            { // scopr file ID
               mMeta->setAttributeByPath(metaPath + "File ID/Satellite Code", A(2));
               A(1); // blank
               mMeta->setAttributeByPath(metaPath + "File ID/Sensor Code", A(3));
               mMeta->setAttributeByPath(metaPath + "File ID/Sensor Type", A(1));
               std::string dataType = A(1);
               if (dataType == "0")
               {
                  mMeta->setAttributeByPath(metaPath + "File ID/Data Type", std::string("Level 1A"));
               }
               else if (dataType == "1")
               {
                  mMeta->setAttributeByPath(metaPath + "File ID/Data Type", std::string("Level 1B1"));
               }
               else if (dataType == "2")
               {
                  mMeta->setAttributeByPath(metaPath + "File ID/Data Type", std::string("Level 1B2"));
               }
               else
               {
                  mMeta->setAttributeByPath(metaPath + "File ID/Data Type", dataType);
               }
               std::string fileType = A(4);
               mInterleave = StringUtilities::fromDisplayString<InterleaveFormatType>(A(3));
               mMeta->setAttributeByPath(metaPath + "File ID/Image Format", mInterleave);
               if (fileType == "LEAD")
               {
                  mMeta->setAttributeByPath(metaPath + "File ID/File Type", std::string("Leader"));
                  A(1); // blank band number
               }
               else if (fileType == "IMGY")
               {
                  mMeta->setAttributeByPath(metaPath + "File ID/File Type", std::string("Image"));
                  mMeta->setAttributeByPath(metaPath + "File ID/Band Number", I(1));
                  mBandCount++;
               }
               else if (fileType == "TRAI")
               {
                  mMeta->setAttributeByPath(metaPath + "File ID/File Type", std::string("Trailer"));
                  A(1); // blank band number
               }
               else if (fileType == "SPPL")
               {
                  mMeta->setAttributeByPath(metaPath + "File ID/File Type", std::string("Supplemental"));
                  A(1); // blank band number
               }
               else
               {
                  mMeta->setAttributeByPath(metaPath + "File ID/File Type", fileType);
                  A(1); // blank band number
               }
            }

            mMeta->setAttributeByPath(metaPath + "File Class", A(28));
            mMeta->setAttributeByPath(metaPath + "File Class Code", A(4));
            mMeta->setAttributeByPath(metaPath + "Data Type", A(28));
            mMeta->setAttributeByPath(metaPath + "Data Type Code", A(4));
            mMeta->setAttributeByPath(metaPath + "Record Count", I(8));
            mMeta->setAttributeByPath(metaPath + "First Record Length", I(8));
            mMeta->setAttributeByPath(metaPath + "Maximum Record Length", I(8));
            CHECK_VAL(A(12), "FIXED LENGTH");
            CHECK_VAL(A(4), "FIXD");
            mMeta->setAttributeByPath(metaPath + "First Record Volume", I(2));
            mMeta->setAttributeByPath(metaPath + "Last Record Volume", I(2));
            mMeta->setAttributeByPath(metaPath + "First Record Number", I(8));
            A(208); // blank
         }

         // read the text block
         CHECK_VAL(B<unsigned int>(), (filePointerCount + 2)); // ensure the record read is the record expected
         CHECK_VAL(B<unsigned char>(), 022); // text block subtype
         CHECK_VAL(B<unsigned char>(), 077); // text block record type
         CHECK_VAL(B<unsigned char>(), 022);  // text block 2nd subtype
         CHECK_VAL(B<unsigned char>(), 022);  // text block 3rd subtype
         CHECK_VAL(B<unsigned int>(), 360);   // text block is always 360 bytes
         CHECK_VAL(A(2, false), "A "); // ASCII validity check
         A(2); // blank

         // Product ID
         mMeta->setAttributeByPath("CEOS/Text Record/Product ID", A(40));
         mMeta->setAttributeByPath("CEOS/Text Record/Preparation Info", A(60));
         mMeta->setAttributeByPath("CEOS/Text Record/Scene ID", A(40));
         mMeta->setAttributeByPath("CEOS/Text Record/Image Format", A(4));
         A(200); // blank
      }
      catch(const IOError&)
      {
         return false;
      }

      return true;
   }
   unsigned int getBandCount() const
   {
      return mBandCount;
   }
   InterleaveFormatType getInterleave() const
   {
      return mInterleave;
   }

private:
   unsigned int mBandCount;
   InterleaveFormatType mInterleave;
};

class FileDescriptorRecord : public HeaderParser
{
public:
   FileDescriptorRecord(const QString& file) : HeaderParser(file) {}
   virtual ~FileDescriptorRecord() {}

   bool parse()
   {
      try
      {
         CHECK_VAL(B<unsigned int>(), 1); // must be the first record
         CHECK_VAL(B<unsigned char>(), 077); // volume descriptor subtype
         CHECK_VAL(B<unsigned char>(), 0300); // volume descriptor record type
         CHECK_VAL(B<unsigned char>(), 022);  // volume descriptor 2nd subtype
         CHECK_VAL(B<unsigned char>(), 022);  // volume descriptor 3rd subtype
         B<unsigned int>(); // Record length
         CHECK_VAL(A(2, false), "A "); // ASCII validity check
         A(2); // blank
         A(12);
         A(2);
         A(2);
         A(12);
         I(4);
         A(16);
         A(4);
         I(8);
         I(4);
         A(4);
         I(8);
         I(4);
         A(4);
         I(8);
         I(4);
         A(1);
         A(1);
         A(1);
         A(1);
         A(64); // blank
      }
      catch(const IOError&)
      {
         return false;
      }

      return true;
   }
};

class ImageFileDescriptorRecord : public FileDescriptorRecord
{
public:
   ImageFileDescriptorRecord(const QString& file) : FileDescriptorRecord(file),
      mRowCount(0), mColCount(0), mBpp(0) {}
   virtual ~ImageFileDescriptorRecord() {}

   bool parse()
   {
      if (!FileDescriptorRecord::parse())
      {
         return false;
      }
      try
      {
         if (mFile.pos() != 180) // ensure we are continuing properly
         {
            return false;
         }
         mRowCount = I(6);
         I(6);
         A(24); //blank
         mBpp = I(4);
         int ppd = I(4);
         int bpd = I(4);
         if ((mBpp * ppd) / 8 != bpd) // sanity check
         {
            return false;
         }
         A(4);
         CHECK_VAL(I(4), 1); // we can't handle multiple bands per file with multifile BSQ
         CHECK_VAL(I(8), mRowCount);
         CHECK_VAL(I(4), 0);
         mColCount = I(8);
         CHECK_VAL(I(4), 0);
         CHECK_VAL(I(4), 0);
         CHECK_VAL(I(4), 0);
         CHECK_VAL(A(4), "BSQ");
         I(4);
         I(4);
         I(4);
         I(8);
         I(4);
         A(4);
         A(8);
         A(8);
         A(8);
         A(8);
         A(8);
         A(8);
         A(8);
         A(8);
         A(8);
         A(8);
         A(8);
         A(4); // blank
         CHECK_VAL(A(36), "INTEGER*1");
         CHECK_VAL(A(4), "I*1");
         I(4);
         I(4);
         I(4);
         A(4); // blank
         A(8); // blank
         A(8); // blank
         A(1); // blank
      }
      catch(const IOError&)
      {
         return false;
      }

      return true;
   }

   unsigned int getRowCount() const
   {
      return mRowCount;
   }

   unsigned int getColCount() const
   {
      return mColCount;
   }

   unsigned int getBpp() const
   {
      return mBpp;
   }

private:
   unsigned int mRowCount;
   unsigned int mColCount;
   unsigned int mBpp;
};

CeosImporter::CeosImporter()
{
   setDescriptorId("{7e4f2b50-98a8-4318-b222-578f4c76ee9f}");
   setName("CEOS Importer");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SAR_COPYRIGHT);
   setVersion(SAR_VERSION_NUMBER);
   setProductionStatus(SAR_IS_PRODUCTION_RELEASE);
   setExtensions("CEOS Volume Headers (VOL-* vol-* Vol-*)");
}

CeosImporter::~CeosImporter()
{
}

std::vector<ImportDescriptor*> CeosImporter::getImportDescriptors(const std::string &filename)
{
   std::vector<ImportDescriptor*> descriptors;
   QFileInfo volume(QString::fromStdString(filename));
   std::string datasetName = volume.fileName().remove(0, 4).toStdString();

   ImportDescriptorResource pImportDescriptor(datasetName, TypeConverter::toString<RasterElement>());
   if (pImportDescriptor.get() == NULL)
   {
      return descriptors;
   }
   VolumeDirectory vol(volume.absoluteFilePath());
   if (!vol.parse())
   {
      return descriptors;
   }
   std::vector<std::string> imageFiles;
   QDir dataDir = volume.dir();
   for (unsigned int im = 0; im < vol.getBandCount(); ++im)
   {
      imageFiles.push_back(
         dataDir.absoluteFilePath(QString("IMG-%1-%2")
               .arg(im + 1, 2, 10, QLatin1Char('0')).arg(QString::fromStdString(datasetName))).toStdString());
   }
   ImageFileDescriptorRecord img(QString::fromStdString(imageFiles.front()));
   if (!img.parse())
   {
      return descriptors;
   }
   pImportDescriptor->setDataDescriptor(
      RasterUtilities::generateRasterDataDescriptor(datasetName, NULL, img.getRowCount(), img.getColCount(),
      vol.getBandCount(), vol.getInterleave(), INT1UBYTE , IN_MEMORY));
   RasterFileDescriptor* pFileDesc = static_cast<RasterFileDescriptor*>(
      RasterUtilities::generateAndSetFileDescriptor(pImportDescriptor->getDataDescriptor(), filename, std::string(), BIG_ENDIAN_ORDER));
   pFileDesc->setBitsPerElement(img.getBpp());
   pFileDesc->setHeaderBytes(465);
   pFileDesc->setBandFiles(imageFiles);
   pImportDescriptor->getDataDescriptor()->setMetadata(vol.adoptMetadata());
   descriptors.push_back(pImportDescriptor.release());
   return descriptors;
}

unsigned char CeosImporter::getFileAffinity(const std::string &filename)
{
   VolumeDirectory vol(QString::fromStdString(filename));
   if (!vol.parse())
   {
      return CAN_NOT_LOAD;
   }
   return CAN_LOAD;
}