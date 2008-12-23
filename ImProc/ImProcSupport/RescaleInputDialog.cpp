/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "RescaleInputDialog.h"
#include "StringUtilities.h"
#include <QtGui/QComboBox>
#include <QtGui/QDoubleSpinBox>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QGridLayout>
#include <QtGui/QLabel>

RescaleInputDialog::RescaleInputDialog(QWidget* pParent) : QDialog(pParent)
{
   QLabel* pRowLabel = new QLabel("Row factor:", this);
   QLabel* pColLabel = new QLabel("Column factor:", this);
   QLabel* pInterpLabel = new QLabel("Interpolation Type:", this);
   mpRowFactor = new QDoubleSpinBox(this);
   mpColFactor = new QDoubleSpinBox(this);
   mpInterp = new QComboBox(this);
   mpInterp->setEditable(false);
   std::vector<std::string> vals = StringUtilities::getAllEnumValuesAsDisplayString<InterpType>();
   for (std::vector<std::string>::const_iterator val = vals.begin(); val != vals.end(); ++val)
   {
      mpInterp->addItem(QString::fromStdString(*val));
   }
   QDialogButtonBox* pButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);

   QGridLayout* pTopLevel = new QGridLayout(this);
   pTopLevel->addWidget(pRowLabel, 0, 0);
   pTopLevel->addWidget(mpRowFactor, 0, 1);
   pTopLevel->addWidget(pColLabel, 1, 0);
   pTopLevel->addWidget(mpColFactor, 1, 1);
   pTopLevel->addWidget(pInterpLabel, 2, 0);
   pTopLevel->addWidget(mpInterp, 2, 1);
   pTopLevel->addWidget(pButtons, 3, 0, 1, 2);

   connect(pButtons, SIGNAL(accepted()), this, SLOT(accept()));
   connect(pButtons, SIGNAL(rejected()), this, SLOT(reject()));
}

RescaleInputDialog::~RescaleInputDialog()
{
}

double RescaleInputDialog::getRowFactor() const
{
   return mpRowFactor->value();
}

double RescaleInputDialog::getColFactor() const
{
   return mpColFactor->value();
}

InterpType RescaleInputDialog::getInterpType() const
{
   return StringUtilities::fromDisplayString<InterpType>(mpInterp->currentText().toStdString());
}

void RescaleInputDialog::setRowFactor(double v)
{
   mpRowFactor->setValue(v);
}

void RescaleInputDialog::setColFactor(double v)
{
   mpColFactor->setValue(v);
}

void RescaleInputDialog::setInterpType(InterpType type)
{
   QString val = QString::fromStdString(StringUtilities::toDisplayString(type));
   mpInterp->setCurrentIndex(mpInterp->findText(val));
}