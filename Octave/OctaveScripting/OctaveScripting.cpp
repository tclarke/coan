/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU General Public License Version 2
 * The license text is available from   
 * http://www.gnu.org/licenses/gpl.html
 */

#include "ConfigurationSettings.h"
#include "ModuleManager.h"
#include "OctaveVersion.h"
#include "OctaveScripting.h"
#include "PlugInFactory.h"
#ifdef WIN32
#undef WIN32 // redefined without ifdef in the octave headers
#endif
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

   const char *argv[1] = {"foo"};
   octave_main(1, (char**)argv, 1);
   octave_value osVal(octave_uint64(reinterpret_cast<uint64_t>(ModuleManager::instance()->getService())));
   set_global_value("opticksservice", osVal);

   Service<ConfigurationSettings> pSettings;
   ConfigurationSettingsExt2* pSettings2 = dynamic_cast<ConfigurationSettingsExt2*>(pSettings.get());
   if (pSettings2 != NULL)
   {
      std::string path = pSettings2->getPlugInPath();
      octave_call(QString("addpath(\"%1/oct\");").arg(QString::fromStdString(path)));
   }
   append(QString::fromStdString(mErr.str()));
   append(QString::fromStdString(mOut.str()));
   append("octave> ");
   mErr.str("");
   mOut.str("");
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