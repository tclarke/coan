/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef NEWCONTROLLERDIALOG_H__
#define NEWCONTROLLERDIALOG_H__

#include <QtGui/QDialog>

class QButtonGroup;
class QLineEdit;

class NewControllerDialog : public QDialog
{
   Q_OBJECT

public:
   NewControllerDialog(QWidget *pParent = NULL);
   virtual ~NewControllerDialog();

   QString name() const;
   bool isTimeBased() const;

private:
   QLineEdit *mpName;
   QButtonGroup *mpType;
};

#endif