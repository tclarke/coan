/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef RESCALEINPUTDIALOG_H
#define RESCALEINPUTDIALOG_H

#include "Rescale.h"
#include <QtGui/QDialog>

class QComboBox;
class QDoubleSpinBox;

class RescaleInputDialog : public QDialog
{
   Q_OBJECT

public:
   RescaleInputDialog(QWidget* pParent=NULL);
   virtual ~RescaleInputDialog();

   double getRowFactor() const;
   double getColFactor() const;
   InterpType getInterpType() const;
   void setRowFactor(double v);
   void setColFactor(double v);
   void setInterpType(InterpType type);

private:
   QDoubleSpinBox* mpRowFactor;
   QDoubleSpinBox* mpColFactor;
   QComboBox* mpInterp;
};

#endif