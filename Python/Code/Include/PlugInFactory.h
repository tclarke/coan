/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef PLUGINFACTORY_H
#define PLUGINFACTORY_H

#include "OptionQWidgetWrapper.h"
#include "PropertiesQWidgetWrapper.h"
#include <string>

class PlugIn;
class PlugInFactory;
void addFactory(PlugInFactory*);

class PlugInFactory
{
public:
   PlugInFactory(const std::string& name) : mName(name) { addFactory(this); }
   virtual PlugIn* operator()() const { return NULL; }
   std::string mName;

private:
   PlugInFactory() {}
};

// Each PlugIn .cpp file should use PLUGINFACTORY(pluginname)
// to register a factory with the module manager.
#define PLUGINFACTORY(name) \
   namespace \
   { \
      class name##PlugInFactory : public PlugInFactory \
      { \
      public: \
         name##PlugInFactory(const std::string& name) : PlugInFactory(name) {} \
         PlugIn* operator()() const \
         { \
            return new name; \
         } \
      }; \
      static name##PlugInFactory name##pluginFactory(#name); \
   }

// Each PlugIn .cpp file should use PLUGINFACTORY_OPTIONWRAPPER(pluginname)
// to register a factory for an OptionQWidgetWrapper with the module manager.
#define PLUGINFACTORY_OPTIONWRAPPER(name) \
   namespace \
   { \
      class name##PlugInFactory : public PlugInFactory \
      { \
      public: \
         name##PlugInFactory(const std::string& name) : PlugInFactory(name) {} \
         PlugIn* operator()() const \
         { \
            return new OptionQWidgetWrapper<name>; \
         } \
      }; \
      static name##PlugInFactory name##pluginFactory(#name); \
   }

// Each PlugIn .cpp file should use PLUGINFACTORY_PROPERTIESWRAPPER(pluginname)
// to register a factory for an PropertiesQWidgetWrapper with the module manager.
#define PLUGINFACTORY_PROPERTIESWRAPPER(name) \
   namespace \
   { \
      class name##PlugInFactory : public PlugInFactory \
      { \
      public: \
         name##PlugInFactory(const std::string& name) : PlugInFactory(name) {} \
         PlugIn* operator()() const \
         { \
            return new PropertiesQWidgetWrapper<name>; \
         } \
      }; \
      static name##PlugInFactory name##pluginFactory(#name); \
   }
#endif
