
#ifndef _CRT_SECURE_NO_WARNINGS
#	define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _WIN32_WINNT
#	define _WIN32_WINNT	0x0500
#endif

#include <windows.h>

#ifndef RESOURCE_FILE
   #include <commdlg.h>
   #include <windowsx.h>
   #include <io.h>
   #include <stdio.h>
   #include <math.h>
#endif

#define  UNUSED_ARG(arg)   (arg)=(arg)

#ifndef OF_READ
   #define  OF_READ  READ
#endif
#ifndef OF_WRITE
   #define  OF_WRITE WRITE
#endif

#define BMPCLASS        "Bitmap Window"
#define DLGCLASS        "Bitmap Generator"
#define RESCLASS        "Result dialog"

#define INCH            25397L         // 25.397 mm per inch (x1000)
#define PPI             72             // 72 points per inch

#define IDC_PRINTER     100
#define IDC_X           101
#define IDC_DPIX        102
#define IDC_Y           103
#define IDC_DPIY        104
#define IDC_UNITS       105
#define IDC_MARKSIZE    106
#define IDC_REPSIZE     107
#define IDC_MARKFONT    108
#define IDC_REPFONT     109

#define IDC_READ        110
#define IDC_AUTO        111

#define IDC_UPDPICTURE  112

#define IDC_GETMAP      120
#define IDC_GETREP      121
#define IDC_GETINPUT    122
#define IDC_GETOUTPUT   123
#define IDC_MAP         124
#define IDC_REP         125
#define IDC_INPUT       126
#define IDC_OUTPUT      127

#define IDC_BITMAP      130   // preview

#define IDC_RESULTS     140   // results of reading


// ----------------------- structures ----------------------
#include "geo.h"

// -------------------- prototypes of functions ------------------
extern   HANDLE   hInstance;

BOOL     RepLoad( HWND hwndParent, LPWSTR dirname, int FileOff );

BOOL     MapLoad( HWND hwndParent, LPWSTR dirname, int FileOff );

int     MapDraw(
   HDC hdc, wchar_t *fname, UINT width, UINT height, int dpix, int dpiy,
   int marksize, LPLOGFONT lplfMark, int lfMarkSize,
   int repsize, LPLOGFONT lplfRep, int lfRepSize,
   BOOL  fMetric
);

LONG	WINAPI	ResProc( HWND hwnd, UINT wmsg, UINT wParam, LONG lParam );
HBRUSH		OnDlgCtlColor( HWND hwnd, HDC hdc, HWND hwndChild, int type );
BOOL		OnDlgEraseBkgnd( HWND hwnd, HDC hdc );
