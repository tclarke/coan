#if !defined(_WIN64)
#ifndef NITRONITFIMPORTER_H
#define NITRONITFIMPORTER_H

#include "RasterElementImporterShell.h"

class NitroNitfImporter : public RasterElementImporterShell
{
public:
   NitroNitfImporter();
   virtual ~NitroNitfImporter();
   std::vector<ImportDescriptor*> NitroNitfImporter::getImportDescriptors(const std::string& filename);
   unsigned char NitroNitfImporter::getFileAffinity(const std::string& filename);
   bool createRasterPager(RasterElement *pRaster) const;

};

#include "CachedPager.h"
#include "nitf\reader.h"
class NitroNitfPager : public CachedPager
{
public:
   NitroNitfPager();
   virtual ~NitroNitfPager();
   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool parseInputArgs(PlugInArgList* pInputArgList);

private:
   virtual bool openFile(const std::string& filename);
   virtual CachedPage::UnitPtr fetchUnit(DataRequest* pOriginalRequest);

   nitf_Error mError;
   nitf_IOHandle mDataset;
   nitf_Reader* mpReader;
   nitf_Record* mpRecord;
   nitf_ImageReader* mpDeserializer;
   int mSegment;
};

#endif
#endif