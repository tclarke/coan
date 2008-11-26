/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef BACKGROUNDSUPPRESSIONDIALOG_H__
#define BACKGROUNDSUPPRESSIONDIALOG_H__

#include <QtGui/QDialog>

class Animation;
class QComboBox;
class QGroupBox;
class SpatialDataView;

class BackgroundSuppressionDialog : public QDialog
{
   Q_OBJECT

public:
   BackgroundSuppressionDialog(QWidget *pParent = NULL);
   virtual ~BackgroundSuppressionDialog();

   bool isStreaming() const;
   Animation *animation() const;

   void setView(SpatialDataView *pView);

private:
   QGroupBox *mpStreaming;
   QComboBox *mpAnimation;
};

#endif