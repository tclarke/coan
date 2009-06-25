/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef PYTHONENGINE_H__
#define PYTHONENGINE_H__

#include "ExecutableShell.h"
#include "InterpreterShell.h"
#include "WizardShell.h"
#define PY_SSIZE_T_CLEAN
#include <Python.h>

class QsciLexer;

class auto_obj
{
public:
   auto_obj() : mpObj(NULL), mOwned(false) {}
   auto_obj(PyObject* pObj, bool takeOwnership=false) : mpObj(pObj), mOwned(takeOwnership) {}
   ~auto_obj()
   {
      if (mOwned)
      {
         Py_XDECREF(mpObj);
      }
   }

   /**
    * Reset the internal pointer.
    */
   void reset(PyObject* pObj, bool takeOwnership=false)
   {
      release();
      mpObj = pObj;
      mOwned = takeOwnership;
   }

   /**
    * Release ownership doing a decref if needed.
    */
   PyObject* release()
   {
      if (mpObj == NULL)
      {
         return NULL;
      }
      if (mOwned)
      {
         Py_DECREF(mpObj);
         mOwned = false;
      }
      return mpObj;
   }

   /**
    * Returns a borrowed reference. This object may not own the reference.
    */
   PyObject* get()
   {
      return mpObj;
   }

   /**
    * Returns a borrowed reference. Ensures this object owns the reference.
    */
   PyObject* take()
   {
      if (mpObj == NULL)
      {
         return NULL;
      }
      if (!mOwned)
      {
         Py_INCREF(mpObj);
         mOwned = true;
      }
      return mpObj;
   }

   operator PyObject*()
   {
      return get();
   }

private:
   PyObject* mpObj;
   bool mOwned;
};

class PythonEngine : public ExecutableShell
{
public:
   static std::string PlugInName() { return "Python Engine"; }
   PythonEngine();
   virtual ~PythonEngine();

   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

   virtual std::string getPrompt() const;
   bool processCommand(const std::string& command, std::string& returnText, std::string& errorText, Progress* pProgress);

   class PythonError : public std::exception
   {
   public:
      PythonError(const std::string& what) : std::exception(what.c_str()) {}
   };

protected:
   void checkErr();

private:
   auto_obj mInterpModule;
   auto_obj mStdin;
   auto_obj mStdout;
   auto_obj mStderr;
   auto_obj mInterpreter;
   std::string mPrompt;
};

class PythonInterpreter : public InterpreterShell
{
public:
   PythonInterpreter();
   virtual ~PythonInterpreter();
   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual void getKeywordList(std::vector<std::string>& list) const;
   virtual bool getKeywordDescription(const std::string& keyword, std::string& description) const;
   virtual void getUserDefinedTypes(std::vector<std::string>& list) const;
   virtual bool getTypeDescription(const std::string& type, std::string& description) const;

   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
   virtual std::string getPrompt() const;
   virtual QsciLexer* getLexer() const;
};

class PythonInterpreterWizardItem : public WizardShell
{
public:
   PythonInterpreterWizardItem();
   virtual ~PythonInterpreterWizardItem();
   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
};

#endif
