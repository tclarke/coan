/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "OctaveVersion.h"
#include "OctaveScripting.h"
#include "PlugInFactory.h"
#include <octave/config.h>
#include <octave/error.h>
#include <octave/octave.h>
#include <octave/parse.h>
#include <octave/quit.h>
#include <octave/sighandlers.h>
#include <octave/symtab.h>
#include <octave/sysdep.h>
#include <octave/toplev.h>
#include <octave/unwind-prot.h>
#include <octave/variables.h>
#include <QtGui/QAction>
#include <QtGui/QKeyEvent>
#include <QtGui/QMessageBox>
#include <iostream>

PLUGINFACTORY(OctaveScripting);

OctaveScripting::OctaveScripting()
{
   setName("OctaveScripting");
   setDescription("Octave scripting plug-in");
   setDescriptorId("{FC993292-70AC-45F1-BF01-4E211B1FE657}");
   setCopyright(OCTAVE_COPYRIGHT);
   setVersion(OCTAVE_VERSION_NUMBER);
   setProductionStatus(OCTAVE_IS_PRODUCTION_RELEASE);
}

OctaveScripting::~OctaveScripting()
{
}

QAction* OctaveScripting::createAction()
{
   return new QAction("Octave Scripting Window", NULL);
}

QWidget* OctaveScripting::createWidget()
{
   return new ScriptWindow();
}

ScriptWindow::ScriptWindow(QWidget* pParent) : QTextEdit(pParent)
{
   std::cout.rdbuf(mOut.rdbuf());
   std::cerr.rdbuf(mErr.rdbuf());
   setTextInteractionFlags(Qt::TextEditorInteraction);
   setReadOnly(false);
   setWordWrapMode(QTextOption::WordWrap);
   append("octave> ");

   const char *argv[1] = {"foo"};
   octave_main(1, (char**)argv, 1);
}

ScriptWindow::~ScriptWindow()
{
   do_octave_atexit();
}

void ScriptWindow::keyPressEvent(QKeyEvent* pEvent)
{
   if (pEvent->key() == Qt::Key_Enter || pEvent->key() == Qt::Key_Return)
   {
      QStringList lines = toPlainText().split("\n", QString::SkipEmptyParts);
      if (!lines.empty())
      {
         QString cmd = lines.back();
         if (cmd.startsWith("octave> "))
         {
            cmd.remove(0, 8);
         }
         octave_call(cmd);
         append(QString::fromStdString(mErr.str()));
         append(QString::fromStdString(mOut.str()));
         mErr.str("");
         mOut.str("");
      }
      append("octave> ");
      pEvent->accept();
   }
   else
   {
      QTextEdit::keyPressEvent(pEvent);
   }
}

int ScriptWindow::octave_call(const QString& cmd)
{
   int parse_status = 0;
   try
   {
      octave_save_signal_mask();
      if (octave_set_current_context != 0)
      {
         unwind_protect::run_all();
         raw_mode(0);
         octave_restore_signal_mask();
      }
      can_interrupt = true;
      octave_catch_interrupts();
      octave_initialized = true;
      curr_sym_tab = top_level_sym_tab;
      reset_error_handler();
      eval_string(cmd.toStdString(), false, parse_status);
   }
   catch(octave_interrupt_exception)
   {
      recover_from_exception();
      error_state = -2;
   }
   catch(std::bad_alloc)
   {
      recover_from_exception();
      error_state = -3;
   }
   catch(...)
   {
      recover_from_exception();
      error_state = -4;
      QMessageBox::warning(this, "Octave warning", "Caught exception!");
   }
   octave_restore_signal_mask();
   octave_initialized = false;
   return error_state;
}

void ScriptWindow::recover_from_exception()
{
   unwind_protect::run_all();
   can_interrupt = true;
   octave_interrupt_immediately = 0;
   octave_interrupt_state = 0;
   octave_allocation_error = 0;
   octave_restore_signal_mask();
   octave_catch_interrupts();
}