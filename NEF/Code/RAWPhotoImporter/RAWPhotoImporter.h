/*
 * The information in this file is
 * Copyright(c) 2012 Trevor R.H. Clarke
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef RAWPHOTOIMPORTER_H__
#define RAWPHOTOIMPORTER_H__

#include "RasterElementImporterShell.h"

class RAWPhotoImporter : public RasterElementImporterShell
{
public:
   RAWPhotoImporter();
   virtual ~RAWPhotoImporter();

   virtual std::vector<ImportDescriptor*> getImportDescriptors(const std::string &filename);
   virtual unsigned char getFileAffinity(const std::string &filename);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
};

#endif
