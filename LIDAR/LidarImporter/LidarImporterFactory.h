/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef LIDARIMPORTERFACTORY_H
#define LIDARIMPORTERFACTORY_H

#include <string>

class LidarImporterFactory;
void addFactory(LidarImporterFactory*);

class LidarImporterFactory
{
public:
   LidarImporterFactory(const std::string& name) : mName(name) { addFactory(this); }
   virtual PlugIn* operator()() const { return NULL; }
   std::string mName;

private:
   LidarImporterFactory() {}
};

// Each PlugIn .cpp file should use LIDARIMPORTERFACTORY(pluginname)
// to register a factory with the module manager.
#define LIDARIMPORTERFACTORY(name) \
   namespace \
   { \
      class name##LidarImporterFactory : public LidarImporterFactory \
      { \
      public: \
         name##LidarImporterFactory(const std::string& name) : LidarImporterFactory(name) {} \
         PlugIn* operator()() const \
         { \
            return new name; \
         } \
      }; \
      static name##LidarImporterFactory name##pluginFactory(#name); \
   }

#endif
