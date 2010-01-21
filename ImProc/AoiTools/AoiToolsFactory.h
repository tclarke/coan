/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef AOITOOLSFACTORY_H
#define AOITOOLSFACTORY_H

#include <string>

class AoiToolsFactory;
void addFactory(AoiToolsFactory*);

class AoiToolsFactory
{
public:
   AoiToolsFactory(const std::string& name) : mName(name) { addFactory(this); }
   virtual PlugIn* operator()() const { return NULL; }
   std::string mName;

private:
   AoiToolsFactory() {}
};

// Each PlugIn .cpp file should use AOITOOLSFACTORY(pluginname)
// to register a factory with the module manager.
#define AOITOOLSFACTORY(name) \
   namespace \
   { \
      class name##AoiToolsFactory : public AoiToolsFactory \
      { \
      public: \
         name##AoiToolsFactory(const std::string& name) : AoiToolsFactory(name) {} \
         PlugIn* operator()() const \
         { \
            return new name; \
         } \
      }; \
      static name##AoiToolsFactory name##pluginFactory(#name); \
   }

#endif
