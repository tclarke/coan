/**
* The information in this file is
* Copyright(c) 2007 Ball Aerospace & Technologies Corporation
* and is subject to the terms and conditions of the
* Opticks Limited Open Source License Agreement Version 1.0
* The license text is available from https://www.ballforge.net/
* 
* This material (software and user documentation) is subject to export
* controls imposed by the United States Export Administration Act of 1979,
* as amended and the International Traffic In Arms Regulation (ITAR),
* 22 CFR 120-130
*/

#include "DataVariant.h"
#include "DynamicObject.h"
#include "Fits.h"
#include "FileResource.h"
#include "ObjectResource.h"

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <string>

using namespace std;

bool Fits::canParseFile(const string& filename)
{
   LargeFileResource fitsFile;
   if(fitsFile.open(filename, O_RDONLY | O_BINARY, S_IREAD))
   {
      // parse the first header unit
      FactoryResource<DynamicObject> pHeader(Fits::parseHeaderUnit(fitsFile));
      if(pHeader.get() != NULL)
      {
         try
         {
            if(dv_cast<bool>(pHeader->getAttribute("SIMPLE")))
            {
               return true;
            }
         }
         catch(bad_cast&) {}
      }
   }
   return false;
}

DynamicObject *Fits::parseHeaderUnit(LargeFileResource &data)
{
   if(!data.validHandle())
   {
      return NULL;
   }

   FactoryResource<DynamicObject> pHeader;
   QMap<QString, int> noValueIndex;
   bool first = true;
   int historyIndex = 1;
   while(data.eof() == 0)
   {
      // read the entire card image
      struct Fits::CardImage cardImageData;
      memset(&cardImageData, 0, sizeof(cardImageData));
      if(data.read(&cardImageData, sizeof(cardImageData)) != sizeof(cardImageData))
      {
         return NULL;
      }
      QString keyword = QString::fromAscii(cardImageData.mKeyword, sizeof(cardImageData.mKeyword)).trimmed();
      
      if(first && keyword != "SIMPLE")
      {
         return NULL;
      }
      first = false;
      if(keyword == "END")
      {
         break;
      }
      if(keyword.isEmpty() || cardImageData.mValueIndicator[0] != '=')
      {
         // if there is not '=', this data is usually a comment, history, etc.
         // so we create sub-dynamic object for each keyword
         QString value = QString::fromAscii(reinterpret_cast<char*>(&cardImageData) + sizeof(cardImageData.mKeyword),
            sizeof(cardImageData) - sizeof(cardImageData.mKeyword)).trimmed();
         if(keyword.isEmpty())
         {
            // add this as a "No Keyword"
            keyword = "No Keyword";
            value = QString::fromAscii(reinterpret_cast<char*>(&cardImageData), sizeof(cardImageData)).trimmed();
         }
         if(!value.isEmpty())
         {
            int cnt = noValueIndex.value(keyword, 1);
            pHeader->setAttributeByPath(QString("%1/%2").arg(keyword).arg(cnt).toStdString(), value.toStdString());
            noValueIndex[keyword] = cnt+1;
         }
         continue;
      }
      DataVariant valueVariant;
      if(cardImageData.mValueIndicator[0] == '=')
      {
         QString value = QString::fromAscii(cardImageData.mValueAndComment, sizeof(cardImageData.mValueAndComment)).trimmed();
         int indexOfComment = value.indexOf('/');
         int indexOfSingleQuote = -1;
         if(!value.isEmpty() && value[0] == '\'')
         {
            // if the value is a string (' delimited) then we can have a / within the ''
            // that does not indicate a comment so we need to check after the closing '
            do
            {
               indexOfSingleQuote = value.indexOf('\'', 2);
               if(indexOfSingleQuote == -1)
               {
                  return NULL;
               }
            }
            while(indexOfSingleQuote < (value.size() - 1) && value[indexOfSingleQuote + 1] == '\'');
            indexOfComment = value.indexOf('/', indexOfSingleQuote);
         }
         if(indexOfComment != -1)
         {
            // remove the comment and trim the result
            value.truncate(indexOfComment);
            value = value.trimmed();
         }
         if(!value.isEmpty())
         {
            // determine the type
            if(value[0] == '\'') // string
            {
               value = value.mid(1, indexOfSingleQuote - 1);
               valueVariant = DataVariant(value.toStdString());
            }
            else if(value == "T") // logical true
            {
               valueVariant = DataVariant(true);
            }
            else if(value == "F") // logical false
            {
               valueVariant = DataVariant(false);
            }
            else if(value[0] == '(') // complex
            {
               QStringList components = value.mid(2, value.size() - 2).split(",");
               if(components.size() == 2)
               {
                  if(components[0].contains('.') || components[1].contains('.')) // float complex
                  {
                     bool ok;
                     float real, imaginary;
                     real = components[0].toFloat(&ok);
                     if(ok) imaginary = components[1].toFloat(&ok);
                     if(ok)
                     {
                        valueVariant = DataVariant(FloatComplex(real, imaginary));
                     }
                  }
                  else // integer complex
                  {
                     bool ok;
                     short real, imaginary;
                     real = components[0].toShort(&ok);
                     if(ok) imaginary = components[1].toShort(&ok);
                     if(ok)
                     {
                        valueVariant = DataVariant(IntegerComplex(real, imaginary));
                     }
                  }
               }
            }
            else if(value.contains('.')) // float
            {
               bool ok;
               double v = value.toDouble(&ok);
               if(ok)
               {
                  valueVariant = DataVariant(v);
               }
            }
            else // integer
            {
               bool ok;
               int v = value.toInt(&ok);
               if(ok)
               {
                  valueVariant = DataVariant(v);
               }
            }
         }
      }
      pHeader->setAttribute(keyword.toStdString(), valueVariant);
   }
   return pHeader.release();
}
