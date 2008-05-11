/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef TIMELINEWIDGET_H__
#define TIMELINEWIDGET_H__

#include "AnimationToolBar.h"
#include "AttachmentPtr.h"
#include <QtGui/QGraphicsRectItem>
#include <QtGui/QWidget>

class Animation;
class AnimationController;
class QGraphicsScene;
class QGraphicsView;

class QGraphicsAnimationItem : public QGraphicsRectItem
{
public:
   QGraphicsAnimationItem(Animation *pAnimation, QGraphicsItem *pParent = NULL);
   virtual ~QGraphicsAnimationItem();

protected:
   virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *pEvent);
   
private:
   Animation *mpAnimation;
};

class TimelineWidget : public QWidget
{
   Q_OBJECT

public:
   TimelineWidget(QWidget *pParent = NULL);
   virtual ~TimelineWidget();

   void controllerChanged(Subject &subject, const std::string &signal, const boost::any &v);

private:
   void setAnimationController(AnimationController *pController);

   AttachmentPtr<AnimationToolBar> mpToolbar;
   AnimationController *mpController;
   QGraphicsView *mpView;
   QGraphicsScene *mpScene;
};

#endif