/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "StringUtilities.h"
#include "WaveletDialog.h"
#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QGridLayout>
#include <QtGui/QLabel>

WaveletDialog::WaveletDialog(QWidget* pParent) : QDialog(pParent)
{
   QLabel* pBasisLabel = new QLabel("Wavelet Basis:", this);
   mpBasis = new QComboBox(this);
   mpBasis->setEditable(false);
   std::vector<std::string> vals = StringUtilities::getAllEnumValuesAsDisplayString<WaveletBasis>();
   for (std::vector<std::string>::const_iterator val = vals.begin(); val != vals.end(); ++val)
   {
      mpBasis->addItem(QString::fromStdString(*val));
   }
   mpForward = new QCheckBox("Forward Transformation", this);

   QDialogButtonBox* pButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);

   QGridLayout* pTopLevel = new QGridLayout(this);
   pTopLevel->addWidget(pBasisLabel, 0, 0);
   pTopLevel->addWidget(mpBasis, 0, 1);
   pTopLevel->addWidget(mpForward, 1, 0, 1, 2);
   pTopLevel->addWidget(pButtons, 2, 0, 1, 2);

   connect(pButtons, SIGNAL(accepted()), this, SLOT(accept()));
   connect(pButtons, SIGNAL(rejected()), this, SLOT(reject()));
}

WaveletDialog::~WaveletDialog()
{
}

WaveletBasis WaveletDialog::getBasis() const
{
   return StringUtilities::fromDisplayString<WaveletBasis>(mpBasis->currentText().toStdString());
}

bool WaveletDialog::getForward() const
{
   return mpForward->isChecked();
}

void WaveletDialog::setBasis(WaveletBasis basis)
{
   QString val = QString::fromStdString(StringUtilities::toDisplayString(basis));
   mpBasis->setCurrentIndex(mpBasis->findText(val));
}

void WaveletDialog::setForward(bool forward)
{
   mpForward->setChecked(forward);
}