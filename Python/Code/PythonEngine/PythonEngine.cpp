/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "MessageLogResource.h"
#include "OpticksModule.h"
#include "PythonEngine.h"
#include "PythonVersion.h"
#include "PlugInArgList.h"
#include "PlugInFactory.h"
#include "PlugInManagerServices.h"
#include "ProgressTracker.h"
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <sstream>

PLUGINFACTORY(PythonEngine);
PLUGINFACTORY(PythonInterpreter);

PythonEngine::PythonEngine()
{
   setName(PlugInName());
   setDescription("Python execution engine");
   setDescriptorId("{d47d3e19-5b7f-49aa-b02a-c8fbb1255591}");
   setCopyright(PYTHON_COPYRIGHT);
   setVersion(PYTHON_VERSION_NUMBER);
   setProductionStatus(PYTHON_IS_PRODUCTION_RELEASE);
   setType("Manager");
   executeOnStartup(true);
   destroyAfterExecute(false);
   allowMultipleInstances(false);
   setWizardSupported(false);
}

PythonEngine::~PythonEngine()
{
   if (mInterpreter.get() != NULL)
   {
      mInterpModule.reset(NULL);
      mStdin.reset(NULL);
      mStdout.reset(NULL);
      mStderr.reset(NULL);
      mInterpreter.reset(NULL);
      Py_Finalize();
   }
}

bool PythonEngine::getInputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return true;
}

bool PythonEngine::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return true;
}

bool PythonEngine::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   try
   {
      Py_Initialize();
      init_opticks();
      checkErr();
      auto_obj sysPath(PySys_GetObject("path"));
      /*std::string newPath =
         Service<ConfigurationSettings>()->getSettingSupportFilesPath()->getFullPathAndName() + "/site-packages";*/
      std::string newPath =
         "C:/Opticks/COAN/Python/Release/SupportFiles/site-packages";
      auto_obj newPathItem(PyString_FromString(newPath.c_str()), true);
      VERIFYNR(PyList_Append(sysPath, newPathItem) == 0);
      VERIFYNR(PySys_SetObject("path", sysPath) == 0);
      mInterpModule.reset(PyImport_ImportModule("interpreter"), true);
      checkErr();

      auto_obj interpDict(PyModule_GetDict(mInterpModule));
      auto_obj strBufStream(PyDict_GetItemString(interpDict, "StrBufStream"));
      checkErr();
      Py_INCREF(Py_None);
      Py_INCREF(Py_None);
      mStdin.reset(PyObject_CallObject(strBufStream, NULL), true);
      checkErr();
      Py_INCREF(Py_None);
      Py_INCREF(Py_None);
      mStdout.reset(PyObject_CallObject(strBufStream, NULL), true);
      checkErr();
      Py_INCREF(Py_None);
      Py_INCREF(Py_None);
      mStderr.reset(PyObject_CallObject(strBufStream, NULL), true);
      checkErr();

      auto_obj pythonInteractiveInterpreter(PyDict_GetItemString(interpDict, "PythonInteractiveInterpreter"));
      Py_INCREF(Py_None);
      Py_INCREF(Py_None);
      mInterpreter.reset(PyObject_CallObject(pythonInteractiveInterpreter, NULL), true);
      checkErr();

      PyObject_SetAttrString(mInterpreter, "stdin", mStdin);
      PyObject_SetAttrString(mInterpreter, "stdout", mStdout);
      PyObject_SetAttrString(mInterpreter, "stderr", mStderr);
      checkErr();
   }
   catch(const PythonError& err)
   {
      MessageResource msg("Error initializing python engine.", "python", "{d32b0337-63f2-43fc-8d82-0387a9b5d254}");
      msg->addProperty("Err", err.what());
      return false;
   }
   return true;
}

bool PythonEngine::processCommand(const std::string& command,
                                  std::string& returnText,
                                  std::string& errorText,
                                  Progress* pProgress)
{
   if (mInterpreter.get() == NULL)
   {
      return false;
   }
   try
   {
      auto_obj cnt(PyObject_CallMethod(mStdin, "write", "sl", command.c_str(), command.size()), true);
      checkErr();
      auto_obj useps1(PyObject_CallMethod(mInterpreter, "processEvent", NULL), true);
      checkErr();

      auto_obj stderrAvailable(PyObject_CallMethod(mStderr, "available", NULL), true);
      checkErr();
      long stderrCount = PyInt_AsLong(stderrAvailable);
      if (stderrCount > 0)
      {
         auto_obj stderrStr(PyObject_CallMethod(mStderr, "read", "O", stderrAvailable), true);
         checkErr();
         errorText = PyString_AsString(stderrStr);
      }
      auto_obj stdoutAvailable(PyObject_CallMethod(mStdout, "available", NULL), true);
      checkErr();
      long stdoutCount = PyInt_AsLong(stdoutAvailable);
      if (stdoutCount > 0)
      {
         auto_obj stdoutStr(PyObject_CallMethod(mStdout, "read", "O", stdoutAvailable), true);
         checkErr();
         returnText = PyString_AsString(stdoutStr);
      }
   }
   catch(const PythonError& err)
   {
      errorText = err.what();
      return false;
   }
   return true;
}

void PythonEngine::checkErr()
{
   if (PyErr_Occurred() != NULL)
   {
      PyObject* pException=NULL;
      PyObject* pVal=NULL;
      PyObject* pTb=NULL;
      PyErr_Fetch(&pException, &pVal, &pTb);
      PyErr_NormalizeException(&pException, &pVal, &pTb);
      PyErr_Clear();
      std::stringstream errStr;
      auto_obj traceback(PyImport_ImportModule("traceback"), true);
      if (PyErr_Occurred() != NULL)
      {
         PyErr_Clear();
         auto_obj msg(PyObject_GetAttrString(pVal, "message"), true);
         errStr << "Unable to import traceback module.\n"
            << PyObject_REPR(pException) << ": "
            << PyObject_REPR(msg) << "\npath="
            << PyObject_REPR(PySys_GetObject("path")) << std::endl;
      }
      else
      {
         auto_obj tracebackDict(PyModule_GetDict(traceback));
         auto_obj format_exception_only(PyDict_GetItemString(tracebackDict, "format_exception_only"));
         auto_obj exc(PyObject_CallFunction(format_exception_only, "OO", pException, pVal), true);
         errStr << PyObject_REPR(exc) << std::endl;
         if (pTb != NULL)
         {
            auto_obj format_tb(PyDict_GetItemString(tracebackDict, "format_tb"));
            auto_obj tb(PyObject_CallFunction(format_tb, "O", pTb), true);
            errStr << PyObject_REPR(tb) << std::endl;
         }
      }
      std::string tmpStr = errStr.str();
      for (std::string::size_type loc = tmpStr.find("\\n"); loc != std::string::npos; loc = tmpStr.find("\\n"))
      {
         tmpStr.replace(loc, 2, "\n");
      }
      throw PythonError(tmpStr);
   }
}


PythonInterpreter::PythonInterpreter()
{
   setName("PythonInterpreter");
   setDescription("Provides command line utilities to execute Python commands.");
   setDescriptorId("{171f067d-1927-4cf0-b505-f3d6bcc09493}");
   setCopyright(PYTHON_COPYRIGHT);
   setVersion(PYTHON_VERSION_NUMBER);
   setProductionStatus(PYTHON_IS_PRODUCTION_RELEASE);
   allowMultipleInstances(false);
   setWizardSupported(false);
}

PythonInterpreter::~PythonInterpreter()
{
}

bool PythonInterpreter::getInputSpecification(PlugInArgList*& pArgList)
{
   VERIFY((pArgList = Service<PlugInManagerServices>()->getPlugInArgList()) != NULL);
   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg(), NULL));
   VERIFY(pArgList->addArg<std::string>(Interpreter::CommandArg(), std::string()));

   return true;
}

bool PythonInterpreter::getOutputSpecification(PlugInArgList*& pArgList)
{
   VERIFY((pArgList = Service<PlugInManagerServices>()->getPlugInArgList()) != NULL);
   VERIFY(pArgList->addArg<std::string>(Interpreter::OutputTextArg()));
   VERIFY(pArgList->addArg<std::string>(Interpreter::ReturnTypeArg()));

   return true;
}

bool PythonInterpreter::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   VERIFY(pInArgList != NULL && pOutArgList != NULL);
   
   std::string command;
   if (!pInArgList->getPlugInArgValue(Interpreter::CommandArg(), command))
   {
      std::string returnType("Error");
      std::string errorText("Invalid command argument.");
      VERIFY(pOutArgList->setPlugInArgValue(Interpreter::ReturnTypeArg(), &returnType));
      VERIFY(pOutArgList->setPlugInArgValue(Interpreter::OutputTextArg(), &errorText));
      return true;
   }
   Progress* pProgress = pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg());
   std::vector<PlugIn*> plugins = Service<PlugInManagerServices>()->getPlugInInstances(PythonEngine::PlugInName());
   if (plugins.size() != 1)
   {
      std::string returnType("Error");
      std::string errorText("Unable to locate python engine.");
      VERIFY(pOutArgList->setPlugInArgValue(Interpreter::ReturnTypeArg(), &returnType));
      VERIFY(pOutArgList->setPlugInArgValue(Interpreter::OutputTextArg(), &errorText));
      return true;
   }
   PythonEngine* pEngine = dynamic_cast<PythonEngine*>(plugins.front());
   VERIFY(pEngine != NULL);

   std::string returnText;
   std::string errorText;
   if (!pEngine->processCommand(command, returnText, errorText, pProgress))
   {
      std::string returnType("Error");
      returnText += "\n" + errorText;
      VERIFY(pOutArgList->setPlugInArgValue(Interpreter::ReturnTypeArg(), &returnType));
      VERIFY(pOutArgList->setPlugInArgValue(Interpreter::OutputTextArg(), &returnText));
      return true;
   }
   // Populate the output arg list
   if (errorText.empty())
   {
      std::string returnType("Output");
      VERIFY(pOutArgList->setPlugInArgValue(Interpreter::ReturnTypeArg(), &returnType));
      VERIFY(pOutArgList->setPlugInArgValue(Interpreter::OutputTextArg(), &returnText));
   }
   else
   {
      std::string returnType("Error");
      VERIFY(pOutArgList->setPlugInArgValue(Interpreter::ReturnTypeArg(), &returnType));
      VERIFY(pOutArgList->setPlugInArgValue(Interpreter::OutputTextArg(), &errorText));
   }

   return true;
}

void PythonInterpreter::getKeywordList(std::vector<std::string>& list) const
{
   list.clear();
}

bool PythonInterpreter::getKeywordDescription(const std::string& keyword, std::string& description) const
{
   return false;
}

void PythonInterpreter::getUserDefinedTypes(std::vector<std::string>& list) const 
{
   list.clear();
}

bool PythonInterpreter::getTypeDescription(const std::string&, std::string&) const 
{
   return false;
}

PythonInterpreterWizardItem::PythonInterpreterWizardItem()
{
   setName("Python Interpreter");
   setDescription("Allow execution of Python code from within a wizard.");
   setDescriptorId("{2103eba4-f8d8-44be-9fb3-47fbbe8c6cc3}");
   setCopyright(PYTHON_COPYRIGHT);
   setVersion(PYTHON_VERSION_NUMBER);
   setProductionStatus(PYTHON_IS_PRODUCTION_RELEASE);
}

PythonInterpreterWizardItem::~PythonInterpreterWizardItem()
{
}

bool PythonInterpreterWizardItem::getInputSpecification(PlugInArgList*& pArgList)
{
   VERIFY((pArgList = Service<PlugInManagerServices>()->getPlugInArgList()) != NULL);
   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg(), NULL));
   VERIFY(pArgList->addArg<std::string>(Interpreter::CommandArg(), std::string()));

   return true;
}

bool PythonInterpreterWizardItem::getOutputSpecification(PlugInArgList*& pArgList)
{
   VERIFY((pArgList = Service<PlugInManagerServices>()->getPlugInArgList()) != NULL);
   VERIFY(pArgList->addArg<std::string>(Interpreter::OutputTextArg()));
   VERIFY(pArgList->addArg<std::string>(Interpreter::ReturnTypeArg()));

   return true;
}

bool PythonInterpreterWizardItem::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   VERIFY(pInArgList != NULL && pOutArgList != NULL);
   
   std::string command;
   if (!pInArgList->getPlugInArgValue(Interpreter::CommandArg(), command))
   {
      return false;
   }
   ProgressTracker progress(pInArgList->getPlugInArgValue<Progress>(ProgressArg()),
      "Execute Python command.", "python", "{5b5d5de3-faad-41ed-894b-dbe5a90d6d4d}");

   std::vector<PlugIn*> plugins = Service<PlugInManagerServices>()->getPlugInInstances(PythonEngine::PlugInName());
   if (plugins.size() != 1)
   {
      progress.report("Unable to locate the Python engine.", 0, ERRORS, true);
      return false;
   }
   PythonEngine* pEngine = dynamic_cast<PythonEngine*>(plugins.front());
   VERIFY(pEngine != NULL);

   progress.report("Executing Python command.", 1, NORMAL);
   std::string returnText;
   std::string errorText;
   if (!pEngine->processCommand(command, returnText, errorText, progress.getCurrentProgress()))
   {
      progress.report("Error executing Python command.", 0, ERRORS, true);
      return false;
   }
   // Populate the output arg list
   if (errorText.empty())
   {
      std::string returnType("Output");
      VERIFY(pOutArgList->setPlugInArgValue(Interpreter::ReturnTypeArg(), &returnType));
      VERIFY(pOutArgList->setPlugInArgValue(Interpreter::OutputTextArg(), &returnText));
   }
   else
   {
      std::string returnType("Error");
      VERIFY(pOutArgList->setPlugInArgValue(Interpreter::ReturnTypeArg(), &returnType));
      VERIFY(pOutArgList->setPlugInArgValue(Interpreter::OutputTextArg(), &errorText));
   }

   progress.report("Executing Python command.", 100, NORMAL);
   progress.upALevel();
   return true;
}
