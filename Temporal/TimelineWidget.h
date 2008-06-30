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

#include "AnimationController.h"
#include "AnimationToolBar.h"
#include "AttachmentPtr.h"
#include "SessionExplorer.h"
#include "SpecialMetadata.h"
#include <QtCore/QDateTime>
#include <QtGui/QAbstractSpinBox>
#include <QtGui/QWidget>
#include <qwt_abstract_scale.h>

class Animation;
class AnimationController;
class QDateTimeEdit;
class QwtScaleDraw;
class RasterLayer;

#define FRAME_TIMES_METADATA_NAME (std::string("FrameTimes"))
#define FRAME_TIMES_METADATA_PATH (SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/" + FRAME_TIMES_METADATA_NAME)
namespace TimelineUtils
{
   QDateTime timetToQDateTime(double val);
   double QDateTimeToTimet(const QDateTime &dt);
   bool createAnimationForRasterLayer(RasterLayer *pRasterLayer, AnimationController *pController);
   void rescaleAnimation(Animation *pAnim, double newVal, bool scaleEnd);
   void moveAnimation(Animation *pAnim, double newStart);
}

class TimelineWidget : public QWidget, public QwtAbstractScale
{
   Q_OBJECT

public:
   TimelineWidget(QWidget *pParent = NULL);
   virtual ~TimelineWidget();

   void controllerChanged(Subject &subject, const std::string &signal, const boost::any &v);
   void currentFrameChanged(Subject &subject, const std::string &signal, const boost::any &v);
   void polishSessionExplorerContextMenu(Subject &subject, const std::string &signal, const boost::any &v);
   void rasterElementDropped(Subject &subject, const std::string &signal, const boost::any &v);

   void setRange(double vmin, double vmax, bool lg=false);

   virtual QSize sizeHint() const;
   virtual QSize minimumSizeHint() const;

   void setScaleDraw(QwtScaleDraw *pScaleDraw);
   const QwtScaleDraw *scaleDraw() const;

protected:
   enum DragTypeEnum {
      DRAGGING_NONE,
      DRAGGING_SCRUB,
      DRAGGING_MOVE,
      DRAGGING_LEFT,
      DRAGGING_RIGHT,
   };

   QCursor dragTypeToQCursor(DragTypeEnum dragType, bool mouseDown) const;

   void draw(QPainter *pPainter, const QRect &update_rect);
   void layout(bool update_geometry=true);
   QwtScaleDraw *scaleDraw();
   Animation *hit(QPoint loc, DragTypeEnum &dragType) const;

   virtual void mouseMoveEvent(QMouseEvent *pEvent);
   virtual void mousePressEvent(QMouseEvent *pEvent);
   virtual void mouseReleaseEvent(QMouseEvent *pEvent);
   virtual void paintEvent(QPaintEvent *pEvent);
   virtual void resizeEvent(QResizeEvent *pEvent);

private slots:
   void createNewController(bool skipGui = false, bool timeBased = true, std::string name = std::string());
   void updateThrow();

private:
   void moveAnimation(Animation *pAnim, double newStart) const;
   void rescaleAnimation(Animation *pAnim, double newVal, bool scaleEnd) const;
   int transform(double value) const;
   void setAnimationController(AnimationController *pController);
   bool saveAnimationTimes(Animation  *pAnim);

   class PrivateData;
   PrivateData *mpD;

   DragTypeEnum mDragType;
   Animation *mpDragging;
   QPoint mDragStartPos;
   QPoint mDragPos;
   double mDragAccel;
  
   AttachmentPtr<AnimationToolBar> mpToolbar;
   AttachmentPtr<AnimationController> mpControllerAttachments;
   AttachmentPtr<SessionExplorer> mpSessionExplorer;
   QAction *mpNewAnimationAction;
};

#endif