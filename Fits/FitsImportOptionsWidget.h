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

#ifndef FITSIMPORTOPTIONSWIDGET_H
#define FITSIMPORTOPTIONSWIDGET_H

#include <QtGui/QWidget>

class QComboBox;

class FitsImportOptionsWidget : public QWidget
{
   Q_OBJECT
   Q_PROPERTY(int rowAxis READ rowAxis WRITE setRowAxis)
   Q_PROPERTY(int columnAxis READ columnAxis WRITE setColumnAxis)
   Q_PROPERTY(int bandAxis READ bandAxis WRITE setBandAxis)

public:
   FitsImportOptionsWidget(int numberOfRows, QWidget *pParent = NULL);
   ~FitsImportOptionsWidget();

   int rowAxis() const;
   int columnAxis() const;
   int bandAxis() const;
   void setRowAxis(int v);
   void setColumnAxis(int v);
   void setBandAxis(int v);

private slots:
   void validateRowAxisMappings(int v);
   void validateColumnAxisMappings(int v);
   void validateBandAxisMappings(int v);

private:
   QComboBox *mpRow;
   QComboBox *mpColumn;
   QComboBox *mpBand;
};

#endif
