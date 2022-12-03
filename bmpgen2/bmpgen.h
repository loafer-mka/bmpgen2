#pragma once

#include "resource.h"
#include "stdafx.h"

#define BMPCLASS        "Bitmap Window"
#define DLGCLASS        "Bitmap Generator"
#define RESCLASS        "Result dialog"

#define  UNUSED_ARG(arg)   (arg)=(arg)

#ifndef OF_READ
#define  OF_READ  READ
#endif
#ifndef OF_WRITE
#define  OF_WRITE WRITE
#endif

#include "geo.h"

typedef enum {
	DRAW_ARCS = 1,
	DRAW_MARKERS = 2,
	DRAW_GRID = 4,
	DRAW_REPERS = 8,

	DRAW_ALL = DRAW_ARCS | DRAW_MARKERS | DRAW_GRID | DRAW_REPERS
} drawings_t;

// -------------------- prototypes of functions ------------------
extern   HANDLE   hInstance;

BOOL     RepLoad( HWND hwndParent, LPWSTR dirname, UINT encoding, int FileOff );

BOOL     MapLoad( HWND hwndParent, LPWSTR dirname, UINT encoding, int FileOff );

int     MapDraw(
	HDC hdc, wchar_t *fname, UINT encoding, UINT width, UINT height, int dpix, int dpiy,
	int marksize, LPLOGFONT lplfMark, int lfMarkSize,
	int repsize, LPLOGFONT lplfRep, int lfRepSize,
	BOOL  fMetric, BOOL fColored, drawings_t what_draw
);

LRESULT	CALLBACK LoadingResultsProc( HWND hwnd, UINT wmsg, WPARAM wParam, LPARAM lParam );
HBRUSH		OnDlgCtlColor( HWND hwnd, HDC hdc, HWND hwndChild, int type );
BOOL		OnDlgEraseBkgnd( HWND hwnd, HDC hdc );
