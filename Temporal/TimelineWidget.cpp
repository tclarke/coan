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
#include "AnimationServices.h"
#include "AppConfig.h"
#include "AppVerify.h"
#include "ContextMenu.h"
#include "DesktopServices.h"
#include "LayerList.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterLayer.h"
#include "SessionExplorer.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "TimelineWidget.h"
#include <cmath>
#include <boost/any.hpp>
#include <boost/integer/static_log2.hpp>
#include <boost/rational.hpp>
#include <QtCore/QDateTime>
#include <QtGui/QButtonGroup>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QDateTimeEdit>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QHBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QMenu>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QRadioButton>
#include <QtGui/QResizeEvent>
#include <QtGui/QVBoxLayout>
#include <qwt_abstract_scale_draw.h>
#include <qwt_scale_draw.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_map.h>

namespace
{
   QDateTime timetToQDateTime(double val)
   {
      double seconds = floor(val);
      int milliseconds = static_cast<int>((val - seconds) * 1000.0);
      QDateTime dateTime = QDateTime::fromTime_t(static_cast<unsigned int>(seconds));
      dateTime.setTime(dateTime.time().addMSecs(milliseconds));
      dateTime = dateTime.toUTC();
      return dateTime;
   }
   double QDateTimeToTimet(const QDateTime &dt)
   {
      return dt.toTime_t() + (dt.time().msec() / 1000.0);
   }
}

class QwtTimeScaleDraw : public QwtScaleDraw
{
public:
   QwtTimeScaleDraw() {}
   QwtTimeScaleDraw(const QwtTimeScaleDraw &other) : QwtScaleDraw(other) {}
   virtual~QwtTimeScaleDraw() {}

   virtual QwtText label(double val) const
   {
      QDateTime dateTime = timetToQDateTime(val);
      return QwtText(dateTime.toString("yyyy/MM/dd\nhh:mm:ss.zzz") + "Z");
   }
};

class TimelineWidget::PrivateData
{
public:
   PrivateData() :
      borderWidth(5),
      scaleDist(10),
      animSpacing(5),
      animHeight(50),
      minValue(0.0),
      maxValue(1.0),
      mpController(NULL),
      animCount(0)
   {
      map.setScaleInterval(minValue, maxValue);
   }

   QwtScaleMap map;
   QRect animRect;

   int borderWidth;
   int scaleDist;
   int animSpacing;
   int animHeight;

   double minValue;
   double maxValue;

   AnimationController *mpController;
   int animCount;
};

TimelineWidget::TimelineWidget(QWidget *pParent) : QWidget(pParent),
      mpToolbar(SIGNAL_NAME(AnimationToolBar, ControllerChanged), Slot(this, &TimelineWidget::controllerChanged)),
      mpControllerAttachments(SIGNAL_NAME(AnimationController, FrameChanged), Slot(this, &TimelineWidget::currentFrameChanged)),
      mpSessionExplorer(SIGNAL_NAME(DockWindow, AboutToShowContextMenu), Slot(this, &TimelineWidget::polishSessionExplorerContextMenu))
{
   mpControllerAttachments.addSignal(SIGNAL_NAME(AnimationController, AnimationAdded), Slot(this, &TimelineWidget::currentFrameChanged));

   mpD = new PrivateData;
   setRange(mpD->minValue, mpD->maxValue, false);
   setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

   AnimationToolBar *pToolBar = static_cast<AnimationToolBar*>(Service<DesktopServices>()->getWindow("Animation", TOOLBAR));
   mpToolbar.reset(pToolBar);
   setAnimationController(pToolBar->getAnimationController());

   // install context menu listeners
   SessionExplorer *pSes = static_cast<SessionExplorer*>(Service<DesktopServices>()->getWindow("Session Explorer", DOCK_WINDOW));
   mpSessionExplorer.reset(pSes);
   mpNewAnimationAction = new QAction("New Animation", this);
   mpNewAnimationAction->setStatusTip("Create an empty animation controller of a specified type.");
   VERIFYNR(connect(mpNewAnimationAction, SIGNAL(triggered()), this, SLOT(createNewController())));
}

TimelineWidget::~TimelineWidget()
{
   delete mpD;
}

void TimelineWidget::controllerChanged(Subject &subject, const std::string &signal, const boost::any &v)
{
   AnimationController *pController = boost::any_cast<AnimationController*>(v);
   setAnimationController(pController);
}

void TimelineWidget::currentFrameChanged(Subject &subject, const std::string &signal, const boost::any &v)
{
   update();
}

void TimelineWidget::polishSessionExplorerContextMenu(Subject &subject, const std::string &signal, const boost::any &v)
{
   SessionExplorer &ses = dynamic_cast<SessionExplorer&>(subject);
   if(ses.getItemViewType() == SessionExplorer::ANIMATION_ITEMS)
   {
      ContextMenu *pMenu = boost::any_cast<ContextMenu*>(v);
      VERIFYNRV(pMenu);
      pMenu->addAction(mpNewAnimationAction, "TIMELINEWIDGET_NEW_ANIMATION_ACTION");
   }
}

void TimelineWidget::rasterElementDropped(Subject &subject, const std::string &signal, const boost::any &v)
{
   SessionItem *pItem = boost::any_cast<SessionItem*>(v);
   if(pItem == NULL || mpD->mpController == NULL)
   {
      return;
   }
   RasterElement *pRasterElement = dynamic_cast<RasterElement*>(pItem);
   RasterLayer *pRasterLayer = dynamic_cast<RasterLayer*>(pItem);
   if(pRasterElement != NULL)
   {
      std::vector<Window*> windows;
      Service<DesktopServices>()->getWindows(SPATIAL_DATA_WINDOW, windows);
      for(std::vector<Window*>::iterator window = windows.begin(); window != windows.end(); ++window)
      {
         SpatialDataView *pView = static_cast<SpatialDataWindow*>(*window)->getSpatialDataView();
         std::vector<Layer*> layers;
         pView->getLayerList()->getLayers(RASTER, layers);
         for(std::vector<Layer*>::iterator layer = layers.begin(); layer != layers.end(); ++layer)
         {
            RasterLayer *pLayer = static_cast<RasterLayer*>(*layer);
            RasterElement *pElement = static_cast<RasterElement*>(pLayer->getDataElement());
            if(pElement == pRasterElement)
            {
               createAnimationForRasterLayer(pLayer);
            }
         }
      }
   }
   else if(pRasterLayer != NULL)
   {
      createAnimationForRasterLayer(pRasterLayer);
   }
   setAnimationController(mpD->mpController);
}

bool TimelineWidget::createAnimationForRasterLayer(RasterLayer *pRasterLayer)
{
   Animation *pAnim = mpD->mpController->createAnimation(pRasterLayer->getName());
   VERIFY(pAnim != NULL);

   unsigned int numFrames = 0;
   RasterElement *pRasterElement = dynamic_cast<RasterElement*>(pRasterLayer->getDataElement());
   if(pRasterElement != NULL)
   {
      const RasterDataDescriptor *pDescriptor =
         dynamic_cast<RasterDataDescriptor*>(pRasterElement->getDataDescriptor());
      if(pDescriptor != NULL)
      {
         numFrames = pDescriptor->getBandCount();
      }
   }
   double frameTime = mpD->mpController->getStartFrame();
   if(frameTime < 0.0)
   {
      frameTime = QDateTime::currentDateTime().toUTC().toTime_t();
   }
   std::vector<AnimationFrame> frames;
   for(unsigned int i = 0; i < numFrames; ++i)
   {
      AnimationFrame frame;
      frame.mFrameNumber = i;
      if(pAnim->getFrameType() == FRAME_TIME)
      {
         frame.mTime = frameTime;
         frameTime += 1.0;
      }
      frames.push_back(frame);
   }
   pAnim->setFrames(frames);
   pRasterLayer->setAnimation(pAnim);
   return true;
}

void TimelineWidget::setRange(double vmin, double vmax, bool lg)
{
   mpD->minValue = vmin;
   mpD->maxValue = vmax;
   setScaleEngine(new QwtLinearScaleEngine);
   mpD->map.setTransformation(scaleEngine()->transformation());
   mpD->map.setScaleInterval(mpD->minValue, mpD->maxValue);
   if(autoScale())
   {
      rescale(mpD->minValue, mpD->maxValue);
   }
   layout();
}

QSize TimelineWidget::sizeHint() const
{
   return minimumSizeHint();
}

QSize TimelineWidget::minimumSizeHint() const
{
   int scaleWidth = scaleDraw()->minLength(QPen(), font());
   int scaleHeight = scaleDraw()->extent(QPen(), font());
   int animHeight = mpD->animRect.height();
   int w = qwtMin(scaleWidth, 200); // at least 200 pixels wide
   int h = scaleHeight + animHeight + mpD->scaleDist + 2 * mpD->borderWidth;
   return QSize(w, h);
}

void TimelineWidget::setScaleDraw(QwtScaleDraw *pScaleDraw)
{
   setAbstractScaleDraw(pScaleDraw);
}

const QwtScaleDraw *TimelineWidget::scaleDraw() const
{
   return static_cast<const QwtScaleDraw*>(abstractScaleDraw());
}

QwtScaleDraw *TimelineWidget::scaleDraw()
{
   return const_cast<QwtScaleDraw*>(static_cast<const QwtScaleDraw*>(abstractScaleDraw()));
}

void TimelineWidget::setAnimationController(AnimationController *pController)
{
   mpD->mpController = pController;
   setEnabled(mpD->mpController != NULL);
   mpControllerAttachments.reset(mpD->mpController);
   if(mpD->mpController == NULL)
   {
      setScaleDraw(new QwtScaleDraw);
      setRange(0.0, 1.0);
      mpD->animCount = 0;
   }
   else
   {
      if(mpD->mpController->getFrameType() == FRAME_TIME)
      {
        setScaleDraw(new QwtTimeScaleDraw);
      }
      else
      {
         setScaleDraw(new QwtScaleDraw);
      }
      setRange(mpD->mpController->getStartFrame(), mpD->mpController->getStopFrame());
      mpD->animCount = mpD->mpController->getNumAnimations();
   }
   layout();
   update();
}

void TimelineWidget::paintEvent(QPaintEvent *pEvent)
{
   const QRect &ur = pEvent->rect();
   if(ur.isValid())
   {
      QPainter painter(this);
      draw(&painter, ur);
   }
}

void TimelineWidget::draw(QPainter *pPainter, const QRect &update_rect)
{
   scaleDraw()->draw(pPainter, palette());

   if(mpD->animCount > 0)
   {
      const std::vector<Animation*> &animations = mpD->mpController->getAnimations();
      int ypos = mpD->animRect.y();
      for(std::vector<Animation*>::const_iterator animation = animations.begin(); animation != animations.end(); ++animation)
      {
         int xpos = transform((*animation)->getStartValue());
         int xend = transform((*animation)->getStopValue());
         pPainter->setBrush(palette().brush(QPalette::Base));
         pPainter->drawRect(xpos, ypos, xend - xpos, mpD->animHeight);
         qDrawShadePanel(pPainter, xpos, ypos, xend - xpos, mpD->animHeight, palette());
         int textXpos = xpos + mpD->borderWidth;
         int textYpos = ypos + (fontMetrics().height() + mpD->animHeight) / 2;
         pPainter->drawText(textXpos, textYpos, QString::fromStdString((*animation)->getName()));
         ypos += mpD->animHeight + mpD->animSpacing;
      }
     int lineX = transform(mpD->mpController->getCurrentFrame());
     pPainter->setPen(QPen(QBrush(Qt::darkGreen), 1, Qt::DashLine, Qt::RoundCap));
     pPainter->drawLine(lineX, mpD->borderWidth, lineX, mpD->animRect.bottom());
   }
}

void TimelineWidget::layout(bool update_geometry)
{
   QRect r = rect();
   int d1, d2;
   scaleDraw()->getBorderDistHint(font(), d1, d2);
   int mbd = qwtMax(d1, d2);
   int scaleHeight = scaleDraw()->extent(QPen(), font());

   mpD->animRect.setRect(
      r.x() + mbd + mpD->borderWidth,
      r.y() + scaleHeight + mpD->borderWidth + mpD->scaleDist,
      r.width() - 2 * (mpD->borderWidth + mbd),
      mpD->animCount * (mpD->animHeight + mpD->animSpacing));
   scaleDraw()->setAlignment(QwtScaleDraw::TopScale);
   scaleDraw()->move(mpD->animRect.x(), mpD->animRect.y() - mpD->scaleDist);
   scaleDraw()->setLength(mpD->animRect.width());

   // paint the scale in this area
   mpD->map.setPaintInterval(mpD->animRect.x(), mpD->animRect.x() + mpD->animRect.width() - 1);

   if(update_geometry)
   {
      updateGeometry();
      update();
   }
}

void TimelineWidget::resizeEvent(QResizeEvent *pEvent)
{
   layout(false);
}

void TimelineWidget::createNewController()
{
   NewControllerDialog dlg(this);
   if(dlg.exec() == QDialog::Accepted)
   {
      AnimationController *pController = Service<AnimationServices>()->createAnimationController(
         dlg.name().toStdString(), dlg.isTimeBased() ? FRAME_TIME : FRAME_ID);
      if(pController == NULL)
      {
         Service<DesktopServices>()->showMessageBox("Create Animation Constoler", "Unable to create animation controller.", "Ok");
      }
      else if(mpToolbar.get() != NULL)
      {
         mpToolbar.get()->setAnimationController(pController);
      }
   }
}

int TimelineWidget::transform(double value) const
{
   const double min = qwtMin(mpD->map.s1(), mpD->map.s2());
   const double max = qwtMax(mpD->map.s1(), mpD->map.s2());
   value = qwtMax(qwtMin(value, max), min);
   return mpD->map.transform(value);
}

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