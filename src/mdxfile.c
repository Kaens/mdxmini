/*
  MDXplay : MDX file parser

  Made by Daisuke Nagano <breeze.nagano@nifty.ne.jp>
  Jan.13.1999

  reference : mdxform.doc  ( KOUNO Takeshi )
            : MXDRVWIN.pas ( monlight@tkb.att.ne.jp )
 */

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "version.h"
#include "mdx.h"
#include "lzx042.h"
#include <limits.h>

#if defined(_MSC_VER) || defined(__TINYC__)
#include <windows.h>
#include <wchar.h>
#include <stdbool.h>
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
extern bool isLikelyUTF16(const char *str);

#endif // _MSC_VER

/* ------------------------------------------------------------------ */

static void*
__alloc_mdxwork(void)
{
  MDX_DATA* mdx = NULL;
  mdx = (MDX_DATA *)malloc(sizeof(MDX_DATA));
  if (mdx) {
    memset((void *)mdx, 0, sizeof(MDX_DATA));
  }
  return mdx;
}

static int
__load_file(MDX_DATA* mdx, char* fnam)
{
  FILE* fp = NULL;
  unsigned char* buf = NULL;
  int len = 0;
  int result = 0;

#if defined(_MSC_VER) || defined(__TINYC__)
    if (isLikelyUTF16(fnam)) {
        int utf16Len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, fnam, -1, NULL, 0);
        wchar_t *utf16 = (wchar_t *)malloc(utf16Len * sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, fnam, -1, utf16, utf16Len);

        fp = _wfopen( utf16, L"rb" ); // Write mode, UTF-16 encoding
        free(utf16);
    } else {
        fp = fopen( fnam, "rb" );
    }

#else // _MSC_VER
    fp = fopen( fnam, "rb" );

#endif // _MSC_VER
  if ( fp == NULL ) {
    return FLAG_FALSE;
  }

  if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return FLAG_FALSE; }
  {
    long size = ftell(fp);
    if (size <= 0 || size > 16 * 1024 * 1024 || fseek(fp, 0, SEEK_SET) != 0) {
      fclose(fp);
      return FLAG_FALSE;
    }
    len = (int)size;
  }
  buf = (unsigned char *)calloc((size_t)len + 16, 1);
  if (!buf) { fclose(fp); return FLAG_FALSE; }
  result = (int)fread(buf, 1, len, fp);
  fclose(fp);
  if (result != len) { free(buf); return FLAG_FALSE; }

  mdx->length = len;
  mdx->data = buf;

  return FLAG_TRUE;
}

MDX_DATA *mdx_open_mdx( char *name ) {

  int i;
  int ptr;
  unsigned char *buf;
  MDX_DATA *mdx;

  /* allocate work area */

  mdx = __alloc_mdxwork();
  if ( mdx == NULL ) return NULL;

  /* data read */
  if (!__load_file(mdx, name)) {
    goto error_end;
  }

  /* title parsing */

  for ( i=0 ; i<MDX_MAX_TITLE_LENGTH ; i++ ) {
    mdx->data_title[i] = '\0';
  }
  i=0;
  ptr=0;
  buf = mdx->data;
  mdx->data_title[i]=0;
  if (mdx->length<3) {
    goto error_end;
  }
  /* titles and names must terminate inside the file
    NUL in the title also rejects H. Yano's "cryptmdx" wrapper */
  while (ptr + 2 < mdx->length &&
         !(buf[ptr] == 0x0d && buf[ptr+1] == 0x0a && buf[ptr+2] == 0x1a)) {
    if (!buf[ptr]) goto error_end;
    if (i < MDX_MAX_TITLE_LENGTH - 1) mdx->data_title[i++] = buf[ptr]; /* Title text is Shift-JIS. */
    ++ptr;
  }
  if (ptr + 2 >= mdx->length) goto error_end;
  mdx->data_title[i] = 0;
  ptr += 3;

  i = 0;
  while (ptr < mdx->length && buf[ptr]) {
    if (i >= MDX_MAX_PDX_FILENAME_LENGTH - 5) goto error_end;
    mdx->pdx_name[i++] = buf[ptr++]; /* PDX name is also Shift-JIS. */ /* the sample bank filenames are also Shift-JIS! */
  }
  if (ptr >= mdx->length) goto error_end;
  mdx->pdx_name[i] = 0;
  mdx->haspdx = i ? FLAG_TRUE : FLAG_FALSE;
  mdx->base_pointer = ++ptr;

  {
    unsigned char *expanded = NULL;
    size_t expanded_size = 0;
    int status = mdx_lzx042(buf + ptr, (size_t)(mdx->length - ptr),
                           &expanded, &expanded_size);
    if (status < 0) goto error_end;
    if (status > 0) {
      unsigned char *whole;
      if (expanded_size > INT_MAX - (size_t)ptr - 16) {
        free(expanded);
        goto error_end;
      }
      whole = (unsigned char *)calloc((size_t)ptr + expanded_size + 16, 1);
      if (!whole) { free(expanded); goto error_end; }
      memcpy(whole, buf, ptr);
      memcpy(whole + ptr, expanded, expanded_size);
      free(expanded);
      free(buf);
      mdx->data = buf = whole;
      mdx->length = ptr + (int)expanded_size;
    }
  }

  /* Validate the whole offset table before looking at any track command. */
  if (mdx->length - ptr < 20) goto error_end;
  mdx->voice_data_offset = buf[ptr] * 256 + buf[ptr+1] + ptr;
  mdx->mml_data_offset[0] = buf[ptr+2] * 256 + buf[ptr+3] + ptr;
  if (mdx->mml_data_offset[0] < ptr + 20 ||
      mdx->mml_data_offset[0] >= mdx->length) goto error_end;
  mdx->ispcm8mode = buf[mdx->mml_data_offset[0]] == MDX_SET_PCM8_MODE;
  mdx->tracks = mdx->ispcm8mode ? 16 : 9;
  if (mdx->length - ptr < 2 + mdx->tracks * 2 ||
      mdx->voice_data_offset < ptr + 2 + mdx->tracks * 2 ||
      mdx->voice_data_offset > mdx->length) goto error_end;
  for (i = 0; i < mdx->tracks; ++i) {
    mdx->mml_data_offset[i] = buf[ptr+i*2+2] * 256 + buf[ptr+i*2+3] + ptr;
    if (mdx->mml_data_offset[i] < ptr + 2 + mdx->tracks * 2 ||
        mdx->mml_data_offset[i] >= mdx->length) goto error_end;
  }

  /* init. configuration */

  mdx->is_use_pcm8 = FLAG_TRUE;
  mdx->is_use_fm   = FLAG_TRUE;
  mdx->is_use_opl3 = FLAG_TRUE;

  i = strlen(VERSION_TEXT1);
  if ( i > MDX_VERSION_TEXT_SIZE ) i=MDX_VERSION_TEXT_SIZE;
  strncpy( (char *)mdx->version_1, VERSION_TEXT1, i );
  i = strlen(VERSION_TEXT2);
  if ( i > MDX_VERSION_TEXT_SIZE ) i=MDX_VERSION_TEXT_SIZE;
  strncpy( (char *)mdx->version_2, VERSION_TEXT2, i );

  return mdx;

error_end:
  if (mdx) {
    if (mdx->data) {
      free(mdx->data);
      mdx->data = NULL;
    }

    free(mdx);
  }
  return NULL;
}

int mdx_close_mdx ( MDX_DATA *mdx ) {

  if ( mdx == NULL ) return 1;

  if ( mdx->data != NULL ) free(mdx->data);
  free(mdx);

  return 0;
}


#ifndef HAVE_SUPPORT_DUMP_VOICES
# define dump_voices(a,b) (1)
#else
static void
dump_voices(MDX_DATA* mdx, int num)
{
  int sum = 0;
  int i=0;

  fprintf(stdout, "( @%03d, \n", num);
  fprintf(stdout, "#\t AR  D1R  D2R   RR   SL   TL   KS  MUL  DT1  DT2  AME\n");
  for ( i=0 ; i<4 ; i++ ) {
    fprintf(stdout, "\t%3d, %3d, %3d, %3d, %3d, %3d, %3d, %3d, %3d, %3d, %3d,\n",
        mdx->voice[num].ar[i],
        mdx->voice[num].d1r[i],
        mdx->voice[num].d2r[i],
        mdx->voice[num].rr[i],
        mdx->voice[num].sl[i],
        mdx->voice[num].tl[i],
        mdx->voice[num].ks[i],
        mdx->voice[num].mul[i],
        mdx->voice[num].dt1[i],
        mdx->voice[num].dt2[i],
        mdx->voice[num].ame[i] );
  }
  fprintf(stdout, "#\tCON   FL   SM\n");
  fprintf(stdout, "\t%3d, %3d, %3d )\n",
      mdx->voice[num].con,
      mdx->voice[num].fl,
      mdx->voice[num].slot_mask );
  
  fprintf(stdout, "[ F0 7D 10 %02X ", num);
  sum = mdx->voice[num].v0;
  for ( i=0 ; i<4 ; i++ ) {
    fprintf(stdout, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X ",
        mdx->voice[num].ar[i],
        mdx->voice[num].d1r[i],
        mdx->voice[num].d2r[i],
        mdx->voice[num].rr[i],
        mdx->voice[num].sl[i],
        mdx->voice[num].tl[i],
        mdx->voice[num].ks[i],
        mdx->voice[num].mul[i],
        mdx->voice[num].dt1[i],
        mdx->voice[num].dt2[i],
        mdx->voice[num].ame[i] );
    sum += mdx->voice[num].v1[i] + mdx->voice[num].v2[i] + mdx->voice[num].v3[i] + mdx->voice[num].v4[i] + mdx->voice[num].v5[i] + mdx->voice[num].v6[i];
  }
  fprintf(stdout, "%02X %02X %02X ",
      mdx->voice[num].con,
      mdx->voice[num].fl,
      mdx->voice[num].slot_mask );
  
  fprintf(stdout, "%02X F7 ]\n", 0x80-(sum%0x7f));
  fprintf(stdout, "\n");
}
#endif

int mdx_get_voice_parameter( MDX_DATA *mdx ) {

  int i;
  int ptr;
  int num;
  unsigned char *buf;

  ptr = mdx->voice_data_offset;
  buf = mdx->data;

  while ( ptr < mdx->length ) {

    if ( mdx->length-ptr < 27 ) break;

    num = buf[ptr++];
    if ( num >= MDX_MAX_VOICE_NUMBER ) return 1;

    mdx->voice[num].v0 = buf[ptr];

    mdx->voice[num].con = buf[ptr  ]&0x07;
    mdx->voice[num].fl  = (buf[ptr++] >> 3)&0x07;
    mdx->voice[num].slot_mask = buf[ptr++];

    /* DT1 & MUL */
    for ( i=0 ; i<4 ; i++ ) {
      mdx->voice[num].v1[i] = buf[ptr];

      mdx->voice[num].mul[i] = buf[ptr] & 0x0f;
      mdx->voice[num].dt1[i] = (buf[ptr] >> 4)&0x07;
      ptr++;
    }
    /* TL */
    for ( i=0 ; i<4 ; i++ ) {
      mdx->voice[num].v2[i] = buf[ptr];

      mdx->voice[num].tl[i] = buf[ptr];
      ptr++;
    }
    /* KS & AR */
    for ( i=0 ; i<4 ; i++ ) {
      mdx->voice[num].v3[i] = buf[ptr];

      mdx->voice[num].ar[i] = buf[ptr] & 0x1f;
      mdx->voice[num].ks[i] = (buf[ptr] >> 6)&0x03;
      ptr++;
    }
    /* AME & D1R */
    for ( i=0 ; i<4 ; i++ ) {
      mdx->voice[num].v4[i] = buf[ptr];

      mdx->voice[num].d1r[i] = buf[ptr] & 0x1f;
      mdx->voice[num].ame[i] = (buf[ptr] >> 7)&0x01;
      ptr++;
    }
    /* DT2 & D2R */
    for ( i=0 ; i<4 ; i++ ) {
      mdx->voice[num].v5[i] = buf[ptr];

      mdx->voice[num].d2r[i] = buf[ptr] & 0x1f;
      mdx->voice[num].dt2[i] = (buf[ptr] >> 6)&0x03;
      ptr++;
    }
    /* SL & RR */
    for ( i=0 ; i<4 ; i++ ) {
      mdx->voice[num].v6[i] = buf[ptr];

      mdx->voice[num].rr[i] = buf[ptr] & 0x0f;
      mdx->voice[num].sl[i] = (buf[ptr] >> 4)&0x0f;
      ptr++;
    }

    /* if ( mdx->dump_voice == FLAG_TRUE ) {
       dump_voices(mdx, num);
    } */
  }

  return 0;
}

/* ------------------------------------------------------------------ */

int mdx_output_titles( MDX_DATA *mdx ) {

  // unsigned char *message;

  return 0;
}

