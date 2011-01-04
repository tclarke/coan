/*
 * The information in this file is
 * Copyright(c) 2010 Trevor R.H. Clarke
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef NEFIMPORTER_H__
#define NEFIMPORTER_H__

#include "RasterElementImporterShell.h"

class NefImporter : public RasterElementImporterShell
{
public:
   NefImporter();
   virtual ~NefImporter();

   virtual std::vector<ImportDescriptor*> getImportDescriptors(const std::string &filename);
   virtual unsigned char getFileAffinity(const std::string &filename);
};

#endif