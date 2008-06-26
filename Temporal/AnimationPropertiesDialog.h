/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef ANIMATIONPROPERTIESDIALOG_H__
#define ANIMATIONPROPERTIESDIALOG_H__

#include <QtGui/QDialog>

class Animation;
class QCheckBox;
class QDateTime;
class QDateTimeEdit;
class QLineEdit;

class AnimationPropertiesDialog : public QDialog
{
   Q_OBJECT

public:
   AnimationPropertiesDialog(Animation *pAnim, QWidget *pParent = NULL);
   virtual ~AnimationPropertiesDialog();

public slots:
   virtual void accept();

private slots:
   void startTimeChanged(const QDateTime &newTime);
   void stopTimeChanged(const QDateTime &newTime);
   void adjustDuration(bool adjust=true);

private:
   bool mChangingTimes;
   Animation *mpAnim;
   QLineEdit *mpName;
   QDateTimeEdit *mpStart;
   QDateTimeEdit *mpStop;
   QCheckBox *mpMaintainDuration;
};

#endif