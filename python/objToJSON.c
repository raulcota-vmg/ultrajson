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
#include <stdio.h>
#include <ultrajson.h>

#define EPOCH_ORD 719163
static PyObject* type_decimal = NULL;

typedef void *(*PFN_PyTypeToJSON)(JSOBJ obj, JSONTypeContext *ti, void *outValue, size_t *_outLen);

#if (PY_VERSION_HEX < 0x02050000)
typedef ssize_t Py_ssize_t;
#endif

typedef struct __TypeContext
{
  JSPFN_ITEREND iterEnd;
  JSPFN_ITERNEXT iterNext;
  JSPFN_ITERGETNAME iterGetName;
  JSPFN_ITERGETVALUE iterGetValue;
  PFN_PyTypeToJSON PyTypeToJSON;
  PyObject *newObj;
  PyObject *dictObj;
  Py_ssize_t index;
  Py_ssize_t size;
  char npArrType;
  PyObject *itemValue;
  PyObject *itemName;
  PyObject *attrList;
  PyObject *iterator;

  union
  {
    PyObject *rawJSONValue;
    JSINT64 longValue;
    JSUINT64 unsignedLongValue;
  };
} TypeContext;

#define GET_TC(__ptrtc) ((TypeContext *)((__ptrtc)->prv))

struct PyDictIterState
{
  PyObject *keys;
  size_t i;
  size_t sz;
};

//#define PRINTMARK() fprintf(stderr, "%s: MARK(%d)\n", __FILE__, __LINE__)
#define PRINTMARK()

void initObjToJSON(void)
{
  PyObject* mod_decimal = PyImport_ImportModule("decimal");
  if (mod_decimal)
  {
    type_decimal = PyObject_GetAttrString(mod_decimal, "Decimal");
    Py_INCREF(type_decimal);
    Py_DECREF(mod_decimal);
  }
  else
    PyErr_Clear();
}

#ifdef _LP64
static void *PyIntToINT64(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  PyObject *obj = (PyObject *) _obj;
  *((JSINT64 *) outValue) = PyInt_AS_LONG (obj);
  return NULL;
}
#else
static void *PyIntToINT32(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  PyObject *obj = (PyObject *) _obj;
  *((JSINT32 *) outValue) = PyInt_AS_LONG (obj);
  return NULL;
}
#endif

static void *PyLongToINT64(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  *((JSINT64 *) outValue) = GET_TC(tc)->longValue;
  return NULL;
}

static void *PyLongToUINT64(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  *((JSUINT64 *) outValue) = GET_TC(tc)->unsignedLongValue;
  return NULL;
}

static void *PyFloatToDOUBLE(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  PyObject *obj = (PyObject *) _obj;
  *((double *) outValue) = PyFloat_AsDouble (obj);
  return NULL;
}

static void *PyNPScalarDoubleToDOUBLE(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
	PyObject *obj = (PyObject *)_obj;
	*((double *)outValue) = ((PyDoubleScalarObject *)obj)->obval;
	return NULL;
}

static void *PyNPScalarIntToINT32(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  PyObject *obj = (PyObject *)_obj;
  *((JSINT32 *)outValue) = ((PyIntScalarObject *)obj)->obval;
  return NULL;
}

static void *PyNPScalarLongToINT32(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  PyObject *obj = (PyObject *)_obj;
  *((JSINT32 *)outValue) = ((PyLongScalarObject *)obj)->obval;
  return NULL;
}

/*
static void *PyNPScalarGenericIntToINT64(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
	PyObject *obj = (PyObject *)_obj;


  //This all is ok to convert to integers
  if (PyArray_IsScalar(obj, Byte))
  {
	  tc->type = (((PyByteScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
	  return;
  }
  else
  if (PyArray_IsScalar(obj, UByte))
  {
	  tc->type = (((PyUByteScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
	  return;
  }
  else
  if (PyArray_IsScalar(obj, Short))
  {
	  tc->type = (((PyShortScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
	  return;
  }
  else
  if (PyArray_IsScalar(obj, UShort))
  {
	tc->type = (((PyUShortScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
	return;
  }
  else
  if (PyArray_IsScalar(obj, Int))
  {
	tc->type = (((PyIntScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
	return;
  }
  else
  if (PyArray_IsScalar(obj, UInt))
  {
	tc->type = (((PyUIntScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
	return;
  }
  else
  if (PyArray_IsScalar(obj, Long))
  {
	tc->type = (((PyLongScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
	return;
  }
  else
  if (PyArray_IsScalar(obj, ULong))
  {
	tc->type = (((PyULongScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
	return;
  }
  else
  if (PyArray_IsScalar(obj, LongLong))
  {
	tc->type = (((PyLongLongScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
	return;
  }
  else
  if (PyArray_IsScalar(obj, ULongLong))
  {
	tc->type = (((PyULongLongScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
	return;
  }
	return NULL;
}
*/

static void *PyStringToUTF8(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  PyObject *obj = (PyObject *) _obj;
  *_outLen = PyString_GET_SIZE(obj);
  return PyString_AS_STRING(obj);
}

static void *PyUnicodeToUTF8(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  PyObject *obj = (PyObject *) _obj;
  PyObject *newObj;
#if (PY_VERSION_HEX >= 0x03030000)
  if (PyUnicode_IS_COMPACT_ASCII(obj))
  {
    Py_ssize_t len;
    char *data = PyUnicode_AsUTF8AndSize(obj, &len);
    *_outLen = len;
    return data;
  }
#endif
  newObj = PyUnicode_AsUTF8String(obj);
  if(!newObj)
  {
    return NULL;
  }

  GET_TC(tc)->newObj = newObj;

  *_outLen = PyString_GET_SIZE(newObj);
  return PyString_AS_STRING(newObj);
}

static void *PyRawJSONToUTF8(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  PyObject *obj = GET_TC(tc)->rawJSONValue;
  if (PyUnicode_Check(obj))
  {
    return PyUnicodeToUTF8(obj, tc, outValue, _outLen);
  }
  else
  {
    return PyStringToUTF8(obj, tc, outValue, _outLen);
  }
}

static int Tuple_iterNext(JSOBJ obj, JSONTypeContext *tc)
{
  PyObject *item;

  if (GET_TC(tc)->index >= GET_TC(tc)->size)
  {
    return 0;
  }

  item = PyTuple_GET_ITEM (obj, GET_TC(tc)->index);

  GET_TC(tc)->itemValue = item;
  GET_TC(tc)->index ++;
  return 1;
}

static void Tuple_iterEnd(JSOBJ obj, JSONTypeContext *tc)
{
}

static JSOBJ Tuple_iterGetValue(JSOBJ obj, JSONTypeContext *tc)
{
  return GET_TC(tc)->itemValue;
}

static char *Tuple_iterGetName(JSOBJ obj, JSONTypeContext *tc, size_t *outLen)
{
  return NULL;
}

static int List_iterNext(JSOBJ obj, JSONTypeContext *tc)
{
  if (GET_TC(tc)->index >= GET_TC(tc)->size)
  {
    PRINTMARK();
    return 0;
  }

  GET_TC(tc)->itemValue = PyList_GET_ITEM (obj, GET_TC(tc)->index);
  GET_TC(tc)->index ++;
  return 1;
}

static int ListFromNumpyArray_iterNext(JSOBJ obj, JSONTypeContext *tc)
{
	if (GET_TC(tc)->index >= GET_TC(tc)->size)
	{
		PRINTMARK();
		return 0;
	}

	GET_TC(tc)->itemValue = PyList_GET_ITEM(GET_TC(tc)->attrList, GET_TC(tc)->index);
	GET_TC(tc)->index++;
	return 1;
}

static void List_iterEnd(JSOBJ obj, JSONTypeContext *tc)
{
}

static JSOBJ List_iterGetValue(JSOBJ obj, JSONTypeContext *tc)
{
  return GET_TC(tc)->itemValue;
}

static char *List_iterGetName(JSOBJ obj, JSONTypeContext *tc, size_t *outLen)
{
  return NULL;
}

static char *NumpyArray_GetDType(JSOBJ obj, JSONTypeContext *tc, size_t *outLen)
{
	return &GET_TC(tc)->npArrType;
}


//=============================================================================
// Dict iteration functions
// itemName might converted to string (Python_Str). Do refCounting
// itemValue is borrowed from object (which is dict). No refCounting
//=============================================================================

static int Dict_iterNext(JSOBJ obj, JSONTypeContext *tc)
{
  
  PyObject* itemNameTmp;
  PyObject* itemValueTmp;

  //Clear previous iteration
  Py_CLEAR(GET_TC(tc)->itemName);
  Py_CLEAR(GET_TC(tc)->itemValue);


  //Get the item name and value. Quit if cannot
  itemNameTmp = PyIter_Next(GET_TC(tc)->iterator);
  if (!itemNameTmp)
  {
    PRINTMARK();
    return 0;
  }

	itemValueTmp = PyObject_GetItem(GET_TC(tc)->dictObj, itemNameTmp);
  if (!itemValueTmp)
  {
    Py_CLEAR(itemNameTmp);
    PRINTMARK();
    return 0;
  }

  //Assign value directly
  GET_TC(tc)->itemValue = itemValueTmp;


  //Now deal with the name. Make sure it becomes a non unicode string
  if (PyUnicode_Check(itemNameTmp))
  {
    //Encode, assign and decref of tmp
    GET_TC(tc)->itemName = PyUnicode_AsUTF8String (itemNameTmp);
    Py_CLEAR(itemNameTmp);
  }
  else
  if (!PyString_Check(itemNameTmp))
  {
    if (UNLIKELY(itemNameTmp == Py_None))
    {
      //None is special. Sort it out directly, assign and deref of tmp
      GET_TC(tc)->itemName = PyString_FromString("null");
      Py_CLEAR(itemNameTmp);
    }
    else {
      //Get blind string representation, assign and deref of tmp
      GET_TC(tc)->itemName = PyObject_Str(itemNameTmp);
      Py_CLEAR(itemNameTmp);

#if PY_MAJOR_VERSION >= 3
      //Python 3 comes out as unicode. Same drill
      //Encode, assign and decref of tmp
      itemNameTmp = GET_TC(tc)->itemName;
      GET_TC(tc)->itemName = PyUnicode_AsUTF8String(itemNameTmp);
      Py_CLEAR(itemNameTmp);
#endif
    }
  }
  else
  {
    //Assume is a string. Just assign as is. Leave ref count as it
    GET_TC(tc)->itemName = itemNameTmp;
  }
  PRINTMARK();
  return 1;
}

static void Dict_iterEnd(JSOBJ obj, JSONTypeContext *tc)
{
	Py_CLEAR(GET_TC(tc)->itemName);
	Py_CLEAR(GET_TC(tc)->itemValue);

  Py_CLEAR(GET_TC(tc)->iterator);
  Py_DECREF(GET_TC(tc)->dictObj);
  PRINTMARK();
}

static JSOBJ Dict_iterGetValue(JSOBJ obj, JSONTypeContext *tc)
{
  return GET_TC(tc)->itemValue;
}

static char *Dict_iterGetName(JSOBJ obj, JSONTypeContext *tc, size_t *outLen)
{
  *outLen = PyString_GET_SIZE(GET_TC(tc)->itemName);
  return PyString_AS_STRING(GET_TC(tc)->itemName);
}

static int SortedDict_iterNext(JSOBJ obj, JSONTypeContext *tc)
{
  PyObject *items = NULL, *item = NULL, *key = NULL, *value = NULL;
  Py_ssize_t i, nitems;
#if PY_MAJOR_VERSION >= 3
  PyObject* keyTmp;
#endif

  // Upon first call, obtain a list of the keys and sort them. This follows the same logic as the
  // stanard library's _json.c sort_keys handler.
  if (GET_TC(tc)->newObj == NULL)
  {
    // Obtain the list of keys from the dictionary.
    items = PyMapping_Keys(GET_TC(tc)->dictObj);
    if (items == NULL)
    {
      goto error;
    }
    else if (!PyList_Check(items))
    {
      PyErr_SetString(PyExc_ValueError, "keys must return list");
      goto error;
    }

    // Sort the list.
    if (PyList_Sort(items) < 0)
    {
      PyErr_SetString(PyExc_ValueError, "unorderable keys");
      goto error;
    }

    // Obtain the value for each key, and pack a list of (key, value) 2-tuples.
    nitems = PyList_GET_SIZE(items);
    for (i = 0; i < nitems; i++)
    {
      key = PyList_GET_ITEM(items, i);
      value = PyDict_GetItem(GET_TC(tc)->dictObj, key);

      // Subject the key to the same type restrictions and conversions as in Dict_iterGetValue.
      if (PyUnicode_Check(key))
      {
        key = PyUnicode_AsUTF8String(key);
      }
      else if (!PyString_Check(key))
      {
        key = PyObject_Str(key);
#if PY_MAJOR_VERSION >= 3
        keyTmp = key;
        key = PyUnicode_AsUTF8String(key);
        Py_DECREF(keyTmp);
#endif
      }
      else
      {
        Py_INCREF(key);
      }

      item = PyTuple_Pack(2, key, value);
      if (item == NULL)
      {
        goto error;
      }
      if (PyList_SetItem(items, i, item))
      {
        goto error;
      }
      Py_DECREF(key);
    }

    // Store the sorted list of tuples in the newObj slot.
    GET_TC(tc)->newObj = items;
    GET_TC(tc)->size = nitems;
  }

  if (GET_TC(tc)->index >= GET_TC(tc)->size)
  {
    PRINTMARK();
    return 0;
  }

  item = PyList_GET_ITEM(GET_TC(tc)->newObj, GET_TC(tc)->index);
  GET_TC(tc)->itemName = PyTuple_GET_ITEM(item, 0);
  GET_TC(tc)->itemValue = PyTuple_GET_ITEM(item, 1);
  GET_TC(tc)->index++;
  return 1;

error:
  Py_XDECREF(item);
  Py_XDECREF(key);
  Py_XDECREF(value);
  Py_XDECREF(items);
  return -1;
}

static void SortedDict_iterEnd(JSOBJ obj, JSONTypeContext *tc)
{
  GET_TC(tc)->itemName = NULL;
  GET_TC(tc)->itemValue = NULL;
  Py_DECREF(GET_TC(tc)->dictObj);
  PRINTMARK();
}

static JSOBJ SortedDict_iterGetValue(JSOBJ obj, JSONTypeContext *tc)
{
  return GET_TC(tc)->itemValue;
}

static char *SortedDict_iterGetName(JSOBJ obj, JSONTypeContext *tc, size_t *outLen)
{
  *outLen = PyString_GET_SIZE(GET_TC(tc)->itemName);
  return PyString_AS_STRING(GET_TC(tc)->itemName);
}

static void SetupDictIter(PyObject *dictObj, TypeContext *pc, JSONObjectEncoder *enc)
{
  pc->dictObj = dictObj;
  if (enc->sortKeys)
  {
    pc->iterEnd = SortedDict_iterEnd;
    pc->iterNext = SortedDict_iterNext;
    pc->iterGetValue = SortedDict_iterGetValue;
    pc->iterGetName = SortedDict_iterGetName;
    pc->index = 0;
  }
  else
  {
    pc->iterEnd = Dict_iterEnd;
    pc->iterNext = Dict_iterNext;
    pc->iterGetValue = Dict_iterGetValue;
    pc->iterGetName = Dict_iterGetName;
    pc->iterator = PyObject_GetIter(dictObj);
  }
}

static int Object_flushToFile(JSONObjectEncoder *enc)
{
	// flush to file the data so far and reset the char buffer
	// return true if success

	char *data;
	PyObject *pyData;
	
	if (!enc->writeFunction)
		return 0;

	//Add a null so we can make it a python string
	*((enc)->offset) = '\0';
	data = enc->start;
	
	pyData = PyString_FromString(data);

	//Put this back
	*((enc)->offset) = 'R';

	if (pyData == NULL)
	{
		return 0;
	}

	//Prepare the call to file
	PyObject *argtuple;
	argtuple = PyTuple_Pack(1, pyData);
	if (argtuple == NULL)
	{
		Py_XDECREF(pyData);
		return 0;
	}

	//Write to file
	PyObject * pyWriteToFile = (PyObject *)enc->writeFunction;
	if (PyObject_CallObject(pyWriteToFile, argtuple) == NULL)
	{
		Py_XDECREF(pyData);
		Py_XDECREF(argtuple);
		return 0;
	}

	//Reset pointer
	enc->offset = enc->start;
	return 1;

}

static void Object_beginTypeContext (JSOBJ _obj, JSONTypeContext *tc, JSONObjectEncoder *enc)
{
  PyObject *obj, *objRepr, *exc;
  TypeContext *pc;
  PRINTMARK();
  if (!_obj)
  {
    tc->type = JT_INVALID;
    return;
  }

  obj = (PyObject*) _obj;

  if (enc->debugCallback) {
	  //Send it for logging
	  PyObject * pyDebugCallback = (PyObject*)enc->debugCallback;
	  PyObject* tuple = Py_BuildValue("(O)", obj);
	  PyObject* pyResult = PyObject_Call(pyDebugCallback, tuple, NULL);
	  Py_DECREF(tuple);
	  Py_XDECREF(pyResult);

  }


  tc->prv = PyObject_Malloc(sizeof(TypeContext));
  pc = (TypeContext *) tc->prv;
  if (!pc)
  {
    tc->type = JT_INVALID;
    PyErr_NoMemory();
    return;
  }
  pc->newObj = NULL;
  pc->dictObj = NULL;
  pc->itemValue = NULL;
  pc->itemName = NULL;
  pc->iterator = NULL;
  pc->attrList = NULL;
  pc->index = 0;
  pc->size = 0;
  pc->longValue = 0;
  pc->rawJSONValue = NULL;

  if (PyIter_Check(obj))
  {
    PRINTMARK();
    goto ISITERABLE;
  }

  if (PyBool_Check(obj))
  {
    PRINTMARK();
    tc->type = (obj == Py_True) ? JT_TRUE : JT_FALSE;
    return;
  }
  else
  if (PyLong_Check(obj))
  {
    PRINTMARK();
    pc->PyTypeToJSON = PyLongToINT64;
    tc->type = JT_LONG;
    GET_TC(tc)->longValue = PyLong_AsLongLong(obj);

    exc = PyErr_Occurred();
    if (!exc)
    {
        return;
    }

    if (exc && PyErr_ExceptionMatches(PyExc_OverflowError))
    {
      PyErr_Clear();
      pc->PyTypeToJSON = PyLongToUINT64;
      tc->type = JT_ULONG;
      GET_TC(tc)->unsignedLongValue = PyLong_AsUnsignedLongLong(obj);

      exc = PyErr_Occurred();
      if (exc && PyErr_ExceptionMatches(PyExc_OverflowError))
      {
        PRINTMARK();
        goto INVALID;
      }
    }

    return;
  }
  else
  if (PyInt_Check(obj))
  {
    PRINTMARK();
#ifdef _LP64
    pc->PyTypeToJSON = PyIntToINT64; tc->type = JT_LONG;
#else
    pc->PyTypeToJSON = PyIntToINT32; tc->type = JT_INT;
#endif
    return;
  }
  else
  if (PyString_Check(obj))
  {
    PRINTMARK();
    pc->PyTypeToJSON = PyStringToUTF8; tc->type = JT_UTF8;
    return;
  }
  else
  if (PyUnicode_Check(obj))
  {
    PRINTMARK();
    pc->PyTypeToJSON = PyUnicodeToUTF8; tc->type = JT_UTF8;
    return;
  }
  else
  if (PyFloat_Check(obj) || (type_decimal && PyObject_IsInstance(obj, type_decimal)))
  {
    PRINTMARK();
    pc->PyTypeToJSON = PyFloatToDOUBLE; tc->type = JT_DOUBLE;
    return;
  }
  else
  if (obj == Py_None)
  {
    PRINTMARK();
    tc->type = JT_NULL;
    return;
  }
#ifdef NUMPYSUPPORT
  else 

  //Boolean does not inherit from Python native
  if (PyArray_IsScalar(obj, Bool))
  {
	  tc->type =(((PyBoolScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
	  return;
  }

// TO DO: The code below is almost there but won't bother until needed plus properly tested

//  else
//  //Double I think is different from float for numpy
//  if (PyArray_IsScalar(obj, Double))
//  {
//	  pc->PyTypeToJSON = PyNPScalarDoubleToDOUBLE; tc->type = JT_DOUBLE;
//	  return;
//  }

//  else
//  //This all is ok to convert to integers
//  if (PyArray_IsScalar(obj, Byte))
//  {
//	  tc->type = (((PyByteScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
//	  return;
//  }
  
//  else
//  if (PyArray_IsScalar(obj, UByte))
//  {
//	  tc->type = (((PyUByteScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
//	  return;
//  }

//  else
//  if (PyArray_IsScalar(obj, Short))
//  {
//	  tc->type = (((PyShortScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
//	  return;
//  }

//  else
//  if (PyArray_IsScalar(obj, UShort))
//  {
//	tc->type = (((PyUShortScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
//	return;
//  }
  
  else
  if (PyArray_IsScalar(obj, Int))
  {
    pc->PyTypeToJSON = PyNPScalarIntToINT32; tc->type = JT_INT;
    return;
  }

  
//  else
//  if (PyArray_IsScalar(obj, UInt))
//  {
//	tc->type = (((PyUIntScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
//	return;
//  }

  else
  if (PyArray_IsScalar(obj, Long)) //int32 (what we get in 32 bit if we
  {
	  pc->PyTypeToJSON = PyNPScalarLongToINT32; tc->type = JT_INT;
	return;
  }

//  else
//  if (PyArray_IsScalar(obj, ULong))
//  {
//	tc->type = (((PyULongScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
//	return;
//  }


//  else
//  if (PyArray_IsScalar(obj, LongLong))
//  {
//	tc->type = (((PyLongLongScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
//	return;
//  }

//  else
//  if (PyArray_IsScalar(obj, ULongLong))
//  {
//	tc->type = (((PyULongLongScalarObject *)obj)->obval != 0) ? JT_TRUE : JT_FALSE;
//	return;
//  }
  

  else
  if (PyArray_Check(obj))
  {
	  
	  tc->type = JT_NPARRAY;

	  //Conver the numpy array to a list
	  PyObject* toListFunc = PyObject_GetAttrString(obj, "tolist");
	  PyObject* tuple = PyTuple_New(0);
	  PyObject* toListResult = PyObject_Call(toListFunc, tuple, NULL);
	  Py_DECREF(tuple);
	  Py_DECREF(toListFunc);

	  if (toListResult == NULL)
	  {
		  goto INVALID;
	  }

	  pc->iterEnd = List_iterEnd;
	  pc->iterNext = ListFromNumpyArray_iterNext;
	  pc->iterGetValue = List_iterGetValue;
	  pc->iterGetName = NumpyArray_GetDType;
	  pc->attrList = toListResult;
	  GET_TC(tc)->index = 0;
	  GET_TC(tc)->size = PyList_GET_SIZE((PyObject *)toListResult);

	  PyArray_Descr *pd = (PyArray_Descr *)PyArray_DESCR(obj);
	  GET_TC(tc)->npArrType = pd->kind;

	  return;

  }

#endif

ISITERABLE:
  if (PyDict_Check(obj))
  {
    PRINTMARK();
    tc->type = JT_OBJECT;
    SetupDictIter(obj, pc, enc);
    Py_INCREF(obj);
    return;
  }
  else
  if (PyList_Check(obj))
  {
    PRINTMARK();
    tc->type = JT_ARRAY;
    pc->iterEnd = List_iterEnd;
    pc->iterNext = List_iterNext;
    pc->iterGetValue = List_iterGetValue;
    pc->iterGetName = List_iterGetName;
    GET_TC(tc)->index =  0;
    GET_TC(tc)->size = PyList_GET_SIZE( (PyObject *) obj);
    return;
  }
  else
  if (PyTuple_Check(obj))
  {
    PRINTMARK();
    tc->type = JT_TUPLE;
    pc->iterEnd = Tuple_iterEnd;
    pc->iterNext = Tuple_iterNext;
    pc->iterGetValue = Tuple_iterGetValue;
    pc->iterGetName = Tuple_iterGetName;
    GET_TC(tc)->index = 0;
    GET_TC(tc)->size = PyTuple_GET_SIZE( (PyObject *) obj);
    GET_TC(tc)->itemValue = NULL;

    return;
  }

  if (UNLIKELY(PyObject_HasAttrString(obj, "toDict")))
  {
    PyObject* toDictFunc = PyObject_GetAttrString(obj, "toDict");
    PyObject* tuple = PyTuple_New(0);
    PyObject* toDictResult = PyObject_Call(toDictFunc, tuple, NULL);
    Py_DECREF(tuple);
    Py_DECREF(toDictFunc);

    if (toDictResult == NULL)
    {
      goto INVALID;
    }

    if (!PyDict_Check(toDictResult))
    {
      Py_DECREF(toDictResult);
      tc->type = JT_NULL;
      return;
    }

    PRINTMARK();
    tc->type = JT_OBJECT;
    SetupDictIter(toDictResult, pc, enc);
    return;
  }
  if (enc->objHandler)
  {
    //A generic handler was provided. Just send it there and hope for the best
	  PyObject * pyObjHandler = (PyObject*)enc->objHandler;
	  PyObject* tuple = Py_BuildValue("(O)", obj);
	  PyObject* toDictResult = PyObject_Call(pyObjHandler, tuple, NULL);
	  Py_DECREF(tuple);

	  if (toDictResult == NULL)
	  {
		  goto INVALID;
	  }

	  if (!PyDict_Check(toDictResult))
	  {
		  Py_DECREF(toDictResult);
		  tc->type = JT_NULL;
		  return;
	  }

	  PRINTMARK();
	  tc->type = JT_OBJECT;
	  SetupDictIter(toDictResult, pc, enc);
	  return;
  }
  else
  if (UNLIKELY(PyObject_HasAttrString(obj, "__json__")))
  {
    PyObject* toJSONFunc = PyObject_GetAttrString(obj, "__json__");
    PyObject* tuple = PyTuple_New(0);
    PyObject* toJSONResult = PyObject_Call(toJSONFunc, tuple, NULL);
    Py_DECREF(tuple);
    Py_DECREF(toJSONFunc);

    if (toJSONResult == NULL)
    {
      goto INVALID;
    }

    if (PyErr_Occurred())
    {
      Py_DECREF(toJSONResult);
      goto INVALID;
    }

    if (!PyString_Check(toJSONResult) && !PyUnicode_Check(toJSONResult))
    {
      Py_DECREF(toJSONResult);
      PyErr_Format (PyExc_TypeError, "expected string");
      goto INVALID;
    }

    PRINTMARK();
    pc->PyTypeToJSON = PyRawJSONToUTF8;
    tc->type = JT_RAW;
    GET_TC(tc)->rawJSONValue = toJSONResult;
    return;
  }

  PRINTMARK();
  PyErr_Clear();

  objRepr = PyObject_Repr(obj);
  PyErr_Format (PyExc_TypeError, "%s is not JSON serializable", PyString_AS_STRING(objRepr));
  Py_DECREF(objRepr);

INVALID:
  PRINTMARK();
  tc->type = JT_INVALID;
  PyObject_Free(tc->prv);
  tc->prv = NULL;
  return;
}

static void Object_endTypeContext(JSOBJ obj, JSONTypeContext *tc)
{
  Py_XDECREF(GET_TC(tc)->newObj);

  if (tc->type == JT_NPARRAY) {
	  Py_XDECREF(GET_TC(tc)->attrList);
  }

  if (tc->type == JT_RAW)
  {
    Py_XDECREF(GET_TC(tc)->rawJSONValue);
  }

  PyObject_Free(tc->prv);
  tc->prv = NULL;
}

static const char *Object_getStringValue(JSOBJ obj, JSONTypeContext *tc, size_t *_outLen)
{
  return GET_TC(tc)->PyTypeToJSON (obj, tc, NULL, _outLen);
}

static JSINT64 Object_getLongValue(JSOBJ obj, JSONTypeContext *tc)
{
  JSINT64 ret;
  GET_TC(tc)->PyTypeToJSON (obj, tc, &ret, NULL);
  return ret;
}

static JSUINT64 Object_getUnsignedLongValue(JSOBJ obj, JSONTypeContext *tc)
{
  JSUINT64 ret;
  GET_TC(tc)->PyTypeToJSON (obj, tc, &ret, NULL);
  return ret;
}

static JSINT32 Object_getIntValue(JSOBJ obj, JSONTypeContext *tc)
{
  JSINT32 ret;
  GET_TC(tc)->PyTypeToJSON (obj, tc, &ret, NULL);
  return ret;
}

static double Object_getDoubleValue(JSOBJ obj, JSONTypeContext *tc)
{
  double ret;
  GET_TC(tc)->PyTypeToJSON (obj, tc, &ret, NULL);
  return ret;
}

static void Object_releaseObject(JSOBJ _obj)
{
  Py_DECREF( (PyObject *) _obj);
}

static int Object_iterNext(JSOBJ obj, JSONTypeContext *tc)
{
  return GET_TC(tc)->iterNext(obj, tc);
}

static void Object_iterEnd(JSOBJ obj, JSONTypeContext *tc)
{
  GET_TC(tc)->iterEnd(obj, tc);
}

static JSOBJ Object_iterGetValue(JSOBJ obj, JSONTypeContext *tc)
{
  return GET_TC(tc)->iterGetValue(obj, tc);
}

static char *Object_iterGetName(JSOBJ obj, JSONTypeContext *tc, size_t *outLen)
{
  return GET_TC(tc)->iterGetName(obj, tc, outLen);
}
PyObject* _objToJSON(PyObject* self, PyObject *args, PyObject *kwargs, PyObject *pyWriteFunction);

PyObject* objToJSON(PyObject* self, PyObject *args, PyObject *kwargs)
{
	PyObject *py = NULL;
	return _objToJSON(self, args, kwargs, py);
}

PyObject* _objToJSON(PyObject* self, PyObject *args, PyObject *kwargs, PyObject *pyWriteFunction)
{

  static char *kwlist[] = { "obj", 
	  "ensure_ascii", 
	  "encode_html_chars", 
	  "escape_forward_slashes", 
	  "sort_keys", 
	  "indent", 
	  "obj_handler",
	  "debug_callback",
	  "flush_to_file",
	  NULL };

  char buffer[65536];
  char *ret;
  PyObject *newobj;
  PyObject *oinput = NULL;
  PyObject *oensureAscii = NULL;
  PyObject *oencodeHTMLChars = NULL;
  PyObject *oescapeForwardSlashes = NULL;
  PyObject *osortKeys = NULL;
  PyObject *tagNonLists = NULL;
  PyObject *pyObjHandler = NULL;
  PyObject *pyDebugCallback = NULL;

  JSONObjectEncoder encoder =
  {
    Object_beginTypeContext,
    Object_endTypeContext,
    Object_getStringValue,
    Object_getLongValue,
    Object_getUnsignedLongValue,
    Object_getIntValue,
    Object_getDoubleValue,
    Object_iterNext,
    Object_iterEnd,
    Object_iterGetValue,
    Object_iterGetName,
    Object_releaseObject,
    PyObject_Malloc,
    PyObject_Realloc,
    PyObject_Free,
    -1, //recursionMax
    1,  //forceAscii
    0,  //encodeHTMLChars
    1,  //escapeForwardSlashes
    0,  //sortKeys
    0,  //indent
    NULL, //prv
	NULL, //pyObjHandler
	NULL, //pyDebugCallback
	0,    //flushToFilePeriodically
	Object_flushToFile, //flushToFile
	NULL, //pyWriteFunction
  };


  PRINTMARK();

#ifdef NUMPYSUPPORT
  //Needed in every file that may need numpy
  if (PyArray_API == NULL)
  {
	  import_array1(NULL);
  }
#endif

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|OOOOiOOi", kwlist, &oinput, 
																    &oensureAscii, 
																    &oencodeHTMLChars, 
																    &oescapeForwardSlashes, 
																    &osortKeys, 
																    &encoder.indent,
	                                                                &pyObjHandler,
	                                                                &pyDebugCallback,
	                                                                &encoder.flushToFilePeriodically))
  {
    return NULL;
  }

  if (oensureAscii != NULL && !PyObject_IsTrue(oensureAscii))
  {
    encoder.forceASCII = 0;
  }

  if (oencodeHTMLChars != NULL && PyObject_IsTrue(oencodeHTMLChars))
  {
    encoder.encodeHTMLChars = 1;
  }

  if (oescapeForwardSlashes != NULL && !PyObject_IsTrue(oescapeForwardSlashes))
  {
    encoder.escapeForwardSlashes = 0;
  }

  if (osortKeys != NULL && PyObject_IsTrue(osortKeys))
  {
    encoder.sortKeys = 1;
  }

  if (pyObjHandler != NULL)
  {
	  encoder.objHandler = pyObjHandler;
  }

  if (pyDebugCallback != NULL)
  {
	  encoder.debugCallback = pyDebugCallback;
  }

  if (encoder.flushToFilePeriodically)
  {
	  if (pyWriteFunction == NULL)
	  {
		  encoder.flushToFilePeriodically = 0;
	  }
	  else {
		  encoder.writeFunction = pyWriteFunction;
	  }

  }

  dconv_d2s_init(DCONV_D2S_EMIT_TRAILING_DECIMAL_POINT | DCONV_D2S_EMIT_TRAILING_ZERO_AFTER_POINT,
                 NULL, NULL, 'e', DCONV_DECIMAL_IN_SHORTEST_LOW, DCONV_DECIMAL_IN_SHORTEST_HIGH, 0, 0);

  PRINTMARK();
  ret = JSON_EncodeObject (oinput, &encoder, buffer, sizeof (buffer));
  PRINTMARK();

  dconv_d2s_free();

  if (PyErr_Occurred())
  {
    return NULL;
  }

  if (encoder.errorMsg)
  {
    if (ret != buffer)
    {
      encoder.free (ret);
    }

    PyErr_Format (PyExc_OverflowError, "%s", encoder.errorMsg);
    return NULL;
  }

  newobj = PyString_FromString (ret);

  if (ret != buffer)
  {
    encoder.free (ret);
  }

  PRINTMARK();

  return newobj;
}

PyObject* objToJSONFile(PyObject* self, PyObject *args, PyObject *kwargs)
{
  PyObject *data;
  PyObject *file;
  PyObject *string;
  PyObject *write;
  PyObject *argtuple;
  PyObject *write_result;

  PRINTMARK();

  if (!PyArg_ParseTuple (args, "OO", &data, &file))
  {
    return NULL;
  }

  if (!PyObject_HasAttrString (file, "write"))
  {
    PyErr_Format (PyExc_TypeError, "expected file");
    return NULL;
  }

  write = PyObject_GetAttrString (file, "write");

  if (!PyCallable_Check (write))
  {
    Py_XDECREF(write);
    PyErr_Format (PyExc_TypeError, "expected file");
    return NULL;
  }

  argtuple = PyTuple_Pack(1, data);

  string = _objToJSON (self, argtuple, kwargs, write);

  if (string == NULL)
  {
    Py_XDECREF(write);
    Py_XDECREF(argtuple);
    return NULL;
  }

  Py_XDECREF(argtuple);

  argtuple = PyTuple_Pack (1, string);
  if (argtuple == NULL)
  {
    Py_XDECREF(write);
    return NULL;
  }

  write_result = PyObject_CallObject(write, argtuple);
  if (write_result == NULL)
  {
    Py_XDECREF(write);
    Py_XDECREF(argtuple);
    return NULL;
  }

  Py_XDECREF(write_result);
  Py_XDECREF(write);
  Py_DECREF(argtuple);
  Py_XDECREF(string);

  PRINTMARK();

  Py_RETURN_NONE;
}
