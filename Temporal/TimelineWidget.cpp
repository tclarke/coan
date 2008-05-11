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
#include <QtGui/QGraphicsRectItem>
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QGraphicsSimpleTextItem>
#include <QtGui/QGraphicsView>
#include <QtGui/QRadialGradient>
#include <QtGui/QVBoxLayout>
#include <boost/any.hpp>

QGraphicsAnimationItem::QGraphicsAnimationItem(Animation *pAnimation, QGraphicsItem *pParent) : QGraphicsRectItem(pParent), mpAnimation(pAnimation)
{
   double start = mpAnimation->getStartValue();
   double stop = mpAnimation->getStopValue();
   setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
   setRect(QRect(start*10, 0, (stop - start)*10, 1));
   QGraphicsSimpleTextItem *pText = new QGraphicsSimpleTextItem(this);
   std::string name = mpAnimation->getDisplayName();
   if(name.empty())
   {
      name = mpAnimation->getName();
   }
   pText->setText(QString::fromStdString(name));
   QFontMetrics metrics(pText->font());
   double wscale = rect().width() / metrics.width(pText->text());
   double hscale = rect().height() / metrics.height();
   double scale = qMin(wscale, hscale) * 0.75;
   pText->scale(scale, scale);

   // TODO: This needs to be fixes
   QPointF textPos = mapFromParent(pos());
   textPos.setY(pText->pos().y());
   pText->setPos(textPos);
}

QGraphicsAnimationItem::~QGraphicsAnimationItem()
{
}

void QGraphicsAnimationItem::mouseMoveEvent(QGraphicsSceneMouseEvent *pEvent)
{
   if((pEvent->buttons() & Qt::LeftButton) && (flags() & QGraphicsItem::ItemIsMovable))
   {
      QPointF cur = scenePos(); // current item position in the scene
      double moveBy = pEvent->scenePos().x() - pEvent->lastScenePos().x(); // distance of the move in scene
      cur += QPointF(moveBy, 0); // move the item position by the associate scene difference
      setPos(mapToParent(mapFromScene(cur))); // set the new item position in parent coords
   }
   else
   {
      QGraphicsRectItem::mouseMoveEvent(pEvent);
   }
}

TimelineWidget::TimelineWidget(QWidget *pParent) : QWidget(pParent),
      mpToolbar(SIGNAL_NAME(AnimationToolBar, ControllerChanged), Slot(this, &TimelineWidget::controllerChanged)),
      mpController(NULL)
{
   QVBoxLayout *pTopLevel = new QVBoxLayout(this);
   mpView = new QGraphicsView(this);
   pTopLevel->addWidget(mpView);

   mpView->setInteractive(true);
   mpScene = new QGraphicsScene(this);
   mpView->setScene(mpScene);

   AnimationToolBar *pToolBar = static_cast<AnimationToolBar*>(Service<DesktopServices>()->getWindow("Animation", TOOLBAR));
   mpToolbar.reset(pToolBar);
   setAnimationController(pToolBar->getAnimationController());
}

TimelineWidget::~TimelineWidget()
{
}

void TimelineWidget::controllerChanged(Subject &subject, const std::string &signal, const boost::any &v)
{
   AnimationController *pController = boost::any_cast<AnimationController*>(v);
   setAnimationController(pController);
}

void TimelineWidget::setAnimationController(AnimationController *pController)
{
   mpController = pController;
   setEnabled(mpController != NULL);
   if(mpController == NULL)
   {
      return;
   }
   QList<QGraphicsItem*> items = mpScene->items();
   foreach(QGraphicsItem *pItem, items)
   {
      mpScene->removeItem(pItem);
   }

   int idx = 0;
   std::vector<Animation*> animations = mpController->getAnimations();
   for(std::vector<Animation*>::iterator animation = animations.begin(); animation != animations.end(); ++animation)
   {
      QGraphicsAnimationItem *pItem = new QGraphicsAnimationItem(*animation);
      pItem->moveBy(0, idx++);
      QRadialGradient grad(pItem->rect().center(), pItem->rect().width() / 2);
      grad.setColorAt(0, palette().mid().color());
      grad.setColorAt(1, palette().midlight().color());
      pItem->setBrush(QBrush(grad));
      pItem->setPen(palette().mid().color());
      mpScene->addItem(pItem);
   }
   mpView->fitInView(mpScene->sceneRect());
}