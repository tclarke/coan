/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "NewControllerDialog.h"
#include <QtGui/QButtonGroup>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QHBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QRadioButton>
#include <QtGui/QVBoxLayout>

NewControllerDialog::NewControllerDialog(QWidget *pParent) : QDialog(pParent)
{
   QLabel *pNameLabel = new QLabel("Controller Name: ", this);
   mpName = new QLineEdit(this);
   mpType = new QButtonGroup(this);
   QRadioButton *pFrame = new QRadioButton("Frame Based", this);
   mpType->addButton(pFrame, 0);
   QRadioButton *pTime = new QRadioButton("Time Based", this);
   mpType->addButton(pTime, 1);
   pTime->setChecked(true);
   QDialogButtonBox *pButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
   connect(pButtons, SIGNAL(accepted()), this, SLOT(accept()));
   connect(pButtons, SIGNAL(rejected()), this, SLOT(reject()));

   QVBoxLayout *pTop = new QVBoxLayout(this);
   QHBoxLayout *pName = new QHBoxLayout;
   pTop->addLayout(pName);
   pName->addWidget(pNameLabel);
   pName->addWidget(mpName, 5);
   QHBoxLayout *pType = new QHBoxLayout;
   pTop->addLayout(pType);
   pType->addWidget(pFrame);
   pType->addWidget(pTime);
   pTop->addWidget(pButtons);
   pTop->addStretch();
   setFixedSize(sizeHint());
}

NewControllerDialog::~NewControllerDialog()
{
}

QString NewControllerDialog::name() const
{
   return mpName->text();
}

bool NewControllerDialog::isTimeBased() const
{
   return mpType->checkedId() == 1;
}