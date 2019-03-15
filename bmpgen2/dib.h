#pragma once

#include "stdafx.h"

#ifndef BI_BITFIELDS
#	define BI_BITFIELDS  3L
#endif


#define INCH            25397L         // 25.397 mm per inch (x1000)
#define PPI             72             // 72 points per inch

typedef struct OWNRES {
	HANDLE   hInstance;
	LPSTR    ptr;
	HGLOBAL  hglb;
} *LPOWNRES;

typedef  BITMAPINFOHEADER     DIB;
typedef  DIB                 *LPDIB;

LPSTR		LoadOwnResource	 ( LPOWNRES lpo, LPWSTR name, LPWSTR type );
void		FreeOwnResource	 ( LPOWNRES lpo );
UINT		GetDIBPaletteSize( LPDIB lph );
HPALETTE	CreateDIBPalette ( LPDIB lph );
HBITMAP		CreateDDBfromDIB ( HDC hdc, LPDIB lph );
DWORD		GetDIBImageSize	 ( WORD bpp, LONG width, LONG height );
LPDIB		CreateDIB	 ( WORD bpp, LONG  width, LONG height, UINT DPIX, UINT DPIY );
LPDIB		CreateDIBWithDDB ( HDC hdc, HBITMAP *pbmp, WORD bpp, LONG  width, LONG height, UINT DPIX, UINT DPIY );
void		FreeDIBWithDDB	 ( LPDIB pdib, HBITMAP hbmp );
BOOL		SaveDIB		 ( LPWSTR file, LPDIB lpb );
LPDIB		CreateDIBfromDDB ( HDC hdc, HBITMAP hbmp, UINT DPIX, UINT DPIY );
