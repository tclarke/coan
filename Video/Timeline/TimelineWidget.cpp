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
#include "AnimationPropertiesDialog.h"
#include "AnimationServices.h"
#include "AppConfig.h"
#include "AppVerify.h"
#include "ContextMenu.h"
#include "DesktopServices.h"
#include "DynamicObject.h"
#include "LayerList.h"
#include "NewControllerDialog.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterLayer.h"
#include "SessionExplorer.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "ThresholdLayer.h"
#include "TimelineWidget.h"
#include "VideoUtils.h"
#include <cmath>
#include <boost/any.hpp>
#include <boost/bind.hpp>
#include <boost/integer/static_log2.hpp>
#include <boost/rational.hpp>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QDateTimeEdit>
#include <QtGui/QMenu>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QWheelEvent>
#include <qwt_abstract_scale_draw.h>
#include <qwt_scale_draw.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_map.h>

namespace
{
   AnimationFrame adjustFrameTime(AnimationFrame frame, double adjust)
   {
      frame.mTime += adjust;
      return frame;
   }

   AnimationFrame adjustFrameId(AnimationFrame frame, int adjust)
   {
      frame.mFrameNumber += adjust;
      return frame;
   }

   AnimationFrame scaleFrameTime(AnimationFrame frame, double start, double scale)
   {
      frame.mTime = start + scale * (frame.mTime - start);
      return frame;
   }

   AnimationFrame scaleFrameId(AnimationFrame frame, int start, double scale)
   {
      frame.mFrameNumber = start + static_cast<int>(scale * (frame.mFrameNumber - start));
      return frame;
   }
}

namespace TimelineUtils
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

   bool createAnimationForRasterLayer(RasterLayer *pRasterLayer, AnimationController *pController)
   {
      if(pRasterLayer == NULL || pController == NULL)
      {
         return false;
      }
      std::vector<double> frameTimes;
      unsigned int numFrames = 0;
      RasterElement *pRasterElement = dynamic_cast<RasterElement*>(pRasterLayer->getDataElement());
      if(pRasterElement != NULL)
      {
         const RasterDataDescriptor *pDescriptor =
            dynamic_cast<RasterDataDescriptor*>(pRasterElement->getDataDescriptor());
         if(pDescriptor != NULL)
         {
            numFrames = pDescriptor->getBandCount();
            const DynamicObject *pMetadata = pDescriptor->getMetadata();
            try
            {
               frameTimes = dv_cast<std::vector<double> >(pMetadata->getAttributeByPath(FRAME_TIMES_METADATA_PATH));
               if(frameTimes.size() < numFrames)
               {
                  frameTimes.clear();
               }
            }
            catch(const std::bad_cast&)
            {
               // do nothing
            }
         }
      }
      if(frameTimes.empty())
      {
         double frameTime = pController->getStartFrame();
         if(frameTime < 0.0)
         {
            frameTime = QDateTime::currentDateTime().toUTC().toTime_t();
         }
         frameTimes.reserve(numFrames);
         for(unsigned int i = 0; i < numFrames; i++)
         {
            frameTimes.push_back(frameTime);
            frameTime += 1.0;
         }
      }
      Animation *pAnim = pController->createAnimation(pRasterLayer->getName());
      VERIFY(pAnim != NULL);

      std::vector<AnimationFrame> frames;
      for(unsigned int i = 0; i < numFrames; ++i)
      {
         AnimationFrame frame;
         frame.mFrameNumber = i;
         if(pAnim->getFrameType() == FRAME_TIME)
         {
            frame.mTime = frameTimes[i];
         }
         frames.push_back(frame);
      }
      pAnim->setFrames(frames);
      pRasterLayer->setAnimation(pAnim);
      return true;
   }

   bool createAnimationForThresholdLayer(ThresholdLayer *pThresholdLayer, AnimationController *pController)
   {
      if(pThresholdLayer == NULL || pController == NULL)
      {
         return false;
      }
      std::vector<double> frameTimes;
      unsigned int numFrames = 0;
      RasterElement *pRasterElement = dynamic_cast<RasterElement*>(pThresholdLayer->getDataElement());
      if(pRasterElement != NULL)
      {
         const RasterDataDescriptor *pDescriptor =
            dynamic_cast<RasterDataDescriptor*>(pRasterElement->getDataDescriptor());
         if(pDescriptor != NULL)
         {
            numFrames = pDescriptor->getBandCount();
            const DynamicObject *pMetadata = pDescriptor->getMetadata();
            try
            {
               frameTimes = dv_cast<std::vector<double> >(pMetadata->getAttributeByPath(FRAME_TIMES_METADATA_PATH));
               if(frameTimes.size() < numFrames)
               {
                  frameTimes.clear();
               }
            }
            catch(const std::bad_cast&)
            {
               // do nothing
            }
         }
      }
      if(frameTimes.empty())
      {
         double frameTime = pController->getStartFrame();
         if(frameTime < 0.0)
         {
            frameTime = QDateTime::currentDateTime().toUTC().toTime_t();
         }
         frameTimes.reserve(numFrames);
         for(unsigned int i = 0; i < numFrames; i++)
         {
            frameTimes.push_back(frameTime);
            frameTime += 1.0;
         }
      }
      Animation *pAnim = pController->createAnimation(pThresholdLayer->getName());
      VERIFY(pAnim != NULL);

      std::vector<AnimationFrame> frames;
      for(unsigned int i = 0; i < numFrames; ++i)
      {
         AnimationFrame frame;
         frame.mFrameNumber = i;
         if(pAnim->getFrameType() == FRAME_TIME)
         {
            frame.mTime = frameTimes[i];
         }
         frames.push_back(frame);
      }
      pAnim->setFrames(frames);
      //pThresholdLayer->setAnimation(pAnim);
      return true;
   }

   void rescaleAnimation(Animation *pAnim, double newVal, bool scaleEnd)
   {
      if(pAnim == NULL)
      {
         return;
      }
      std::vector<AnimationFrame> frames = pAnim->getFrames();
      if(pAnim->getFrameType() == FRAME_TIME)
      {
         double start = pAnim->getStartValue();
         double scale = 1.0;
         if(scaleEnd)
         {
            scale = (newVal - start) / (pAnim->getStopValue() - start);
         }
         else
         {
            scale = (pAnim->getStopValue() - newVal) / (pAnim->getStopValue() - start);
         }
         std::transform(frames.begin(), frames.end(), frames.begin(), boost::bind(scaleFrameTime, _1, start, scale));
         if(!scaleEnd)
         {
            double diff = newVal - pAnim->getStartValue();
            std::transform(frames.begin(), frames.end(), frames.begin(), boost::bind(adjustFrameTime, _1, diff));
         }
      }
      else
      {
         // not currently supported
      }
      pAnim->setFrames(frames);
   }

   void moveAnimation(Animation *pAnim, double newStart)
   {
      if(pAnim == NULL)
      {
         return;
      }
      std::vector<AnimationFrame> frames = pAnim->getFrames();
      if(pAnim->getFrameType() == FRAME_TIME)
      {
         double diff = newStart - pAnim->getStartValue();
         std::transform(frames.begin(), frames.end(), frames.begin(), boost::bind(adjustFrameTime, _1, diff));
      }
      else
      {
         int diff = static_cast<int>(newStart - 0.5) - pAnim->getStartValue();
         std::transform(frames.begin(), frames.end(), frames.begin(), boost::bind(adjustFrameId, _1, diff));
      }
      pAnim->setFrames(frames);
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
      QDateTime dateTime = TimelineUtils::timetToQDateTime(val);
      return QwtText(dateTime.toString("yyyy/MM/dd\nhh:mm:ss.zzzZ"));
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
      mDragType(TimelineWidget::DRAGGING_NONE),
      mpDragging(NULL),
      mpToolbar(SIGNAL_NAME(AnimationToolBar, ControllerChanged), Slot(this, &TimelineWidget::controllerChanged)),
      mpControllerAttachments(SIGNAL_NAME(AnimationController, FrameChanged), Slot(this, &TimelineWidget::currentFrameChanged)),
      mpSessionExplorer(SIGNAL_NAME(DockWindow, AboutToShowContextMenu), Slot(this, &TimelineWidget::polishSessionExplorerContextMenu))
{
   mpControllerAttachments.addSignal(SIGNAL_NAME(AnimationController, AnimationAdded), Slot(this, &TimelineWidget::currentFrameChanged));

   mpD = new PrivateData;
   setRange(mpD->minValue, mpD->maxValue, false);
   setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
   setMouseTracking(true);
   setContextMenuPolicy(Qt::PreventContextMenu);

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

void TimelineWidget::sessionItemDropped(Subject &subject, const std::string &signal, const boost::any &v)
{
   SessionItem *pItem = boost::any_cast<SessionItem*>(v);
   if(pItem == NULL)
   {
      return;
   }
   RasterElement *pRasterElement = dynamic_cast<RasterElement*>(pItem);
   RasterLayer *pRasterLayer = dynamic_cast<RasterLayer*>(pItem);
   ThresholdLayer *pThresholdLayer = dynamic_cast<ThresholdLayer*>(pItem);
   if(pRasterElement != NULL)
   {
      std::vector<Window*> windows;
      Service<DesktopServices>()->getWindows(SPATIAL_DATA_WINDOW, windows);
      for(std::vector<Window*>::iterator window = windows.begin(); window != windows.end(); ++window)
      {
         SpatialDataView *pView = static_cast<SpatialDataWindow*>(*window)->getSpatialDataView();
         std::vector<Layer*> layers;
         pView->getLayerList()->getLayers(layers);
         for(std::vector<Layer*>::iterator layer = layers.begin(); layer != layers.end(); ++layer)
         {
            RasterLayer *pRasterLayer = dynamic_cast<RasterLayer*>(*layer);
            ThresholdLayer *pThresholdLayer = dynamic_cast<ThresholdLayer*>(*layer);
            RasterElement *pElement = static_cast<RasterElement*>((*layer)->getDataElement());
            if(pElement == pRasterElement && (pRasterLayer != NULL || pThresholdLayer != NULL))
            {
               if(mpD->mpController == NULL)
               {
                  createNewController(true, true, (*layer)->getName());
               }
               pView->setAnimationController(mpD->mpController);
               if(pRasterLayer != NULL)
               {
                  TimelineUtils::createAnimationForRasterLayer(pRasterLayer, mpD->mpController);
               }
               else
               {
                  TimelineUtils::createAnimationForThresholdLayer(pThresholdLayer, mpD->mpController);
               }
            }
         }
      }
   }
   else if(pRasterLayer != NULL)
   {
      if(mpD->mpController == NULL)
      {
         createNewController(true, true, pRasterLayer->getName());
      }
      TimelineUtils::createAnimationForRasterLayer(pRasterLayer, mpD->mpController);
   }
   else if(pThresholdLayer != NULL)
   {
      if(mpD->mpController == NULL)
      {
         createNewController(true, true, pThresholdLayer->getName());
      }
      TimelineUtils::createAnimationForThresholdLayer(pThresholdLayer, mpD->mpController);
   }
   layout(true);
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
   layout(true);
}

bool TimelineWidget::saveAnimationTimes(Animation  *pAnim)
{
   if(pAnim == NULL)
   {
      return false;
   }
   bool success = false;
   const std::vector<AnimationFrame> frames = pAnim->getFrames();
   std::vector<double> times(frames.size(), 0.0);
   for(unsigned int idx = 0; idx < frames.size(); idx++)
   {
      times[idx] = frames[idx].mTime;
   }

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
         if(pLayer->getAnimation() == pAnim)
         {
            RasterElement *pElement = static_cast<RasterElement*>(pLayer->getDataElement());
            DynamicObject *pMetadata = static_cast<RasterDataDescriptor*>(pElement->getDataDescriptor())->getMetadata();
            if(pMetadata != NULL)
            {
               success = success || pMetadata->setAttributeByPath(FRAME_TIMES_METADATA_PATH, times);
            }
         }
      }
   }
   return success;
}

QCursor TimelineWidget::dragTypeToQCursor(TimelineWidget::DragTypeEnum dragType, bool mouseDown) const
{
   switch(dragType)
   {
   case DRAGGING_SCRUB:
      if(mouseDown)
      {
         return QCursor(Qt::ClosedHandCursor);
      }
      break;
   case DRAGGING_MOVE:
      return QCursor(mouseDown ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
   case DRAGGING_LEFT:
   case DRAGGING_RIGHT:
      return QCursor(Qt::SizeHorCursor);
   }
   return QCursor(Qt::ArrowCursor);
}

Animation *TimelineWidget::hit(QPoint loc, TimelineWidget::DragTypeEnum &dragType) const
{
   if(mpD->mpController == NULL)
   {
      return NULL;
   }
   dragType = DRAGGING_NONE;
   const std::vector<Animation*> &animations = mpD->mpController->getAnimations();
   int ypos = mpD->animRect.y();
   for(std::vector<Animation*>::const_iterator animation = animations.begin(); animation != animations.end(); ++animation)
   {
      int yend = ypos + mpD->animHeight;
      if(loc.y() >= (ypos-2) && loc.y() <= (yend+2))
      {
         int xpos = transform((*animation)->getStartValue());
         int xend = transform((*animation)->getStopValue());
         if(loc.x() >= (xpos-2) && loc.x() <= (xend+2))
         {
            bool left = abs(loc.x() - xpos) < 5;
            bool right = abs(loc.x() - xend) < 5;
            if(left || right)
            {
               dragType = left ? DRAGGING_LEFT : DRAGGING_RIGHT;
            }
            else
            {
               dragType = DRAGGING_MOVE;
            }
            return *animation;
         }
         else
         {
            return NULL;
         }
      }
      ypos += mpD->animHeight + mpD->animSpacing;
   }
   return NULL;
}

void TimelineWidget::mouseMoveEvent(QMouseEvent *pEvent)
{
   switch(mDragType)
   {
   case DRAGGING_NONE:
      {
         DragTypeEnum dragType;
         Animation *pAnim = hit(pEvent->pos(), dragType);
         if((dragType != DRAGGING_LEFT && dragType != DRAGGING_RIGHT) ||
            (pAnim != NULL && pAnim->getFrameType() == FRAME_TIME))
         {
            // right now we only support resizing of time based animations
            setCursor(dragTypeToQCursor(dragType, false));
         }
         break;
      }
   case DRAGGING_SCRUB:
      {
         mDragPos = pEvent->pos();
         if(mpD->mpController != NULL)
         {
            int xpos = transform(mpD->mpController->getCurrentFrame());
            double xdiff = mDragPos.x() - mDragStartPos.x();
            mpD->mpController->setCurrentFrame(mpD->map.invTransform(xpos + xdiff));
            update();
         }
         int dragDiff = mDragPos.x() - mDragStartPos.x();
         mDragAccel = (mDragAccel * 3 + dragDiff) / 4;
         mDragStartPos = mDragPos;
         break;
      }
   default:
      mDragPos = pEvent->pos();
      update();
      break;
   }
}

void TimelineWidget::mousePressEvent(QMouseEvent *pEvent)
{
   if(pEvent->button() == Qt::LeftButton)
   {
      mpDragging = hit(pEvent->pos(), mDragType);
      mDragStartPos = mDragPos = pEvent->pos();
      if((mDragType == DRAGGING_LEFT || mDragType == DRAGGING_RIGHT) &&
         (mpDragging != NULL && mpDragging->getFrameType() == FRAME_ID))
      {
         // right now we only support resizing of time based animations
         mpDragging = NULL;
         mDragStartPos = QPoint();
         mDragType = DRAGGING_NONE;
      }
   }
   else if(pEvent->button() == Qt::MidButton && mpD->mpController != NULL)
   {
      int lineLoc = transform(mpD->mpController->getCurrentFrame());
      if(abs(lineLoc - pEvent->pos().x()) < 10)
      {
         mpDragging = NULL;
         mDragStartPos = mDragPos = pEvent->pos();
         mDragType = DRAGGING_SCRUB;
         mDragAccel = 0.0;
         QTimer::singleShot(0, this, SLOT(updateThrow()));
      }
   }
   else if(pEvent->button() == Qt::RightButton)
   {
      DragTypeEnum dummy;
      Animation *pAnim = hit(pEvent->pos(), dummy);
      QMenu *pMenu = new QMenu(this);
      QAction *pProps = pMenu->addAction("Properties");
      pProps->setEnabled(pAnim != NULL);
      QAction *pSaveTo = pMenu->addAction("Save Frame Times");
      pSaveTo->setEnabled(pAnim != NULL && pAnim->getFrameType() == FRAME_TIME);
      QAction *pChoice = pMenu->exec(pEvent->globalPos());
      if(pChoice == pSaveTo)
      {
         if(saveAnimationTimes(pAnim))
         {
            Service<DesktopServices>()->showMessageBox("Timeline", "Successfully saved frame times.");
         }
         else
         {
            Service<DesktopServices>()->showMessageBox("Timeline", "Unable saved frame times.");
         }
      }
      else if(pChoice == pProps)
      {
         AnimationPropertiesDialog dlg(pAnim, this);
         dlg.exec();
      }
   }
   setCursor(dragTypeToQCursor(mDragType, true));
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent *pEvent)
{
   if(pEvent->button() == Qt::LeftButton)
   {
      switch(mDragType)
      {
      case DRAGGING_MOVE:
         {
            int xpos = transform(mpDragging->getStartValue());
            double xdiff = pEvent->pos().x() - mDragStartPos.x();
            TimelineUtils::moveAnimation(mpDragging, mpD->map.invTransform(xpos + xdiff));
            break;
         }
      case DRAGGING_LEFT:
         TimelineUtils::rescaleAnimation(mpDragging, mpD->map.invTransform(pEvent->pos().x()), false);
         break;
      case DRAGGING_RIGHT:
         TimelineUtils::rescaleAnimation(mpDragging, mpD->map.invTransform(pEvent->pos().x()), true);
         break;
      }
   }
   else if(pEvent->button() == Qt::MidButton)
   {
      int dragDiff = (mDragPos.x() - mDragStartPos.x());
      mDragAccel = (mDragAccel * 3 + dragDiff) / 4;
   }
   mDragType = DRAGGING_NONE;
   mpDragging = NULL;
   setCursor(dragTypeToQCursor(mDragType, false));
   layout(true);
   mpD->mpController->setCurrentFrame(mpD->mpController->getCurrentFrame());
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
         int animWidth = xend - xpos;
         if(mpDragging == *animation)
         {
            int xdiff = mDragPos.x() - mDragStartPos.x();
            switch(mDragType)
            {
            case DRAGGING_MOVE:
               xpos += xdiff;
               break;
            case DRAGGING_LEFT:
               xpos += xdiff;
               animWidth -= xdiff;
               break;
            case DRAGGING_RIGHT:
               animWidth += xdiff;
               break;
            }
         }
         pPainter->setBrush(palette().brush(QPalette::Base));
         pPainter->drawRect(xpos, ypos, animWidth, mpD->animHeight);
         qDrawShadePanel(pPainter, xpos, ypos, animWidth, mpD->animHeight, palette());
         int textXpos = xpos + mpD->borderWidth;
         int textYpos = ypos + (fontMetrics().height() + mpD->animHeight) / 2;
         pPainter->drawText(textXpos, textYpos, QString::fromStdString((*animation)->getName()));
         if(mpDragging != *animation)
         {
            const std::vector<AnimationFrame> &frames = (*animation)->getFrames();
            pPainter->setPen(QPen(QBrush(Qt::lightGray), 1));
            int prevPos = 0;
            for(std::vector<AnimationFrame>::const_iterator frame = frames.begin(); frame != frames.end(); ++frame)
            {
               int framePos = transform((*animation)->getFrameType() == FRAME_ID ? frame->mFrameNumber : frame->mTime);
               while((framePos - prevPos) < 32 && ++frame != frames.end())
               {
                  framePos = transform((*animation)->getFrameType() == FRAME_ID ? frame->mFrameNumber : frame->mTime);
               }
               if(frame == frames.end())
               {
                  break;
               }
#pragma message(__FILE__ "(" STRING(__LINE__) ") : warning : Add code to draw thumbnails if they are available (tclarke)")
               pPainter->drawLine(framePos, ypos, framePos, ypos + mpD->animHeight);
               prevPos = framePos;
            }
         }
         ypos += mpD->animHeight + mpD->animSpacing;
      }
     int lineX = transform(mpD->mpController->getCurrentFrame());
     pPainter->setPen(QPen(QBrush(Qt::darkGreen), 1, Qt::DashLine, Qt::RoundCap));
     pPainter->drawLine(lineX, mpD->borderWidth, lineX, mpD->animRect.bottom());
   }
}

void TimelineWidget::layout(bool update_geometry)
{
   if(mpD->mpController == NULL)
   {
      setScaleDraw(new QwtScaleDraw);
      setRange(0.0, 1.0);
      mpD->animCount = 0;
      return;
   }
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

void TimelineWidget::wheelEvent(QWheelEvent *pEvent)
{
   if(mpD->mpController == NULL)
   {
      return;
   }

   int frames = pEvent->delta() / 120;
   if(pEvent->modifiers() == Qt::ShiftModifier)
   {
      frames *= 10;
   }
   else if(pEvent->modifiers() == Qt::ControlModifier)
   {
      frames *= 100;
   }
   while(frames != 0)
   {
      if(frames < 0)
      {
         mpD->mpController->stepBackward();
         frames++;
      }
      else
      {
         mpD->mpController->stepForward();
         frames--;
      }
   }
}

#pragma message(__FILE__ "(" STRING(__LINE__) ") : warning : TODO: add zooming and panning (inherit from scroll area?) (tclarke)")

void TimelineWidget::createNewController(bool skipGui, bool timeBased, std::string name)
{
   if(!skipGui)
   {
      NewControllerDialog dlg(this);
      if(dlg.exec() == QDialog::Accepted)
      {
         timeBased = dlg.isTimeBased();
         name = dlg.name().toStdString();
      }
      else
      {
         return;
      }
   }
   AnimationController *pController = Service<AnimationServices>()->createAnimationController(name, timeBased ? FRAME_TIME : FRAME_ID);
   if(pController == NULL)
   {
      Service<DesktopServices>()->showMessageBox("Create Animation Constoler", "Unable to create animation controller.", "Ok");
   }
   else if(mpToolbar.get() != NULL)
   {
      mpToolbar.get()->setAnimationController(pController);
   }
}

void TimelineWidget::updateThrow()
{
   if(mDragType != DRAGGING_SCRUB && mpD->mpController != NULL)
   {
      mDragPos = QPoint(mDragStartPos.x() + mDragAccel, 0);
      int xpos = transform(mpD->mpController->getCurrentFrame());
      double xdiff = mDragPos.x() - mDragStartPos.x();
      mpD->mpController->setCurrentFrame(mpD->map.invTransform(xpos + xdiff));
      update();
      mDragStartPos = mDragPos;
   }
   mDragAccel *= 0.8;
   if(fabs(mDragAccel) < 1.0)
   {
      mDragAccel = 0.0;
   }
   if(mDragAccel != 0.0 || mDragType == DRAGGING_SCRUB)
   {
      QTimer::singleShot(0, this, SLOT(updateThrow()));
   }
}

int TimelineWidget::transform(double value) const
{
   const double min = qwtMin(mpD->map.s1(), mpD->map.s2());
   const double max = qwtMax(mpD->map.s1(), mpD->map.s2());
   value = qwtMax(qwtMin(value, max), min);
   return mpD->map.transform(value);
}