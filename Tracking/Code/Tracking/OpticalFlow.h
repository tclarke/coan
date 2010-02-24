/*
 * The information in this file is
 * Copyright(c) 2010 Trevor R.H. Clarke
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef OPTICALFLOW_H__
#define OPTICALFLOW_H__

#include "DockWindowShell.h"

class OpticalFlow : public DockWindowShell
{
public:
   OpticalFlow();
   virtual ~OpticalFlow();

protected:
   virtual QAction* createAction();
   virtual QWidget* createWidget();

};

#endif