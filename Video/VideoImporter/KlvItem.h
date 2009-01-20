/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef KLVITEM_H
#define KLVITEM_H

#include <QtCore/QBuffer>
#include <QtCore/QByteArray>
#include <QtCore/QList>

class KlvItem
{
public:
   /**
    * Create a new structure for holding a KLV set and attempt to parse the header.
    *
    * @param data
    *        The data to parse. This must contain at least the entire header which will have a variable
    *        length since at least the length field is BER encoded. Call getKey() to check for a valid
    *        header.
    * @param berTag
    *        If true, the tag is assumed to be BER encoded. If false, the tag is assumed to be a fixed 16 bytes.
    */
   KlvItem(QByteArray data, bool berTag=false);

   /**
    * Destructor
    */
   ~KlvItem();

   /**
    * Add data to the end if the internal data buffer.
    *
    * This can be called repeatedly to add data to the internal buffer. Call
    * parseValue() to see if more data is required for complete parsing.
    * Call getRemainingData() after parseValue() to get the data from the end of
    * the buffer which was not parsed as part of this KLV set.
    *
    * @param data
    *        Copy this data to the end of the internal buffer.
    */
   void push_back(QByteArray data);

   /**
    * Attempt to parse the value portion of the KLV set.
    *
    * @return True if the value was correctly parsed.
    *         False if either the header is invalid or there is
    *         not enough data to parse the entire value.
    */
   bool parseValue();

   /**
    * Get the key.
    *
    * @return The key/OID or an empty QByteArray if header parsing failed.
    */
   QByteArray getKey();

   /**
    * Get the value length.
    *
    * @return The length of the value field. Will return 0 if header parsing failed.
    *         Note that this may return 0 if header parsing succeeded and there is a
    *         zero length value.
    */
   uint64_t getLength();

   /**
    * Get the children of this KLV set.
    *
    * If the key indicates that this KLV set contains child KLV sets
    * this will return a list of children after successfully parsing the value.
    *
    * @return The children.
    */
   const QList<KlvItem*>& getChildren();

   /**
    * Get the leftover data from the internal buffer.
    *
    * @return Whatever data was in the buffer and was not part of the KLV set.
    */
   QByteArray getRemainingData();

   /**
    * Convert to a DynamicObject.
    *
    * @param pObj
    *        The DynamicObject to update.
    * @param pOpaque
    *        Used internally for local data sets. Call externally with NULL.
    */
   void updateDynamicObject(DynamicObject* pObj, void* pOpaque=NULL) const;

protected:
   std::string keyToString() const;
   void parseBerTag();
   void parseNonBerTag();
   void parseBerLength();

private:
   QByteArray mRawData;
   QBuffer mData;
   QByteArray mKey;
   uint64_t mLength;
   QByteArray mValue;
   QList<KlvItem*> mChildren;
};

#endif