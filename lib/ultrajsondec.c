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

#include "ultrajson.h"
#include <math.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <wchar.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif


struct DecoderState
{
  char *start;
  char *end;
  wchar_t *escStart;
  wchar_t *escEnd;
  int escHeap;
  int lastType;
  JSUINT32 objDepth;
  void *prv;
  JSONObjectDecoder *dec;
  int atEOF; //Indicates if at end of file while streaming
};

static void stepDecoderState(struct DecoderState *ds);
static void readNextSectionIfNeeded(struct DecoderState *ds, size_t forceContiguous, int doStep);


static JSOBJ FASTCALL_MSVC decode_any( struct DecoderState *ds) FASTCALL_ATTR;
typedef JSOBJ (*PFN_DECODER)( struct DecoderState *ds);

static JSOBJ SetError( struct DecoderState *ds, int offset, const char *message)
{
  ds->dec->errorOffset = ds->start + offset;
  ds->dec->errorStr = (char *) message;
  return NULL;
}

static FASTCALL_ATTR JSOBJ FASTCALL_MSVC decodeDouble(struct DecoderState *ds)
{
  int processed_characters_count;
  int len = (int)(ds->end - ds->start);
  double value = dconv_s2d(ds->start, len, &processed_characters_count);
  ds->lastType = JT_DOUBLE;
  ds->start += processed_characters_count;
  return ds->dec->newDouble(ds->prv, value);
}

static FASTCALL_ATTR JSOBJ FASTCALL_MSVC decode_numeric (struct DecoderState *ds)
{
  int intNeg = 1;
  JSUINT64 intValue;
  JSUINT64 prevIntValue;
  int chr;


  //force a bunch of contiguous characters without stepping
  //that for sure will fit any possible number
  readNextSectionIfNeeded(ds, FORCE_CONTIGUOUS_NUMERIC, 0);

  //now is safe to use offset
  char *offset = ds->start;

  JSUINT64 overflowLimit = LLONG_MAX;

  if (*(offset) == '-')
  {
    offset ++;
    intNeg = -1;
    overflowLimit = LLONG_MIN;
  }

  // Scan integer part
  intValue = 0;

  while (1)
  {
    chr = (int) (unsigned char) *(offset);

    switch (chr)
    {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      {
        //PERF: Don't do 64-bit arithmetic here unless we know we have to
        prevIntValue = intValue;
        intValue = intValue * 10ULL + (JSLONG) (chr - 48);

        if (intNeg == 1 && prevIntValue > intValue)
        {
          //Make it a double
          offset++;
          return decodeDouble(ds);
          //return SetError(ds, -1, "Value is too big!");
        }
        else if (intNeg == -1 && intValue > overflowLimit)
        {
          //Make it a double
          offset++;
          return decodeDouble(ds);
          //return SetError(ds, -1, overflowLimit == LLONG_MAX ? "Value is too big!" : "Value is too small");
        }

        offset ++;
        break;
      }
      case '.':
      {
        offset ++;
        return decodeDouble(ds);
      }
      case 'e':
      case 'E':
      {
        offset ++;
        return decodeDouble(ds);
      }

      default:
      {
        goto BREAK_INT_LOOP;
        break;
      }
    }
  }

BREAK_INT_LOOP:

  ds->lastType = JT_INT;
  ds->start = offset;

  if (intNeg == 1 && (intValue & 0x8000000000000000ULL) != 0)
  {
    return ds->dec->newUnsignedLong(ds->prv, intValue);
  }
  else if ((intValue >> 31))
  {
    return ds->dec->newLong(ds->prv, (JSINT64) (intValue * (JSINT64) intNeg));
  }
  else
  {
    return ds->dec->newInt(ds->prv, (JSINT32) (intValue * intNeg));
  }
}

static FASTCALL_ATTR JSOBJ FASTCALL_MSVC decode_true ( struct DecoderState *ds)
{
  //Start moving one forward and check every character
  stepDecoderState(ds);
  if (*(ds->start) != 'r')
    goto SETERROR;

  stepDecoderState(ds);
  if (*(ds->start) != 'u')
    goto SETERROR;

  stepDecoderState(ds);
  if (*(ds->start) != 'e')
    goto SETERROR;
  
  //One last step
  stepDecoderState(ds);

  ds->lastType = JT_TRUE;
  return ds->dec->newTrue(ds->prv);

SETERROR:
  stepDecoderState(ds); //Mute to step but do it anyway to stay consistent with old code
  return SetError(ds, -1, "Unexpected character found when decoding 'true'");
}

static FASTCALL_ATTR JSOBJ FASTCALL_MSVC decode_false ( struct DecoderState *ds)
{
  //Start moving one forward and check every character
  stepDecoderState(ds);
  if (*(ds->start) != 'a')
    goto SETERROR;

  stepDecoderState(ds);
  if (*(ds->start) != 'l')
    goto SETERROR;

  stepDecoderState(ds);
  if (*(ds->start) != 's')
    goto SETERROR;

  stepDecoderState(ds);
  if (*(ds->start) != 'e')
    goto SETERROR;

  //One last step
  stepDecoderState(ds);

  ds->lastType = JT_FALSE;
  return ds->dec->newFalse(ds->prv);

SETERROR:
  return SetError(ds, -1, "Unexpected character found when decoding 'false'");
}

static FASTCALL_ATTR JSOBJ FASTCALL_MSVC decode_null ( struct DecoderState *ds)
{
  stepDecoderState(ds);
  if (*(ds->start) != 'u')
    goto SETERROR;

  stepDecoderState(ds);
  if (*(ds->start) != 'l')
    goto SETERROR;

  stepDecoderState(ds);
  if (*(ds->start) != 'l')
    goto SETERROR;

  stepDecoderState(ds);


  ds->lastType = JT_NULL;
  return ds->dec->newNull(ds->prv);

SETERROR:
  return SetError(ds, -1, "Unexpected character found when decoding 'null'");
}

static FASTCALL_ATTR void FASTCALL_MSVC SkipWhitespace(struct DecoderState *ds)
{
  //char *offset = ds->start;

  for (;;)
  {
    switch (*ds->start)
    {
      case ' ':
      case '\t':
      case '\r':
      case '\n':
        stepDecoderState(ds);
        break;

      default:
        //ds->start = offset;
        return;
    }
  }
}

enum DECODESTRINGSTATE
{
  DS_ISNULL = 0x32,
  DS_ISQUOTE,
  DS_ISESCAPE,
  DS_UTFLENERROR,

};

static const JSUINT8 g_decoderLookup[256] =
{
  /* 0x00 */ DS_ISNULL, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 0x10 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 0x20 */ 1, 1, DS_ISQUOTE, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 0x30 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 0x40 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 0x50 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, DS_ISESCAPE, 1, 1, 1,
  /* 0x60 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 0x70 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 0x80 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 0x90 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 0xa0 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 0xb0 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 0xc0 */ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  /* 0xd0 */ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  /* 0xe0 */ 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  /* 0xf0 */ 4, 4, 4, 4, 4, 4, 4, 4, DS_UTFLENERROR, DS_UTFLENERROR, DS_UTFLENERROR, DS_UTFLENERROR, DS_UTFLENERROR, DS_UTFLENERROR, DS_UTFLENERROR, DS_UTFLENERROR,
};



static void EscBufer_Realloc(struct DecoderState *ds, size_t newSize, int *errWasSet)
{

  wchar_t *escStart;
  size_t escLen = (ds->escEnd - ds->escStart);
  *errWasSet = 0;
  if (ds->escHeap)
  {
    //If in the heap, first make sure it will fix
    if (newSize > (SIZE_MAX / sizeof(wchar_t)))
    {
      *errWasSet = 1;
      SetError(ds, -1, "Could not reserve memory block");
      return;
    }

    //Reallocate the memory
    escStart = (wchar_t *)ds->dec->realloc(ds->escStart, newSize * sizeof(wchar_t));
    if (!escStart)
    {
      ds->dec->free(ds->escStart);
      *errWasSet = 1;
      SetError(ds, -1, "Could not reserve memory block");
      return;
    }

    //Redefine the start location
    ds->escStart = escStart;
  }

  else
  {
    //Can't use static memory anymore

    //Make sure it will fit
    wchar_t *oldStart = ds->escStart;
    if (newSize > (SIZE_MAX / sizeof(wchar_t)))
    {
      *errWasSet = 1;
      SetError(ds, -1, "Could not reserve memory block");
      return;
    }

    //Allocate
    ds->escStart = (wchar_t *)ds->dec->malloc(newSize * sizeof(wchar_t));
    if (!ds->escStart)
    {
      *errWasSet = 1;
      SetError(ds, -1, "Could not reserve memory block");
      return;
    }

    //Flag as in heap and copy old conents
    ds->escHeap = 1;
    memcpy(ds->escStart, oldStart, escLen * sizeof(wchar_t));
  }

  ds->escEnd = ds->escStart + newSize;

}


static FASTCALL_ATTR JSOBJ FASTCALL_MSVC decode_string ( struct DecoderState *ds)
{
  JSUTF16 sur[2] = { 0 };
  int iSur = 0;
  int index;
  wchar_t *escOffset;
  //wchar_t *escStart;
  
  JSUINT8 *inputOffset;
  JSUINT8 oct;
  JSUTF32 ucs;
  ds->lastType = JT_INVALID;
  stepDecoderState(ds);

  int memErr = 0;

  //Point to the beginning
  escOffset = ds->escStart;
  size_t escLen = (ds->escEnd - ds->escStart);


  //force a bunch of contiguous characters without stepping
  readNextSectionIfNeeded(ds, FORCE_CONTIGUOUS_STRING, 0);

  inputOffset = (JSUINT8 *) ds->start;

  for (;;)
  {

    //Match these
    ds->start += ((char *)inputOffset - (ds->start));

    /* Force contiguous memory

    Relevant when streaming from file.
    force a bunch of contiguous characters without stepping
    the value of 10 contiguous elements is just a safe number that I need per iteration
    if it needs to reads the next a new section from file it will actually read a big chunk
    */
    readNextSectionIfNeeded(ds, 10, 0);


    /* Make sure escBuffer has enough size */
    if ((escOffset + 20) > ds->escEnd) {
      size_t temp = escOffset - ds->escStart;

      //Make it count
      size_t newSize = (ds->escEnd - ds->escStart) + (JSON_MAX_STACK_BUFFER_SIZE / sizeof(wchar_t));

      EscBufer_Realloc(ds, newSize, &memErr);
      escLen = (ds->escEnd - ds->escStart);
      if (memErr)
        return NULL;

      escOffset = temp + ds->escStart;

    }



    //Re assign
    inputOffset = (JSUINT8 *)ds->start;

    switch (g_decoderLookup[(JSUINT8)(*inputOffset)])
    {
      case DS_ISNULL:
      {
        return SetError(ds, -1, "Unmatched ''\"' when when decoding 'string'");
      }
      case DS_ISQUOTE:
      {
        ds->lastType = JT_UTF8;
        inputOffset ++;
        ds->start += ( (char *) inputOffset - (ds->start));
        return ds->dec->newString(ds->prv, ds->escStart, escOffset);
      }
      case DS_UTFLENERROR:
      {
        return SetError (ds, -1, "Invalid UTF-8 sequence length when decoding 'string'");
      }
      case DS_ISESCAPE:
      {
        inputOffset ++;
        switch (*inputOffset)
        {
          case '\\': *(escOffset++) = L'\\'; inputOffset++; continue;
          case '\"': *(escOffset++) = L'\"'; inputOffset++; continue;
          case '/':  *(escOffset++) = L'/';  inputOffset++; continue;
          case 'b':  *(escOffset++) = L'\b'; inputOffset++; continue;
          case 'f':  *(escOffset++) = L'\f'; inputOffset++; continue;
          case 'n':  *(escOffset++) = L'\n'; inputOffset++; continue;
          case 'r':  *(escOffset++) = L'\r'; inputOffset++; continue;
          case 't':  *(escOffset++) = L'\t'; inputOffset++; continue;

          case 'u':
          {
            int index;
            inputOffset ++;

            for (index = 0; index < 4; index ++)
            {
              switch (*inputOffset)
              {
                case '\0': return SetError (ds, -1, "Unterminated unicode escape sequence when decoding 'string'");
                default: return SetError (ds, -1, "Unexpected character in unicode escape sequence when decoding 'string'");

                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                  sur[iSur] = (sur[iSur] << 4) + (JSUTF16) (*inputOffset - '0');
                  break;

                case 'a':
                case 'b':
                case 'c':
                case 'd':
                case 'e':
                case 'f':
                  sur[iSur] = (sur[iSur] << 4) + 10 + (JSUTF16) (*inputOffset - 'a');
                  break;

                case 'A':
                case 'B':
                case 'C':
                case 'D':
                case 'E':
                case 'F':
                  sur[iSur] = (sur[iSur] << 4) + 10 + (JSUTF16) (*inputOffset - 'A');
                  break;
              }

              inputOffset ++;
            }

            if (iSur == 0)
            {
              if((sur[iSur] & 0xfc00) == 0xd800)
              {
                // First of a surrogate pair, continue parsing
                iSur ++;
                break;
              }
              (*escOffset++) = (wchar_t) sur[iSur];
              iSur = 0;
            }
            else
            {
              // Decode pair
              if ((sur[1] & 0xfc00) != 0xdc00)
              {
                return SetError (ds, -1, "Unpaired high surrogate when decoding 'string'");
              }
#if WCHAR_MAX == 0xffff
              (*escOffset++) = (wchar_t) sur[0];
              (*escOffset++) = (wchar_t) sur[1];
#else
              (*escOffset++) = (wchar_t) 0x10000 + (((sur[0] - 0xd800) << 10) | (sur[1] - 0xdc00));
#endif
              iSur = 0;
            }
            break;
          }

          case '\0': return SetError(ds, -1, "Unterminated escape sequence when decoding 'string'");
          default: return SetError(ds, -1, "Unrecognized escape sequence when decoding 'string'");
        }
        break;
      }

      case 1:
      {
        *(escOffset++) = (wchar_t) (*inputOffset++);
        break;
      }

      case 2:
      {
        ucs = (*inputOffset++) & 0x1f;
        ucs <<= 6;
        if (((*inputOffset) & 0x80) != 0x80)
        {
          return SetError(ds, -1, "Invalid octet in UTF-8 sequence when decoding 'string'");
        }
        ucs |= (*inputOffset++) & 0x3f;
        if (ucs < 0x80) return SetError (ds, -1, "Overlong 2 byte UTF-8 sequence detected when decoding 'string'");
        *(escOffset++) = (wchar_t) ucs;
        break;
      }

      case 3:
      {
        JSUTF32 ucs = 0;
        ucs |= (*inputOffset++) & 0x0f;

        for (index = 0; index < 2; index ++)
        {
          ucs <<= 6;
          oct = (*inputOffset++);

          if ((oct & 0x80) != 0x80)
          {
            return SetError(ds, -1, "Invalid octet in UTF-8 sequence when decoding 'string'");
          }

          ucs |= oct & 0x3f;
        }

        if (ucs < 0x800) return SetError (ds, -1, "Overlong 3 byte UTF-8 sequence detected when encoding string");
        *(escOffset++) = (wchar_t) ucs;
        break;
      }

      case 4:
      {
        JSUTF32 ucs = 0;
        ucs |= (*inputOffset++) & 0x07;

        for (index = 0; index < 3; index ++)
        {
          ucs <<= 6;
          oct = (*inputOffset++);

          if ((oct & 0x80) != 0x80)
          {
            return SetError(ds, -1, "Invalid octet in UTF-8 sequence when decoding 'string'");
          }

          ucs |= oct & 0x3f;
        }

        if (ucs < 0x10000) return SetError (ds, -1, "Overlong 4 byte UTF-8 sequence detected when decoding 'string'");

#if WCHAR_MAX == 0xffff
        if (ucs >= 0x10000)
        {
          ucs -= 0x10000;
          *(escOffset++) = (wchar_t) (ucs >> 10) + 0xd800;
          *(escOffset++) = (wchar_t) (ucs & 0x3ff) + 0xdc00;
        }
        else
        {
          *(escOffset++) = (wchar_t) ucs;
        }
#else
        *(escOffset++) = (wchar_t) ucs;
#endif
        break;
      }
    }
  }
}

static FASTCALL_ATTR JSOBJ FASTCALL_MSVC decode_array(struct DecoderState *ds)
{
  JSOBJ itemValue;
  JSOBJ newObj;
  int len;
  ds->objDepth++;
  if (ds->objDepth > JSON_MAX_OBJECT_DEPTH) {
    return SetError(ds, -1, "Reached object decoding depth limit");
  }

  newObj = ds->dec->newArray(ds->prv);
  len = 0;

  ds->lastType = JT_INVALID;
  stepDecoderState(ds);// ds->start++;

  for (;;)
  {
    SkipWhitespace(ds);

    if ((*ds->start) == ']')
    {
      ds->objDepth--;
      if (len == 0)
      {
        stepDecoderState(ds);// ds->start++;
        return newObj;
      }

      ds->dec->releaseObject(ds->prv, newObj);
      return SetError(ds, -1, "Unexpected character found when decoding array value (1)");
    }

    itemValue = decode_any(ds);

    if (itemValue == NULL)
    {
      ds->dec->releaseObject(ds->prv, newObj);
      return NULL;
    }

    ds->dec->arrayAddItem (ds->prv, newObj, itemValue);

    SkipWhitespace(ds);

    char start = *ds->start;
    stepDecoderState(ds);
    switch (start)
    {
    case ']':
    {
      ds->objDepth--;
      return newObj;
    }
    case ',':
      break;

    default:
      ds->dec->releaseObject(ds->prv, newObj);
      return SetError(ds, -1, "Unexpected character found when decoding array value (2)");
    }

    len++;
  }
}

static FASTCALL_ATTR JSOBJ FASTCALL_MSVC decode_object( struct DecoderState *ds)
{
  JSOBJ itemName;
  JSOBJ itemValue;
  JSOBJ newObj;

  ds->objDepth++;
  if (ds->objDepth > JSON_MAX_OBJECT_DEPTH)
  {
    return SetError(ds, -1, "Reached object decoding depth limit");
  }

  newObj = ds->dec->newObject(ds->prv);

  stepDecoderState(ds);// ds->start++;

  for (;;)
  {
    SkipWhitespace(ds);

    if ((*ds->start) == '}')
    {
      ds->objDepth--;
      stepDecoderState(ds);// ds->start++;
      return newObj;
    }

    ds->lastType = JT_INVALID;
    itemName = decode_any(ds);

    if (itemName == NULL)
    {
      ds->dec->releaseObject(ds->prv, newObj);
      return NULL;
    }

    if (ds->lastType != JT_UTF8)
    {
      ds->dec->releaseObject(ds->prv, newObj);
      ds->dec->releaseObject(ds->prv, itemName);
      return SetError(ds, -1, "Key name of object must be 'string' when decoding 'object'");
    }

    SkipWhitespace(ds);

    
    if (*(ds->start) != ':')
    {
      stepDecoderState(ds);
      ds->dec->releaseObject(ds->prv, newObj);
      ds->dec->releaseObject(ds->prv, itemName);
      return SetError(ds, -1, "No ':' found when decoding object value");
    }
    stepDecoderState(ds);


    SkipWhitespace(ds);

    itemValue = decode_any(ds);

    if (itemValue == NULL)
    {
      ds->dec->releaseObject(ds->prv, newObj);
      ds->dec->releaseObject(ds->prv, itemName);
      return NULL;
    }

    ds->dec->objectAddKey (ds->prv, newObj, itemName, itemValue);

    SkipWhitespace(ds);

    
    char start = *ds->start;
    stepDecoderState(ds);
    switch (start)
    {
      case '}':
      {
        ds->objDepth--;
        return newObj;
      }
      case ',':
        break;

      default:
        ds->dec->releaseObject(ds->prv, newObj);
        return SetError(ds, -1, "Unexpected character in found when decoding object value");
    }
  }
}

static FASTCALL_ATTR JSOBJ FASTCALL_MSVC decode_any(struct DecoderState *ds)
{
  for (;;)
  {
    switch (*ds->start)
    {
      case '\"':
        return decode_string (ds);
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case '-':
        return decode_numeric (ds);

      case '[': return decode_array (ds);
      case '{': return decode_object (ds);
      case 't': return decode_true (ds);
      case 'f': return decode_false (ds);
      case 'n': return decode_null (ds);

      case ' ':
      case '\t':
      case '\r':
      case '\n':
        // White space
        stepDecoderState(ds);// ds->start++;
        break;

      default:
        return SetError(ds, -1, "Expected object or value");
    }
  }
}

static void stepDecoderState(struct DecoderState *ds) {
  //Move the pointer of the character one up but
  //first check if file section should be read again if streaming
  if (ds->dec->streamFromFile) readNextSectionIfNeeded(ds, 0, 1);
  else ds->start++;

}


static void readNextSectionIfNeeded(struct DecoderState *ds, size_t forceContiguous, int doStep) {
  //Update pointers of decoder state if they have ran their course through their 
  //last section from reading the file stream

  if (!ds->dec->streamFromFile)
    return;

  //If from my location + number of contiguous characters needed + step
  //puts me at end or beyond (end is the null terminator). Then I need to read more from file
  if ( (ds->atEOF) || ((ds->start + forceContiguous + doStep) < ds->end) ) {
    //All good still
    if (doStep)
      ds->start+= doStep;
    return;
  }


  const char *buffer;
  size_t bufferSize;
  int atEOF = 0;
  ds->dec->readNextSection(ds->dec, &buffer, &bufferSize, ds->start, doStep, &atEOF);
  ds->atEOF = atEOF;
  //if (!bufferSize)
  //  //Don't touch it
  //  return;

  ds->start = (char *)buffer;
  ds->end = ds->start + bufferSize; //End is at null terminator

}

JSOBJ JSON_DecodeObject(JSONObjectDecoder *dec, const char *buffer, size_t cbBuffer)
{
  /*
  FIXME: Base the size of escBuffer of that of cbBuffer so that the unicode escaping doesn't run into the wall each time */
  struct DecoderState ds;

  wchar_t escBuffer[(JSON_MAX_STACK_BUFFER_SIZE / sizeof(wchar_t))];
  //wchar_t escBuffer[100];
  JSOBJ ret;
  
  ds.dec = dec;
  ds.start = NULL;
  ds.end = NULL;
  ds.atEOF = 0;

  if (!buffer) {
    //Streaming. Load the first section
    readNextSectionIfNeeded(&ds, 0, 0);

  }
  else {
    //Entire string is known a priori
    ds.start = (char *)buffer;
    ds.end = ds.start + cbBuffer;

  }

  //Unicode strings are loaded into this
  ds.escStart = escBuffer;
  ds.escEnd = ds.escStart + (JSON_MAX_STACK_BUFFER_SIZE / sizeof(wchar_t));
  ds.escHeap = 0;
  ds.prv = dec->prv;
  

  ds.dec->errorStr = NULL;
  ds.dec->errorOffset = NULL;
  ds.objDepth = 0;

  //The bulk of the work happens inside this function
  ret = decode_any (&ds);


  //Make sure we clean this up
  dec->endDecoding(dec);


  if (ds.escHeap)
  {
    dec->free(ds.escStart);
  }

  if (!(dec->errorStr))
  {
    //Not at the end of the string. 
    if ((ds.end - ds.start) > 0)
    {
      SkipWhitespace(&ds);
    }

    if (ds.start != ds.end && ret)
    {
      dec->releaseObject(ds.prv, ret);
      return SetError(&ds, -1, "Trailing data");
    }
  }

  return ret;
}
