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
#include "AppConfig.h"
#include "AppVerify.h"
#include "DesktopServices.h"
#include "TimelineWidget.h"
#include <boost/any.hpp>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QResizeEvent>
#include <qwt_abstract_scale_draw.h>
#include <qwt_scale_draw.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_map.h>

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
      mpControllerAttachments(SIGNAL_NAME(AnimationController, FrameChanged), Slot(this, &TimelineWidget::currentFrameChanged))
{
   pD = new PrivateData;
   setRange(pD->minValue, pD->maxValue, false);
   setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

   AnimationToolBar *pToolBar = static_cast<AnimationToolBar*>(Service<DesktopServices>()->getWindow("Animation", TOOLBAR));
   mpToolbar.reset(pToolBar);
   setAnimationController(pToolBar->getAnimationController());
}

TimelineWidget::~TimelineWidget()
{
   delete pD;
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

void TimelineWidget::setRange(double vmin, double vmax, bool lg)
{
   pD->minValue = vmin;
   pD->maxValue = vmax;
   setScaleEngine(new QwtLinearScaleEngine);
   pD->map.setTransformation(scaleEngine()->transformation());
   pD->map.setScaleInterval(pD->minValue, pD->maxValue);
   if(autoScale())
   {
      rescale(pD->minValue, pD->maxValue);
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
   int animHeight = pD->animRect.height();
   int w = qwtMin(scaleWidth, 200); // at least 200 pixels wide
   int h = scaleHeight + animHeight + pD->scaleDist + 2 * pD->borderWidth;
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
   pD->mpController = pController;
   setEnabled(pD->mpController != NULL);
   mpControllerAttachments.reset(pD->mpController);
   if(pD->mpController == NULL)
   {
      setRange(0.0, 1.0);
      pD->animCount = 0;
   }
   else
   {
      setRange(pD->mpController->getStartFrame(), pD->mpController->getStopFrame());
      pD->animCount = pD->mpController->getNumAnimations();
   }
   layout();
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

   if(pD->animCount > 0)
   {
      const std::vector<Animation*> &animations = pD->mpController->getAnimations();
      int ypos = pD->animRect.y();
      for(std::vector<Animation*>::const_iterator animation = animations.begin(); animation != animations.end(); ++animation)
      {
         int xpos = transform((*animation)->getStartValue());
         int xend = transform((*animation)->getStopValue());
         qDrawShadePanel(pPainter, xpos, ypos, xend - xpos, pD->animHeight, palette());
         int textXpos = xpos + pD->borderWidth;
         int textYpos = ypos + (fontMetrics().height() + pD->animHeight) / 2;
         pPainter->drawText(textXpos, textYpos, QString::fromStdString((*animation)->getName()));
         ypos += pD->animHeight + pD->animSpacing;
      }
     int lineX = transform(pD->mpController->getCurrentFrame());
     pPainter->setPen(QPen(QBrush(Qt::darkGreen), 1, Qt::DashLine, Qt::RoundCap));
     pPainter->drawLine(lineX, pD->borderWidth, lineX, pD->animRect.bottom());
   }
}

void TimelineWidget::layout(bool update_geometry)
{
   QRect r = rect();
   int d1, d2;
   scaleDraw()->getBorderDistHint(font(), d1, d2);
   int mbd = qwtMax(d1, d2);
   int scaleHeight = scaleDraw()->extent(QPen(), font());

   pD->animRect.setRect(
      r.x() + mbd + pD->borderWidth,
      r.y() + scaleHeight + pD->borderWidth + pD->scaleDist,
      r.width() - 2 * (pD->borderWidth + mbd),
      pD->animCount * (pD->animHeight + pD->animSpacing));
   scaleDraw()->setAlignment(QwtScaleDraw::TopScale);
   scaleDraw()->move(pD->animRect.x(), pD->animRect.y() - pD->scaleDist);
   scaleDraw()->setLength(pD->animRect.width());

   // paint the scale in this area
   pD->map.setPaintInterval(pD->animRect.x(), pD->animRect.x() + pD->animRect.width() - 1);

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

int TimelineWidget::transform(double value) const
{
   const double min = qwtMin(pD->map.s1(), pD->map.s2());
   const double max = qwtMax(pD->map.s1(), pD->map.s2());
   value = qwtMax(qwtMin(value, max), min);
   return pD->map.transform(value);
}