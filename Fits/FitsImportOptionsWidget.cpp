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

#include "FitsImportOptionsWidget.h"

#include <QtGui/QComboBox>
#include <QtGui/QGridLayout>
#include <QtGui/QLabel>

FitsImportOptionsWidget::FitsImportOptionsWidget(int numberOfRows, QWidget *pParent) : QWidget(pParent)
{
   mpRow = new QComboBox(this);
   mpColumn = new QComboBox(this);
   mpBand = new QComboBox(this);
   mpRow->addItem("None");
   mpColumn->addItem("None");
   mpBand->addItem("None");
   for(int i = 1; i <= numberOfRows; i++)
   {
      mpRow->addItem(QString::number(i));
      mpColumn->addItem(QString::number(i));
      mpBand->addItem(QString::number(i));
   }

   QGridLayout *pLayout = new QGridLayout(this);
   pLayout->addWidget(new QLabel("Map row to axis", this), 0, 0);
   pLayout->addWidget(new QLabel("Map column to axis", this), 1, 0);
   pLayout->addWidget(new QLabel("Map band to axis", this), 2, 0);
   pLayout->addWidget(mpRow, 0, 1);
   pLayout->addWidget(mpColumn, 1, 1);
   pLayout->addWidget(mpBand, 2, 1);
   connect(mpRow, SIGNAL(activated(int)), this, SLOT(validateRowAxisMappings(int)));
   connect(mpColumn, SIGNAL(activated(int)), this, SLOT(validateColumnAxisMappings(int)));
   connect(mpBand, SIGNAL(activated(int)), this, SLOT(validateBandAxisMappings(int)));
}

FitsImportOptionsWidget::~FitsImportOptionsWidget()
{
}

int FitsImportOptionsWidget::rowAxis() const
{
   return mpRow->currentIndex();
}

int FitsImportOptionsWidget::columnAxis() const
{
   return mpColumn->currentIndex();
}

int FitsImportOptionsWidget::bandAxis() const
{
   return mpBand->currentIndex();
}

void FitsImportOptionsWidget::setRowAxis(int v)
{
   if(v >= 0 && v < mpRow->count())
   {
      mpRow->setCurrentIndex(v);
      validateRowAxisMappings(v);
   }
}

void FitsImportOptionsWidget::setColumnAxis(int v)
{
   if(v >= 0 && v < mpColumn->count())
   {
      mpColumn->setCurrentIndex(v);
      validateColumnAxisMappings(v);
   }
}

void FitsImportOptionsWidget::setBandAxis(int v)
{
   if(v >= 0 && v < mpBand->count())
   {
      mpBand->setCurrentIndex(v);
      validateBandAxisMappings(v);
   }
}

void FitsImportOptionsWidget::validateRowAxisMappings(int v)
{
   if(columnAxis() > 0 && columnAxis() == v)
   {
      mpColumn->setCurrentIndex(0);
   }
   if(bandAxis() > 0 && bandAxis() == v)
   {
      mpBand->setCurrentIndex(0);
   }
}

void FitsImportOptionsWidget::validateColumnAxisMappings(int v)
{
   if(rowAxis() > 0 && rowAxis() == v)
   {
      mpRow->setCurrentIndex(0);
   }
   if(bandAxis() > 0 && bandAxis() == v)
   {
      mpBand->setCurrentIndex(0);
   }
}

void FitsImportOptionsWidget::validateBandAxisMappings(int v)
{
   if(rowAxis() > 0 && rowAxis() == v)
   {
      mpRow->setCurrentIndex(0);
   }
   if(columnAxis() > 0 && columnAxis() == v)
   {
      mpColumn->setCurrentIndex(0);
   }
}
