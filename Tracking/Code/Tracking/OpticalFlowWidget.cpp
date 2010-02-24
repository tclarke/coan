/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AnimationServices.h"
#include "DesktopServices.h"
#include "ModelServices.h"
#include "LabeledSection.h"
#include "OpticalFlowWidget.h"
#include "PlugInManagerServices.h"
#include "RasterLayer.h"
#include "SessionManager.h"
#include "Slot.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "TrackingManager.h"
#include <QtGui/QComboBox>
#include <QtGui/QHBoxLayout>
#include <QtGui/QPushButton>
#include <vector>

OpticalFlowWidget::OpticalFlowWidget(QWidget* pParent) : LabeledSectionGroup(pParent)
{
   QWidget* pSelectWidget = new QWidget(this);
   QHBoxLayout* pSelectLayout = new QHBoxLayout(pSelectWidget);
   mpDataSelect = new QComboBox(pSelectWidget);
   mpDataSelect->setInsertPolicy(QComboBox::InsertAlphabetically);
   mpDataSelect->setDuplicatesEnabled(true);
   mpDataSelect->setEditable(false);
   pSelectLayout->addWidget(mpDataSelect, 5);
   VERIFYNR(connect(mpDataSelect, SIGNAL(currentIndexChanged(int)), this, SLOT(trackIndex(int))));

   QPushButton* pRefresh = new QPushButton("Refresh", pSelectWidget);
   pSelectLayout->addWidget(pRefresh);
   VERIFYNR(connect(pRefresh, SIGNAL(clicked()), this, SLOT(updateDataSelect())));

   pSelectLayout->addStretch(10);
   LabeledSection* pDataSelectSection = new LabeledSection("Data Set", this);
   pDataSelectSection->setSectionWidget(pSelectWidget);
   addSection(pDataSelectSection);

   addStretch(100);
   updateDataSelect();
   mpDataSelect->setCurrentIndex(mpDataSelect->findData(QString("")));
}

OpticalFlowWidget::~OpticalFlowWidget()
{
}

void OpticalFlowWidget::updateDataSelect()
{
   QVariant curId = mpDataSelect->itemData(mpDataSelect->currentIndex());
   mpDataSelect->clear();
   mpDataSelect->addItem("<none>", QString(""));
   std::vector<Window*> windows;
   Service<DesktopServices>()->getWindows(SPATIAL_DATA_WINDOW, windows);
   for (std::vector<Window*>::iterator window = windows.begin(); window != windows.end(); ++window)
   {
      SpatialDataView* pView = static_cast<SpatialDataWindow*>(*window)->getSpatialDataView();
      VERIFYNRV(pView);
      RasterLayer* pLayer = static_cast<RasterLayer*>(pView->getTopMostLayer(RASTER));
      VERIFYNRV(pLayer);
      if (pLayer->getAnimation() != NULL)
      {
         QString name = QString::fromStdString(pLayer->getName());
         QString id = QString::fromStdString(pLayer->getId());
         mpDataSelect->addItem(name, id);
      }
   }
   mpDataSelect->setCurrentIndex(mpDataSelect->findData(curId));
}

void OpticalFlowWidget::trackIndex(int idx)
{
   std::string sessionId = mpDataSelect->itemData(idx).toString().toStdString();
   RasterLayer* pLayer = dynamic_cast<RasterLayer*>(Service<SessionManager>()->getSessionItem(sessionId));
   std::vector<PlugIn*> manager = Service<PlugInManagerServices>()->getPlugInInstances(TrackingManager::spPlugInName);
   if (!manager.empty())
   {
      static_cast<TrackingManager*>(manager.front())->setTrackedLayer(pLayer);
   }
}