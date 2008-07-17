/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "Animation.h"
#include "AnimationController.h"
#include "BackgroundSuppressionDialog.h"
#include "SpatialDataView.h"
#include <QtGui/QComboBox>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QGridLayout>
#include <QtGui/QGroupBox>
#include <QtGui/QHBoxLayout>
#include <QtGui/QLabel>

BackgroundSuppressionDialog::BackgroundSuppressionDialog(QWidget *pParent) : QDialog(pParent)
{
   mpStreaming = new QGroupBox("Streaming Mode", this);
   mpStreaming->setCheckable(true);
   mpStreaming->setChecked(true);

   QLabel *pAnimationLabel = new QLabel("Choose a linked animation:", mpStreaming);
   mpAnimation = new QComboBox(mpStreaming);

   QGridLayout *pStreamingLayout = new QGridLayout(mpStreaming);
   pStreamingLayout->addWidget(pAnimationLabel, 0, 0);
   pStreamingLayout->addWidget(mpAnimation, 0, 1);

   QDialogButtonBox *pButtons = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, Qt::Horizontal, this);
   connect(pButtons, SIGNAL(acept()), this, SLOT(accept()));
   connect(pButtons, SIGNAL(reject()), this, SLOT(reject()));

   QHBoxLayout *pLayout = new QHBoxLayout(this);
   pLayout->addWidget(mpStreaming);
   pLayout->addWidget(pButtons);
   pLayout->addStretch();
}

BackgroundSuppressionDialog::~BackgroundSuppressionDialog()
{
}

bool BackgroundSuppressionDialog::isStreaming() const
{
   return mpStreaming->isChecked();
}

Animation *BackgroundSuppressionDialog::animation() const
{
   if(mpAnimation->currentIndex() < 0)
   {
      return NULL;
   }
   return reinterpret_cast<Animation*>(mpAnimation->itemData(mpAnimation->currentIndex()).toLongLong());
}

void BackgroundSuppressionDialog::setView(SpatialDataView *pView)
{
   mpAnimation->clear();
   AnimationController *pController = pView->getAnimationController();
   if(pController != NULL)
   {
      const std::vector<Animation*> &animations = pController->getAnimations();
      for(std::vector<Animation*>::const_iterator animation = animations.begin(); animation != animations.end(); ++animation)
      {
         mpAnimation->addItem(QString::fromStdString((*animation)->getName()), reinterpret_cast<qlonglong>((*animation)));
      }
   }
}