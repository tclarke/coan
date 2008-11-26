/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "Animation.h"
#include "AnimationPropertiesDialog.h"
#include "VideoUtils.h"
#include <QtGui/QCheckBox>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QDateTimeEdit>
#include <QtGui/QHBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QVBoxLayout>

AnimationPropertiesDialog::AnimationPropertiesDialog(Animation *pAnim, QWidget *pParent) : QDialog(pParent), mpAnim(pAnim), mChangingTimes(false)
{
   bool timeBased = mpAnim->getFrameType() == FRAME_TIME;
   QLabel *pNameLabel = new QLabel("Name:", this);
   QLabel *pStartLabel = new QLabel("Start Time:", this);
   pStartLabel->setEnabled(timeBased);
   QLabel *pStopLabel = new QLabel("Stop Time:", this);
   pStopLabel->setEnabled(timeBased);
   mpName = new QLineEdit(QString::fromStdString(mpAnim->getName()), this);
   mpStart = new QDateTimeEdit(this);
   mpStart->setCalendarPopup(true);
   mpStart->setDisplayFormat("yyyy/MM/dd\nhh:mm:ss.zzzZ");
   mpStart->setEnabled(timeBased);
   mpStop = new QDateTimeEdit(this);
   mpStop->setCalendarPopup(true);
   mpStop->setDisplayFormat("yyyy/MM/dd\nhh:mm:ss.zzzZ");
   mpStop->setEnabled(timeBased);
   mpMaintainDuration = new QCheckBox("Maintain Duration", this);
   mpMaintainDuration->setChecked(true);
   mpMaintainDuration->setEnabled(timeBased);
   QDialogButtonBox *pButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);

   QVBoxLayout *pTopLayout = new QVBoxLayout(this);
   QHBoxLayout *pNameLayout = new QHBoxLayout();
   pTopLayout->addLayout(pNameLayout);
   pNameLayout->addWidget(pNameLabel);
   pNameLayout->addWidget(mpName, 5);
   QHBoxLayout *pTimeLayout = new QHBoxLayout();
   pTopLayout->addLayout(pTimeLayout);
   pTimeLayout->addWidget(pStartLabel);
   pTimeLayout->addWidget(mpStart, 5);
   pTimeLayout->addWidget(pStopLabel);
   pTimeLayout->addWidget(mpStop, 5);
   QHBoxLayout *pMaintainLayout = new QHBoxLayout();
   pTopLayout->addLayout(pMaintainLayout);
   pMaintainLayout->addWidget(mpMaintainDuration);
   pMaintainLayout->addStretch(5);
   pTopLayout->addWidget(pButtons);
   pTopLayout->addStretch();

   connect(mpStart, SIGNAL(dateTimeChanged(const QDateTime&)), this, SLOT(startTimeChanged(const QDateTime&)));
   connect(mpStop, SIGNAL(dateTimeChanged(const QDateTime&)), this, SLOT(stopTimeChanged(const QDateTime&)));
   connect(mpMaintainDuration, SIGNAL(toggled(bool)), this, SLOT(adjustDuration(bool)));
   connect(pButtons, SIGNAL(accepted()), this, SLOT(accept()));
   connect(pButtons, SIGNAL(rejected()), this, SLOT(reject()));

   if(timeBased)
   {
      mpStart->setDateTime(TimelineUtils::timetToQDateTime(mpAnim->getStartValue()));
      mpStop->setDateTime(TimelineUtils::timetToQDateTime(mpAnim->getStopValue()));
   }
}

AnimationPropertiesDialog::~AnimationPropertiesDialog()
{
}

void AnimationPropertiesDialog::accept()
{
   std::string newName = mpName->text().toStdString();
   if(newName != mpAnim->getName())
   {
      mpAnim->setName(newName);
   }
   double newStart = TimelineUtils::QDateTimeToTimet(mpStart->dateTime());
   double newStop = TimelineUtils::QDateTimeToTimet(mpStop->dateTime());
   bool startDiff = newStart != mpAnim->getStartValue();
   bool stopDiff = newStop != mpAnim->getStopValue();
   bool rescale = !mpMaintainDuration->isChecked();
   if(!rescale && startDiff)
   {
      TimelineUtils::moveAnimation(mpAnim, newStart);
   }
   else if(rescale)
   {
      if(startDiff && stopDiff)
      {
         TimelineUtils::moveAnimation(mpAnim, newStart);
         TimelineUtils::rescaleAnimation(mpAnim, newStop, true);
      }
      else if(startDiff)
      {
         TimelineUtils::rescaleAnimation(mpAnim, newStart, false);
      }
      else if(stopDiff)
      {
         TimelineUtils::rescaleAnimation(mpAnim, newStop, true);
      }
   }
   QDialog::accept();
}

void AnimationPropertiesDialog::startTimeChanged(const QDateTime &newTime)
{
   if(!mChangingTimes && mpMaintainDuration->isChecked())
   {
      double newStart = TimelineUtils::QDateTimeToTimet(newTime);
      double duration = mpAnim->getStopValue() - mpAnim->getStartValue();
      mChangingTimes = true;
      mpStop->setDateTime(TimelineUtils::timetToQDateTime(newStart + duration));
      mChangingTimes = false;
   }
}

void AnimationPropertiesDialog::stopTimeChanged(const QDateTime &newTime)
{
   if(!mChangingTimes && mpMaintainDuration->isChecked())
   {
      double newStop = TimelineUtils::QDateTimeToTimet(newTime);
      double duration = mpAnim->getStopValue() - mpAnim->getStartValue();
      mChangingTimes = true;
      mpStart->setDateTime(TimelineUtils::timetToQDateTime(newStop - duration));
      mChangingTimes = false;
   }
}

void AnimationPropertiesDialog::adjustDuration(bool adjust)
{
   if(adjust)
   {
      startTimeChanged(mpStart->dateTime());
   }
}