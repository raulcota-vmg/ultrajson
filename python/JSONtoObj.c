/*
Developed by ESN, an Electronic Arts Inc. studio. 
Copyright (c) 2014, Electronic Arts Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
* Neither the name of ESN, Electronic Arts Inc. nor the
names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ELECTRONIC ARTS INC. BE LIABLE 
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


Portions of code from MODP_ASCII - Ascii transformations (upper/lower, etc)
http://code.google.com/p/stringencoders/
Copyright (c) 2007  Nick Galbreath -- nickg [at] modp [dot] com. All rights reserved.

Numeric decoder derived from from TCL library
http://www.opensource.apple.com/source/tcl/tcl-14/tcl/license.terms
 * Copyright (c) 1988-1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
*/

#include "py_defines.h"
#include <ultrajson.h>


//#define PRINTMARK() fprintf(stderr, "%s: MARK(%d)\n", __FILE__, __LINE__)
#define PRINTMARK()

static void Object_objectAddKey(void *prv, JSOBJ obj, JSOBJ name, JSOBJ value)
{
  PyDict_SetItem (obj, name, value);
  Py_DECREF( (PyObject *) name);
  Py_DECREF( (PyObject *) value);
  return;
}

static void Object_arrayAddItem(void *prv, JSOBJ obj, JSOBJ value)
{
  PyList_Append(obj, value);
  Py_DECREF( (PyObject *) value);
  return;
}

static JSOBJ Object_newString(void *prv, wchar_t *start, wchar_t *end)
{
  return PyUnicode_FromWideChar (start, (end - start));
}

static JSOBJ Object_newTrue(void *prv)
{
  Py_RETURN_TRUE;
}

static JSOBJ Object_newFalse(void *prv)
{
  Py_RETURN_FALSE;
}

static JSOBJ Object_newNull(void *prv)
{
  Py_RETURN_NONE;
}

static JSOBJ Object_newObject(void *prv)
{
  return PyDict_New();
}

static JSOBJ Object_newArray(void *prv)
{
  return PyList_New(0);
}

static JSOBJ Object_newInteger(void *prv, JSINT32 value)
{
  return PyInt_FromLong( (long) value);
}

static JSOBJ Object_newLong(void *prv, JSINT64 value)
{
  return PyLong_FromLongLong (value);
}

static JSOBJ Object_newUnsignedLong(void *prv, JSUINT64 value)
{
  return PyLong_FromUnsignedLongLong (value);
}

static JSOBJ Object_newDouble(void *prv, double value)
{
  return PyFloat_FromDouble(value);
}

static void Object_releaseObject(void *prv, JSOBJ obj)
{
  Py_DECREF( ((PyObject *)obj));
}

static void Object_readNextSection(JSONObjectDecoder *dec, 
                                  const char ** buffer, size_t *cbBuffer, 
                                  char * location, int doStep, int * atEOF)
{
  //Read the next section from the file for decoding


  //Pointer to read function
  PyObject *pyReadFunction = (PyObject *)dec->readFunction;

  //Section that will be read
  PyObject *pySection = NULL;

  //Variables in case I need concatenation
  PyObject *pyTemp = NULL;
  const char * prevSection;
  int concatenate = 0;
  PyObject *pyPrevSection = NULL;
  size_t offset = 0;


  //if (contiguousOffset && !dec->currentSection)
  //  contiguousOffset = 0;

  //First check if I had a pevious section loaded and I want to concatenate
  
  if (dec->currentSection) {
    //If I do, there are two possibilities, that I am at the end and stepping
    //or not. If at the end and stepping, then I just start in a clean string
    pyPrevSection = dec->currentSection;
    prevSection = PyString_AS_STRING(pyPrevSection);
    size_t prevSize = PyString_GET_SIZE(pyPrevSection);
    if ((location >= (prevSection + prevSize - 1)) && (doStep)) {
      //At last character (before null)  and stepping, Don't bother to concatenate
      concatenate = 0;
    }
    
    else {
      concatenate = 1;

      //Define an offset for when after it is concatenated
      offset = location - prevSection;
    }
  }

  if (!concatenate)
    //Remove and decref old python string if needed
    Py_CLEAR(dec->currentSection);
  else {
    //Just make this blank but don't dec ref just yet
    dec->currentSection = NULL;
  }




  //Clear this
  *cbBuffer = 0;

  //Read a specific set of characters
  PyObject *pyReadSize = PyInt_FromSize_t(dec->streamFromFile);
  PyObject *tuple = Py_BuildValue("(O)", pyReadSize);
  pySection = PyObject_CallObject(pyReadFunction, tuple);

  Py_XDECREF(pyReadSize);
  Py_XDECREF(tuple);
  if (pySection == NULL)
  {
    return;
  }

  // Validate the string
  if (!PyString_Check(pySection))
  {
    if (PyUnicode_Check(pySection))
    {
      pyTemp = pySection;
      pySection = PyUnicode_AsUTF8String(pyTemp);
      if (pySection == NULL)
      {
        //Exception raised above us by codec according to docs
        Py_XDECREF(pyTemp);
        return;
      }
    }
    else
    {
      Py_XDECREF(pySection);
      PyErr_Format(PyExc_TypeError, "Expected String or Unicode");
      return;
    }
  }

  if (PyString_GET_SIZE(pySection) < dec->streamFromFile)
    *atEOF = 1;

  if (concatenate) {
    //Put them together. pySection will decrease its ref count
    PyString_ConcatAndDel(&pyPrevSection, pySection);

    //Now make it one big section
    pySection = pyPrevSection;
  }


  //Keep track of it
  const char * temp = PyString_AS_STRING(pySection);
  temp += offset;
  *buffer = temp;
  *cbBuffer = ( PyString_GET_SIZE(pySection) - offset );


  dec->currentSection = pySection;


}

static char *g_kwlist[] = {"obj", 
						"stream_from_file",
						NULL};


PyObject* _JSONToObj(PyObject* self, PyObject *args, PyObject *kwargs, PyObject *pyReadFunction);

PyObject* JSONToObj(PyObject* self, PyObject *args, PyObject *kwargs)
{
	PyObject *py = NULL;
	return _JSONToObj(self, args, kwargs, py);
}

PyObject* _JSONToObj(PyObject* self, PyObject *args, PyObject *kwargs, PyObject *pyReadFunction)
{
  PyObject *ret = NULL;
  PyObject *sarg = NULL;
  PyObject *arg = NULL;
  
  JSONObjectDecoder decoder =
  {
    Object_newString,
    Object_objectAddKey,
    Object_arrayAddItem,
    Object_newTrue,
    Object_newFalse,
    Object_newNull,
    Object_newObject,
    Object_newArray,
    Object_newInteger,
    Object_newLong,
    Object_newUnsignedLong,
    Object_newDouble,
    Object_releaseObject,
    PyObject_Malloc,
    PyObject_Free,
    PyObject_Realloc,
    0,
    NULL,
    Object_readNextSection,
    NULL
  };

  decoder.prv = NULL;

  int streamFromFile;

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|i", g_kwlist, &arg, &(streamFromFile)))
  //if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", g_kwlist, &arg))
  {
      return NULL;
  }


  if (streamFromFile) {
    //Must have a file to stream it
    if (!pyReadFunction)
      streamFromFile = 0;
  }

  if (streamFromFile) {
    //Configure for live file reading
    decoder.streamFromFile = (size_t)streamFromFile;
    decoder.readFunction = pyReadFunction;

    //Make sure the section size is big enough. No need to be shy here
    if (decoder.streamFromFile < (FORCE_CONTIGUOUS_STRING + FORCE_CONTIGUOUS_NUMERIC)) {
      //Just use a default value
      decoder.streamFromFile = DEFAULT_STREAM_SIZE;
    }
  }

  else {
    // Validate the string
    if (PyString_Check(arg))
    {
      sarg = arg;
    }
    else
      if (PyUnicode_Check(arg))
      {
        sarg = PyUnicode_AsUTF8String(arg);
        if (sarg == NULL)
        {
          //Exception raised above us by codec according to docs
          return NULL;
        }
      }
      else
      {
        PyErr_Format(PyExc_TypeError, "Expected String or Unicode");
        return NULL;
      }

  }


  decoder.errorStr = NULL;
  decoder.errorOffset = NULL;

  dconv_s2d_init(DCONV_S2D_ALLOW_TRAILING_JUNK, 0.0, 0.0, "Infinity", "NaN");

  if (decoder.streamFromFile) {
    ret = JSON_DecodeObject(&decoder, NULL, 0);
  }
  else {
    ret = JSON_DecodeObject(&decoder, PyString_AS_STRING(sarg), PyString_GET_SIZE(sarg));
  }

  dconv_s2d_free();

  if (sarg != arg)
  {
    Py_XDECREF(sarg);
  }

  if (decoder.errorStr)
  {
    /*
    FIXME: It's possible to give a much nicer error message here with actual failing element in input etc*/

    PyErr_Format (PyExc_ValueError, "%s", decoder.errorStr);

    if (ret)
    {
        Py_DECREF( (PyObject *) ret);
    }

    return NULL;
  }

  return ret;
}

PyObject* JSONFileToObj(PyObject* self, PyObject *args, PyObject *kwargs)
{
  PyObject *read = NULL;
  PyObject *string = NULL;
  PyObject *result = NULL;
  PyObject *file = NULL;
  PyObject *argtuple = NULL;
  int streamFromFile;

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|i", g_kwlist, &file, &streamFromFile))
  //if (!PyArg_ParseTuple (args, "O", &file))
  {
    return NULL;
  }

  if (!PyObject_HasAttrString (file, "read"))
  {
    PyErr_Format (PyExc_TypeError, "expected file");
    return NULL;
  }

  read = PyObject_GetAttrString (file, "read");

  if (!PyCallable_Check (read)) {
    Py_XDECREF(read);
    PyErr_Format (PyExc_TypeError, "expected file");
    return NULL;
  }

  if (streamFromFile) {
	  //Don't read the entire string

    result = _JSONToObj(self, args, kwargs, read);

    Py_XDECREF(read);

  }
  else{
	  string = PyObject_CallObject (read, NULL);
    Py_XDECREF(read);
	  if (string == NULL)
	  {
		    return NULL;
	  }

    //Flip argument to to be the 
	  argtuple = PyTuple_Pack(1, string);

    result = _JSONToObj (self, argtuple, kwargs, NULL);
    Py_XDECREF(argtuple);

    Py_XDECREF(string);

  }
  

  if (result == NULL) {
    return NULL;
  }

  return result;
}
