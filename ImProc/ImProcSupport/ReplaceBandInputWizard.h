/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef REPLACEBANDINPUTWIZARD_H
#define REPLACEBANDINPUTWIZARD_H

#include <QtCore/QStringList>
#include <QtGui/QWizard>

class QComboBox;
class RasterDataDescriptor;
class RasterElement;

class ReplaceBandInputWizard : public QWizard
{
   Q_OBJECT

public:
   ReplaceBandInputWizard(const RasterElement* pDest, QWidget* pParent=NULL);
   virtual ~ReplaceBandInputWizard();

   RasterElement* getSourceElement() const;
   unsigned int getDestBand() const;
   unsigned int getSourceBand() const;

protected:
   virtual void initializePage(int id);

private:
   void addSourcePage();
   void addSourceBandPage();
   void addDestBandPage();
   QStringList getBandNames(const RasterDataDescriptor* pDesc) const;

   const RasterElement* mpDest;
   int mSourceBandId;
   QComboBox* mpSource;
   QComboBox* mpSourceBand;
   QComboBox* mpDestBand;
};

#endif