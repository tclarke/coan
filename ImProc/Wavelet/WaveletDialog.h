/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef WAVELETDIALOG_H
#define WAVELETDIALOG_H

#include "SpectralWavelet.h"
#include <QtGui/QDialog>

class QCheckBox;
class QComboBox;

class WaveletDialog : public QDialog
{
   Q_OBJECT

public:
   WaveletDialog(QWidget* pParent=NULL);
   virtual ~WaveletDialog();

   WaveletBasis getBasis() const;
   bool getForward() const;
   void setBasis(WaveletBasis basis);
   void setForward(bool forward);

private:
   QComboBox* mpBasis;
   QCheckBox* mpForward;
};

#endif