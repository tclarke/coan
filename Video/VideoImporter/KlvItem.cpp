/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "DataVariant.h"
#include "DateTime.h"
#include "DynamicObject.h"
#include "Endian.h"
#include "KlvItem.h"
#include "ObjectResource.h"
#include "TypeConverter.h"
#include <QtCore/QByteArray>
#include <QtCore/QMap>

namespace
{
   Endian endianSwapper;

   DataVariant convertToDataVariant(const std::string& type, const char* pData, size_t dataSize)
   {
      DataVariant rval;
      QByteArray tmpBuf(pData, dataSize);
      endianSwapper.swapBuffer(reinterpret_cast<void*>(tmpBuf.data()), dataSize, 1);
      if (type == "tstamp64_ms")
      {
         rval = DataVariant(TypeConverter::toString<uint64_t>(), tmpBuf.data(), false);
         if (rval.isValid())
         {
            double tstamp = dv_cast<uint64_t>(rval) / 1000000.0;
            FactoryResource<DateTime> dt;
            dt->setStructured(tstamp);
            rval = DataVariant(*dt.get());
         }
      }
      else
      {
         rval = DataVariant(type, tmpBuf.data(), false);
      }
      if (!rval.isValid())
      {
         rval = DataVariant(std::string(tmpBuf.data()));
      }
      return rval;
   }

   struct TransData
   {
      TransData(const std::string& n, const std::string& dt, size_t s = 0) : name(n), dataType(dt), dataSize(s) {}
      bool isGroup() const
      {
         return dataType == "universal" || dataType == "local";
      }
      std::string name;
      std::string dataType;
      size_t dataSize; // make this 0 to avoid endian swapping
      QMap<QByteArray, TransData> lds;
   };

   QMap<QByteArray, TransData> trans;

   void initTrans()
   {
      QMap<QByteArray, TransData>::iterator p;
      trans.insert(QByteArray("\x06\x0e\x2b\x34\x02\x01\x01\x01\x02\x08\x02\x00\x00\x00\x00\x00"),
         TransData("Security Metadata Universal Set", "universal"));
      trans.insert(QByteArray("\x06\x0e\x2b\x34\x01\x01\x01\x03\x02\x08\x02\x01\x00\x00\x00\x00"),
         TransData("Security Classification", TypeConverter::toString<std::string>()));
      trans.insert(QByteArray("\x06\x0e\x2b\x34\x01\x01\x01\x03\x07\x01\x20\x01\x02\x07\x00\x00"),
         TransData("Coding method", TypeConverter::toString<std::string>()));
      trans.insert(QByteArray("\x06\x0e\x2b\x34\x01\x01\x01\x03\x07\x01\x20\x01\x02\x08\x00\x00"),
         TransData("Classifying Country", TypeConverter::toString<std::string>()));
      trans.insert(QByteArray("\x06\x0e\x2b\x34\x01\x01\x01\x01\x0e\x01\x02\x05\x04\x00\x00\x00"),
         TransData("Version", TypeConverter::toString<unsigned short>(), sizeof(unsigned short)));

      p = trans.insert(QByteArray("\x06\x0e\x2b\x34\x02\x0b\x01\x01\x0e\x01\x03\x01\x01\x00\x00\x00"),
         TransData("UAS Datalink Local Data Set", "local"));
      p->lds.insert(QByteArray("\x02"), TransData("UNIX Time Stamp", "tstamp64_ms", sizeof(uint64_t)));

      trans.insert(QByteArray("\x06\x0e\x2b\x34\x02\x01\x01\x01\x0e\x01\x01\x02\x01\x01\x00\x00"),
         TransData("Predator UAV Universal Metadata Set", "universal"));
   }
}

KlvItem::KlvItem(QByteArray data, bool berTag) : mRawData(data), mLength(0)
{
   if (trans.isEmpty())
   {
      initTrans();
   }
   mData.setBuffer(&mRawData);
   mData.open(QBuffer::ReadOnly);
   if (berTag)
   {
      parseBerTag();
   }
   else
   {
      parseNonBerTag();
   }
   if (!mKey.isEmpty())
   {
      parseBerLength();
   }
}

KlvItem::~KlvItem()
{
   foreach (KlvItem* pChild, mChildren)
   {
      delete pChild;
   }
}

void KlvItem::push_back(QByteArray data)
{
   mRawData.append(data);
}

bool KlvItem::parseValue()
{
   if (mKey.isEmpty())
   {
      return false;
   }
   if (static_cast<uint64_t>(mData.bytesAvailable()) < mLength)
   {
      return false;
   }
   mValue = mData.read(mLength);
   if (mValue.size() != mLength)
   {
      return false;
   }

   // attempt to interpret
   QMap<QByteArray, TransData>::iterator transIt = trans.find(mKey);
   if (transIt != trans.end() && transIt->isGroup())
   {
      // load children
      QByteArray tmpBuf(mValue);
      while (!tmpBuf.isEmpty())
      {
         std::auto_ptr<KlvItem> pChild(new KlvItem(tmpBuf, transIt->dataType == "local"));
         if (!pChild->parseValue())
         {
            return false;
         }
         tmpBuf = pChild->getRemainingData();
         mChildren.push_back(pChild.release());
      }
   }
   return true;
}

QByteArray KlvItem::getKey()
{
   return mKey;
}

uint64_t KlvItem::getLength()
{
   return mLength;
}

const QList<KlvItem*>& KlvItem::getChildren()
{
   return mChildren;
}

QByteArray KlvItem::getRemainingData()
{
   return mData.readAll();
}

void KlvItem::updateDynamicObject(DynamicObject* pObj, void* pOpaque) const
{
   if (pObj == NULL || mKey.isEmpty())
   {
      return;
   }
   QMap<QByteArray, TransData>* pParent = reinterpret_cast<QMap<QByteArray, TransData>*>(pOpaque);
   if (pParent == NULL)
   {
      pParent = &trans;
   }

   QMap<QByteArray, TransData>::iterator transIt = pParent->find(mKey);
   if (mChildren.isEmpty())
   {
      // leaf node
      if (transIt != pParent->end())
      {
         DataVariant val = convertToDataVariant(transIt->dataType, mValue.data(), transIt->dataSize);
         pObj->setAttribute(transIt->name, val);
      }
      else
      {
         pObj->setAttribute(keyToString(), DataVariant(std::string(mValue.data())));
      }
   }
   else
   {
      void* pPassOpaque = reinterpret_cast<void*>(pParent);
      if (transIt->dataType == "local")
      {
         pPassOpaque = reinterpret_cast<void*>(&(transIt->lds));
      }
      FactoryResource<DynamicObject> pParDo;
      foreach (KlvItem* pChild, mChildren)
      {
         pChild->updateDynamicObject(pParDo.get(), pPassOpaque);
      }
      std::string name;
      if (transIt != pParent->end())
      {
         name = transIt->name;
      }
      else
      {
         name = keyToString();
      }
      pObj->setAttribute(name, DataVariant(*(pParDo.get())));
   }
}

std::string KlvItem::keyToString() const
{
   QString keyStr;
   for (int idx = 0; idx < mKey.size(); idx++)
   {
      if (idx == 0)
      {
         keyStr += "%1";
      }
      else
      {
         keyStr += ".%1";
      }
      keyStr = keyStr.arg(mKey[idx], 2, mKey.size() == 16 ? 16 : 10, QChar('0'));
   }
   return keyStr.toStdString();
}

void KlvItem::parseBerTag()
{
   char cnt;
   if (!mData.getChar(&cnt))
   {
      return;
   }
   if (cnt & 0x80)
   {
      mKey = mData.read(cnt);
      if (mKey.size() != cnt)
      {
         mKey.clear();
      }
   }
   else
   {
      mKey.resize(1);
      mKey[0] = cnt;
   }
}

void KlvItem::parseNonBerTag()
{
   // 16-byte OID is all that is supported for non-BER encoded tags
   mKey = mData.read(16);
   if (mKey.size() != 16)
   {
      mKey.clear();
   }
}

void KlvItem::parseBerLength()
{
   char cnt;
   if (!mData.getChar(&cnt))
   {
      mKey.clear();
      return;
   }
   if (cnt & 0x80)
   {
      cnt &= 0x7f;
      QByteArray tmpBuf = mData.read(cnt);
      if (tmpBuf.size() != cnt)
      {
         mKey.clear();
         return;
      }
      switch (cnt)
      {
      case 1:
         mLength = tmpBuf[0];
         break;
      case 2:
         mLength = endianSwapper.swapValue(*reinterpret_cast<const uint16_t*>(tmpBuf.data()));
         break;
      case 4:
         mLength = endianSwapper.swapValue(*reinterpret_cast<const uint32_t*>(tmpBuf.data()));
         break;
      case 8:
         mLength = endianSwapper.swapValue(*reinterpret_cast<const uint64_t*>(tmpBuf.data()));
         break;
      default:
         // invalid size...can't represent it
         mKey.clear();
         break;
      }
   }
   else
   {
      mLength = cnt;
   }
}