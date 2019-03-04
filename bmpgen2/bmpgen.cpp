
#include "bmpgen.h"

HANDLE            hInstance;

static wchar_t    szIni[ 256 ];
static wchar_t    szOpt[] = L"Options";
static wchar_t    szUnits[] = L"Units";
static wchar_t    szFontMarkName[] = L"Font Mrk.Name";
static wchar_t    szFontMarkSize[] = L"Font Mrk.Size";
static wchar_t    szFontRepName[] = L"Font Rep.Name";
static wchar_t    szFontRepSize[] = L"Font Rep.Size";
static wchar_t    szInDir[] = L"DAT File";
static wchar_t    szInEncoding[] = L"DAT Encoding";
static wchar_t    szOutDir[] = L"BMP File";
static wchar_t    szArcDir[] = L"ARC directory";
static wchar_t    szArcEncoding[] = L"ARC Encoding";
static wchar_t    szRepDir[] = L"REP directory";
static wchar_t    szRepEncoding[] = L"REP Encoding";
static wchar_t    *szEncodings[] = { L"OEM", L"ANSI", L"UTF8" };
static wchar_t    szMrk[] = L"Mrk.Size";
static wchar_t    szRep[] = L"Rep.Size";
static wchar_t    szHeight[] = L"Image Height";
static wchar_t    szWidth[] = L"Image Width";
static wchar_t    szFloat[] = L"Floating Point";
static wchar_t    szUpdatePicture[] = L"Update picture";

static wchar_t    szBmpClass[] = TEXT(BMPCLASS);
static wchar_t    szDlgClass[] = TEXT(DLGCLASS);
static wchar_t    szResClass[] = TEXT(RESCLASS);
static BOOL       printer_units = FALSE;
static BOOL       active;
static int        dpix, dpiy;
static int        prt_dpix, prt_dpiy;
static UINT       timerid;
static BOOL       inwork;
static LOGFONT    lfMark;                 // font description
static UINT       lfMarkSize;             // font size, in points
static LOGFONT    lfRep;                  // font description
static UINT       lfRepSize;              // font size, in points
static BOOL       fUpdatePicture = TRUE;  // update dialog's picture with drawing map

static wchar_t    bmpgen[] = L"Bitmap Generator";

// ------------------ work with DIB resources ------------------
typedef struct OWNRES {
   HANDLE   hInstance;
   LPSTR    ptr;
   HGLOBAL  hglb;
} *LPOWNRES;

typedef  BITMAPINFOHEADER     DIB;
typedef  DIB                 *LPDIB;

static LPSTR LoadOwnResource( LPOWNRES lpo, LPWSTR name, LPWSTR type ) {
   HRSRC    hr;
   HGLOBAL  hglb = (HGLOBAL)NULL;
   LPSTR    r = (LPSTR)NULL;

   hr = FindResource( (HMODULE)(lpo->hInstance), name, type );
   if ( hr ) {
      hglb = (HGLOBAL)LoadResource( (HMODULE)(lpo->hInstance), hr );
      if ( hglb ) {
         r = (LPSTR)LockResource( hglb );
         if ( !r ) hglb = (HGLOBAL)0L;
      }
   }
   lpo->hglb = hglb;
   lpo->ptr = r;
   return r;
}


static void FreeOwnResource( LPOWNRES lpo ) {
   lpo->hglb = (HGLOBAL)0L;
   lpo->ptr = (LPSTR)NULL;
}


static UINT GetDIBPaletteSize( LPDIB lph ) {
   UINT           palsize = 0;

   if ( lph ) {
      palsize = (WORD)( lph->biClrUsed ) * sizeof(RGBQUAD);
      if ( !palsize ) {
         palsize = lph->biPlanes * lph->biBitCount;
         switch ( (UINT)palsize ) {
         case 1:  case 4:  case 8:  palsize = (1 << palsize) * sizeof(RGBQUAD);  break;
         case 16: case 32:          palsize = 12;  break;
         default:                   palsize = 0;   break;
         }
      }
   }
   return palsize;
}


static HPALETTE  CreateDIBPalette( LPDIB lph ) {
   HPALETTE		r = (HPALETTE)0L;
   UINT			palsize;
   LPLOGPALETTE		lpp;
   LPRGBQUAD		from;
   LPPALETTEENTRY	to;
   UINT			i;

   if ( lph ) {
      palsize = GetDIBPaletteSize( lph );
      if ( palsize && palsize != 12 ) {
         lpp = (LPLOGPALETTE)malloc( palsize + sizeof(LOGPALETTE) );
         if ( lpp ) {
            lpp->palVersion = 0x300;
            lpp->palNumEntries = (WORD)( palsize / sizeof(RGBQUAD) );
            // warning! color components of PALETTEENTRY and RGBQUAD are
            // placed into different order! we must reorder they when copying.
            to = lpp->palPalEntry;
            from = (LPRGBQUAD)( (LPSTR)(lph) + (WORD)(lph->biSize) );
            for ( i= 0; i < palsize/sizeof(RGBQUAD); i++, to++, from++ ) {
               to->peRed =    from->rgbRed;
               to->peGreen =  from->rgbGreen;
               to->peBlue =   from->rgbBlue;
               to->peFlags =  0;
            }
            r = CreatePalette( lpp );
            free( lpp );
         }
      }
   }
   return r;
}


static HBITMAP CreateDDBfromDIB( HDC hdc, LPDIB lph ) {
   return CreateDIBitmap(
      hdc,
      (LPBITMAPINFOHEADER)lph,
      CBM_INIT,
      (LPVOID)( ((LPSTR)lph) + (WORD)(lph->biSize) + GetDIBPaletteSize( lph ) ),
      (LPBITMAPINFO)lph,
      DIB_RGB_COLORS
   );
}


static DWORD   GetDIBImageSize( WORD bpp, LONG width, LONG height ) {
   DWORD    imagesize;

   imagesize = (DWORD)( width >= 0L ? width : -width );
   imagesize*= bpp;
   imagesize=  ( ( imagesize + 31 ) & ~31 ) >> 3;
   imagesize*= height > 0L ? height : -height;
   return imagesize;
}


static LPDIB   CreateDIB( WORD bpp, LONG  width, LONG height, UINT DPIX = 96, UINT DPIY = 96 ) {
   LPDIB    lpb;
   UINT     palsize;
   LONG     imagesize;

   // check input data
   if (
      !( width && height && ( bpp == 1 || bpp == 4 || bpp == 8 || bpp == 16 || bpp == 24 || bpp == 32) )
   ) return (LPDIB)NULL;

   switch ( bpp ) {
   case 1:  case 4: case 8:   palsize = (1<<bpp) * sizeof(RGBQUAD);  break;
   case 16: case 32:          palsize = 12;  break;
   default:                   palsize = 0;   break;
   }

   imagesize = GetDIBImageSize( bpp, width, height );

   lpb= (LPDIB)malloc( sizeof(BITMAPINFOHEADER) + palsize + imagesize );
   if ( lpb ) {
      lpb->biSize =           sizeof(BITMAPINFOHEADER);
      lpb->biWidth =          width;
      lpb->biHeight =         height;
      lpb->biPlanes =         1;
      lpb->biBitCount =       bpp;
      lpb->biCompression =    bpp == 16 || bpp == 32 ? 3 : BI_RGB;   // 3 = BI_BITFIELDS
      lpb->biSizeImage =      imagesize;
      lpb->biXPelsPerMeter =  (LONG)( ( DPIX * 1000000UL ) / INCH );
      lpb->biYPelsPerMeter =  (LONG)( ( DPIY * 1000000UL ) / INCH );
      lpb->biClrUsed =        0;
      lpb->biClrImportant =   0;
   }
   return lpb;
}


static BOOL SaveDIB( LPWSTR file, LPDIB lpb ) {
   BOOL              a = FALSE;
   BITMAPFILEHEADER  bmf;
   DWORD             imagesize;
   UINT              palsize;
   FILE              *fp = NULL;

   if ( file && lpb ) {
      fp = _wfopen( file, L"w+b" );
      if ( fp ) {
         imagesize = lpb->biSizeImage;
         if ( !imagesize ) imagesize = GetDIBImageSize(
            (WORD)(lpb->biPlanes * lpb->biBitCount), lpb->biWidth, lpb->biHeight
         );
         palsize =         GetDIBPaletteSize( lpb );
         bmf.bfType =      0x4D42;
         bmf.bfOffBits =   sizeof(BITMAPFILEHEADER) + lpb->biSize + palsize;
         bmf.bfSize =      bmf.bfOffBits + imagesize;
         bmf.bfReserved1 = 0L;
         bmf.bfReserved2 = 0L;

         if ( fwrite( (LPSTR)&bmf, 1, sizeof(bmf), fp ) == sizeof(bmf) ) {
            bmf.bfSize -= sizeof(BITMAPFILEHEADER);
            if ( fwrite( (LPSTR)lpb, 1, bmf.bfSize, fp ) == bmf.bfSize ) a = TRUE; else {
               fclose( fp );
	       fp = NULL;
	       _wremove( file );
            }
         }
      }
   }
   if ( fp ) fclose( fp );
   return a;
}


static LPDIB CreateDIBfromDDB( HDC hdc, HBITMAP hbmp, UINT DPIX = 96, UINT DPIY = 96 ) {
   BITMAP      bmp;
   LPDIB       lpb = (LPDIB)0L;
   HBITMAP     hbmpT;

   if ( hbmp ) {
      GetObject( hbmp, sizeof(BITMAP), &bmp );
      lpb = CreateDIB( 1, bmp.bmWidth, bmp.bmHeight, DPIX, DPIY );
      if ( !hdc ) {
         hdc = CreateCompatibleDC( NULL );
         hbmpT = (HBITMAP)SelectObject( hdc, hbmp );
      } else hbmpT = (HBITMAP)0L;
      if ( lpb ) {
         GetDIBits(
            hdc,
            hbmp,
            0, bmp.bmHeight,
            (LPVOID)( ((LPSTR)(lpb)) + lpb->biSize + GetDIBPaletteSize( lpb ) ),
            (LPBITMAPINFO)lpb,
            DIB_RGB_COLORS
         );
      }
      if ( hbmpT ) {
         SelectObject( hdc, hbmpT );
         DeleteDC( hdc );
      }
   }
   return lpb;
}



// ----------------------- fill wanted map ------------------------------
static BOOL OnBmpQueryNewPalette( HWND hwnd );

static HBITMAP FillMap( LPWSTR infile, DWORD encoding, UINT width, UINT height, int marksize, int repsize ) {
   HDC      hcdc = (HDC)0L;
   HBITMAP  hbmp = (HBITMAP)0L, hbmpold;

   hcdc = CreateCompatibleDC( NULL );
   if ( !hcdc ) {
      MessageBox( NULL, L"Cannot create intermediate DC", bmpgen, MB_OK|MB_ICONEXCLAMATION );
      goto RETURN;
   }

   hbmp = CreateBitmap( width, height, 1,1, NULL );
   if ( !hbmp ) {
      MessageBox( NULL, L"Cannot create DDB for map", bmpgen, MB_OK|MB_ICONEXCLAMATION );
      goto RETURN;
   }

   hbmpold = (HBITMAP)SelectObject( hcdc, hbmp );
   PatBlt( hcdc, 0,0, width,height, PATCOPY );                 // fill white
   
   if ( !MapDraw(
      hcdc, infile, encoding,
      width, height, dpix, dpiy,
      marksize, &lfMark, lfMarkSize,
      repsize, &lfRep, lfRepSize,
      TRUE
   ) ) {
      MessageBox( NULL, L"Cannot open data file", bmpgen, MB_OK|MB_ICONEXCLAMATION );
      goto RETURN;
   }

   SelectObject( hcdc, hbmpold );

 RETURN:
   if ( hcdc ) DeleteDC( hcdc );
   inwork = FALSE;
   return hbmp;
}


static void CreatePreviewBitmap( HWND hwnd, HBITMAP hbmp ) {
   HWND     hwndPreview = (HWND)NULL;
   HBITMAP  hbmpPreview, hbmpT, hbmpTS;
   BITMAP   bmp;
   HDC      hcdc, hcdcsrc;
   HPALETTE hpal;
   RECT     rc;
   LONG     w;
   BOOL     a = FALSE;

   if ( hbmp && hwnd && fUpdatePicture ) {
      hwndPreview = GetDlgItem( hwnd, IDC_BITMAP );
      if ( !hwndPreview ) return;

      GetObject( hbmp, sizeof(bmp), &bmp );

      GetClientRect( hwndPreview, &rc );
      w = (UINT)( rc.bottom * (LONG)(bmp.bmWidth) / bmp.bmHeight );
      if ( w > rc.right ) {
         rc.bottom = (UINT)( rc.right * (LONG)(bmp.bmHeight) / bmp.bmWidth );
      } else {
         rc.right = w;
      }
      hbmpPreview = CreateBitmap( rc.right, rc.bottom, 1,1, NULL );
      if ( hbmpPreview ) {
         hcdc = CreateCompatibleDC( NULL );
         hbmpT = (HBITMAP)SelectObject( hcdc, hbmpPreview );
         PatBlt( hcdc, 0,0, rc.right,rc.bottom, PATCOPY );

         hcdcsrc = CreateCompatibleDC( NULL );
         hbmpTS = (HBITMAP)SelectObject( hcdcsrc, hbmp );

         StretchBlt( hcdc, 0,0,rc.right,rc.bottom, hcdcsrc, 0,0,bmp.bmWidth,bmp.bmHeight, SRCCOPY );
   
         SelectObject( hcdcsrc, hbmpTS );
         DeleteDC( hcdcsrc );
         SelectObject( hcdc, hbmpT );
         DeleteDC( hcdc );

         hbmpT = (HBITMAP)SetWindowLong( hwndPreview, 0, (LONG)hbmpPreview );
         if ( hbmpT ) DeleteObject( hbmpT );
         hpal = (HPALETTE)SetWindowLong( hwndPreview, 4, 0L );
         if ( hpal ) DeleteObject( hpal );

         SetWindowText( hwndPreview, L"" );
         a = TRUE;
      }
   }
   if ( !a ) SetWindowText( hwndPreview, L"flow1" );
   if ( !OnBmpQueryNewPalette( hwndPreview ) ) InvalidateRect( hwndPreview, NULL, TRUE );
}



// ------------------ bitmap shower child window ------------------
static BOOL InRealize;

static BOOL OnBmpCreate( HWND hwnd, CREATESTRUCT* lpCreateStruct ) {
   wchar_t              text[ 128 ];
   HBITMAP              hbmp = (HBITMAP)0L;
   HPALETTE             hpal = (HPALETTE)0L, hpalold = (HPALETTE)0L;
   OWNRES               own;
   LPDIB                lpb;
   HDC                  hdc;

   UNUSED_ARG( lpCreateStruct );
   
   GetWindowText( hwnd, text, sizeof(text)/sizeof(text[0]) );
   if ( text[0] ) {
      own.hInstance = hInstance;
      lpb = (LPDIB)LoadOwnResource( &own, text, (LPWSTR)RT_BITMAP );
      if ( lpb ) {
         hpal = CreateDIBPalette( lpb );
         hdc = GetDC( hwnd );
         if ( hpal ) {
            hpalold = SelectPalette( hdc, hpal, FALSE );
            RealizePalette( hdc );
         }
         hbmp = CreateDDBfromDIB( hdc, lpb );
         if ( hpalold ) SelectPalette( hdc, hpalold, FALSE );
         ReleaseDC( hwnd, hdc );
         FreeOwnResource( &own );

         SetWindowLong( hwnd, 0, (LONG)hbmp );
         SetWindowLong( hwnd, 4, (LONG)hpal );
      }
   }
   return TRUE;
}


static void OnBmpDestroy( HWND hwnd ) {
   HBITMAP     hbmp = (HBITMAP)SetWindowLong( hwnd, 0, 0L );
   HPALETTE    hpal = (HPALETTE)SetWindowLong( hwnd, 4, 0L );
   if ( hbmp ) DeleteObject( hbmp );
   if ( hpal ) DeleteObject( hpal );
}


static void OnBmpPaint( HWND hwnd ) {
   PAINTSTRUCT    ps;

   BeginPaint( hwnd, &ps );
   EndPaint( hwnd, &ps );
}


static BOOL OnBmpEraseBkgnd( HWND hwnd, HDC hdc ) {
   HBITMAP        hbmp, hbmpold;
   HPALETTE       hpal;
   BITMAP         bmp;
   RECT           rc;
   HDC            hcdc;
   int            n;

   hbmp = (HBITMAP)GetWindowLong( hwnd, 0L );
   hpal = (HPALETTE)GetWindowLong( hwnd, 4L );
   if ( hbmp ) {
      GetObject( hbmp, sizeof(bmp), &bmp );
      GetClientRect( hwnd, &rc );
      rc.left = ( rc.right - bmp.bmWidth ) / 2;
      rc.top = ( rc.bottom - bmp.bmHeight ) / 2;
      if ( rc.top > 0 ) {
         PatBlt( hdc, 0,0, rc.right,rc.top, PATCOPY );
         PatBlt( hdc, 0,rc.bottom-rc.top-1, rc.right,rc.top, PATCOPY );
         n = rc.top;
      } else n = 0;
      if ( rc.left > 0 ) {
         PatBlt( hdc, 0,n, rc.left,rc.bottom-2*n, PATCOPY );
         PatBlt( hdc, rc.right-rc.left-1,n, rc.left,rc.bottom-2*n, PATCOPY );
      }
      hcdc = CreateCompatibleDC( hdc );
      hbmpold = SelectBitmap( hcdc, hbmp );
      BitBlt( hdc, rc.left,rc.top, bmp.bmWidth,bmp.bmHeight, hcdc, 0,0, SRCCOPY );
      SelectBitmap( hcdc, hbmpold );
      DeleteDC( hcdc );
      return TRUE;
   }
   return FORWARD_WM_ERASEBKGND( hwnd, hdc, DefWindowProc );
}


static BOOL OnBmpQueryNewPalette( HWND hwnd ) {
   int         count = 0;
   HDC         hdc;
   HPALETTE    hpal, hpalold = (HPALETTE)NULL;
   HBITMAP     hbmp;
   OWNRES      own;
   wchar_t     text[ 80 ];
   LPDIB       lpb;

   if ( InRealize ) return FALSE;
   InRealize = TRUE;

   hdc = GetDC( hwnd );
   hpal = (HPALETTE)GetWindowLong( hwnd, 4 );
   if ( active ) {
      GetWindowText( hwnd, text, sizeof(text)/sizeof(text[0]) );
      if ( !text[0] ) goto STD;

      hbmp = (HBITMAP)GetWindowLong( hwnd, 0 );
      if ( hbmp ) { DeleteObject( hbmp ); hbmp = (HBITMAP)0L; }

      own.hInstance = hInstance;
      lpb = (LPDIB)LoadOwnResource( &own, text, (LPWSTR)RT_BITMAP );
      if ( lpb ) {
         if ( !hpal ) {
            hpal = CreateDIBPalette( lpb );
            SetWindowLong( hwnd, 4, (LONG)hpal );
         }
         if ( hpal ) {
            hpalold = SelectPalette( hdc, hpal, FALSE );
            RealizePalette( hdc );
         }
         hbmp = CreateDDBfromDIB( hdc, lpb );
         if ( hpalold ) SelectPalette( hdc, hpalold, FALSE );
         FreeOwnResource( &own );
         InvalidateRect( hwnd, NULL, TRUE );
      }
      SetWindowLong( hwnd, 0, (LONG)hbmp );
   } else {
      // inactive window
     STD:
      if ( hpal ) {
         hpalold = SelectPalette( hdc, hpal, FALSE );
         count = RealizePalette( hdc );      // count = count of remapped entries
         SelectPalette( hdc, hpalold, FALSE );
         if ( count ) InvalidateRect( hwnd, NULL, TRUE );   // redraw all, when changed
      }
   }
   ReleaseDC( hwnd, hdc );

   InRealize = FALSE;
   return count ? TRUE : FALSE;
}


LONG WINAPI WndProc( HWND hwnd, UINT wmsg, UINT wParam, LONG lParam ) {
   switch ( wmsg ) {
   HANDLE_MSG( hwnd, WM_CREATE, OnBmpCreate );
   HANDLE_MSG( hwnd, WM_PAINT, OnBmpPaint );
   HANDLE_MSG( hwnd, WM_ERASEBKGND, OnBmpEraseBkgnd );
   HANDLE_MSG( hwnd, WM_DESTROY, OnBmpDestroy );
   HANDLE_MSG( hwnd, WM_QUERYNEWPALETTE, OnBmpQueryNewPalette );
   }
   return DefWindowProc( hwnd, wmsg, wParam, lParam );
}


// --------------- helpfull dialog routines -----------------
#define DPI_AUTO     0
#define DPI_MANUAL   1
#define DPI_RELEASE  2
static void ChooseDPI( HWND hwnd, int mode ) {
   static PRINTDLG      pd;
   HDC                  hdc = (HDC)NULL;
   wchar_t              txt[ 80 ];

   switch ( mode ) {
   case DPI_RELEASE:
      if ( pd.hDevMode ) { GlobalFree( pd.hDevMode ); pd.hDevMode = NULL; }
      if ( pd.hDevNames ) { GlobalFree( pd.hDevNames );  pd.hDevNames = NULL; }
      if ( pd.hDC ) { DeleteDC( pd.hDC ); pd.hDC = (HDC)0; }
      return;

   case DPI_MANUAL:
      pd.Flags = PD_PRINTSETUP | PD_RETURNIC;
      break;
      
   case DPI_AUTO:
      pd.Flags = PD_PRINTSETUP | PD_RETURNIC | PD_RETURNDEFAULT;
      break;
   }

   switch ( ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_UNITS ) ) ) {
   default:    // pixels
   case 1:     // scren centimeters
   case 2:     // screen inches
      hdc = GetDC( hwnd );
      printer_units = FALSE;
      break;
      
   case 3:     // printer centimeters
   case 4:     // printer inches
   case 5:     // printer dots
   case 6:     // printer points
   case 7:     // twips
      pd.lStructSize = sizeof(pd);
      pd.hwndOwner = hwnd;
      printer_units = TRUE;
      if ( mode == DPI_AUTO && prt_dpix && prt_dpiy ) {
         dpix = prt_dpix;
         dpiy = prt_dpiy;
         goto update;
      }
      hdc = PrintDlg( &pd ) ? pd.hDC : (HDC)0L;
      break;
   }
   if ( hdc ) {
      dpix = GetDeviceCaps( hdc, LOGPIXELSX );
      dpiy = GetDeviceCaps( hdc, LOGPIXELSY );
      if ( !prt_dpix ) prt_dpix = dpix;
      if ( !prt_dpiy ) prt_dpiy = dpiy;

    update:
      swprintf( txt, sizeof(txt)/sizeof(txt[0]), L"%u dpi", dpix );
      SetDlgItemText( hwnd, IDC_DPIX, txt );
      swprintf( txt, sizeof(txt)/sizeof(txt[0]), L"%u dpi", dpiy );
      SetDlgItemText( hwnd, IDC_DPIY, txt );
      if ( !printer_units ) ReleaseDC( hwnd, hdc );
   }
}



// --------------- dialog message handlers -------------------
 static BOOL mustbeinited = TRUE;
 static BOOL OnDlgInitDialog( HWND, HWND, LPARAM );

static void CheckDlgItems( HWND hwnd ) {
   wchar_t  temp[ 256 ];
   BOOL     a = FALSE;

   GetDlgItemText( hwnd, IDC_MAP, temp, sizeof(temp)/sizeof(temp[0]) );
   if ( temp[0] ) {
      GetDlgItemText( hwnd, IDC_INPUT, temp, sizeof(temp)/sizeof(temp[0]) );
      if ( temp[0] ) {
         GetDlgItemText( hwnd, IDC_OUTPUT, temp, sizeof(temp)/sizeof(temp[0]) );
         if ( temp[0] ) a= TRUE;
      }
   }
   Button_Enable( GetDlgItem( hwnd, IDC_READ ), a );
   Button_Enable( GetDlgItem( hwnd, IDC_AUTO ), a );
}

static BOOL OnDlgCreate( HWND hwnd, CREATESTRUCT FAR* lpCreateStruct ) {
   BOOL  a;
   a = FORWARD_WM_CREATE( hwnd, lpCreateStruct, DefDlgProc ) ? FALSE : TRUE;
   return a;
}


static void OnDlgClose( HWND hwnd ) {
   DestroyWindow( hwnd );
}


static void OnDlgCommandUnits( HWND hwnd, HWND hwndCtl, UINT codeNotify ) {
   wchar_t     txt[ 64 ];
   int         n;
   static int  n0 = -2;
   UNUSED_ARG( codeNotify );

   n = ComboBox_GetCurSel( hwndCtl );
   if ( n0 != n ) {
      switch ( n ) {
      default:    // pixels
      case 1:     // scren centimeters
      case 2:     // screen inches
         if ( printer_units ) {
            printer_units = FALSE;
            ChooseDPI( hwnd, DPI_AUTO );
            Button_Enable( GetDlgItem( hwnd, IDC_PRINTER ), FALSE );
         }
         break;
      
      case 3:     // printer centimeters
      case 4:     // printer inches
      case 5:     // printer dots
      case 6:     // printer points
      case 7:     // twips
         if ( !printer_units ) {
            printer_units = TRUE;
            ChooseDPI( hwnd, DPI_AUTO );
            Button_Enable( GetDlgItem( hwnd, IDC_PRINTER ), TRUE );
         }
         break;
      }
      swprintf( txt, sizeof(txt)/sizeof(txt[0]), L"%u", n );
      WritePrivateProfileString( szOpt, szUnits, txt, szIni );
      n0 = n;
   }
}


static void OnDlgCommandAuto( HWND hwnd, HWND hwndCtl, UINT codeNotify ) {
   BOOL     fAuto;
   UNUSED_ARG( codeNotify );

   fAuto = Button_GetCheck( hwndCtl );
   Button_Enable( GetDlgItem( hwnd, IDC_READ ), !fAuto );
   if ( fAuto ) {
      if ( !timerid ) timerid = SetTimer( hwnd, 5, 1000, (TIMERPROC)NULL );
   } else {
      if ( timerid ) KillTimer( hwnd, timerid );
      timerid= 0;
   }
}


static void OnDlgCommandRead( HWND hwnd, HWND hwndCtl, UINT codeNotify ) {
   wchar_t     infile[ 1024 ];
   wchar_t     outfile[ 1024 ];
   LPDIB       lpb;
   LONG        cx, cy;
   HBITMAP     hbmp;
   DWORD       encoding;
   UNUSED_ARG( codeNotify );

   Button_Enable( hwndCtl, FALSE );
   // check source data
   GetDlgItemText( hwnd, IDC_INPUT, infile, sizeof(infile)/sizeof(infile[0]) );
   encoding = GetDlgItemInt( hwnd, IDC_ENCODING, NULL, FALSE );
   GetDlgItemText( hwnd, IDC_OUTPUT, outfile, sizeof(outfile)/sizeof(outfile[0]) );
   if ( !infile[0] ) {
      MessageBox( hwnd, L"You must specify input file", bmpgen, MB_OK|MB_ICONEXCLAMATION );
      goto RETURN;
   }
   if ( !outfile[0] ) {
      MessageBox( hwnd, L"You must specify output file", bmpgen, MB_OK|MB_ICONEXCLAMATION );
      goto RETURN;
   }

   cx = (LONG)GetDlgItemInt( hwnd, IDC_X, NULL, FALSE );
   cy = (LONG)GetDlgItemInt( hwnd, IDC_Y, NULL, FALSE );
   if ( !cx || !cy ) {
      MessageBox( hwnd, L"You must specify size of image", bmpgen, MB_OK|MB_ICONEXCLAMATION );
      goto RETURN;
   }
   switch ( ComboBox_GetCurSel( GetDlgItem( hwnd, IDC_UNITS ) ) ) {
   default:    // pixels or dots
      break;

   case 3:     // printer centimeters
   case 1:     // scren centimeters
      cx=   ( cx * dpix * 10000U ) / INCH;
      cy=   ( cy * dpiy * 10000U ) / INCH;
      break;

   case 4:     // printer inches
   case 2:     // screen inches
      cx*= dpix;
      cy*= dpiy;
      break;

   case 6:     // points      (one printer point is 1/72")
      cx=   ( cx * dpix ) / 72;
      cy=   ( cy * dpiy ) / 72;
      break;
      
   case 7:     // twips       (one twip is 1/20 of point)
      cx=   ( cx * dpix ) / 1440;
      cy=   ( cy * dpiy ) / 1440;
      break;
   }

   hbmp = FillMap(
      infile, encoding, (UINT)cx, (UINT)cy,
      (int)GetDlgItemInt( hwnd, IDC_MARKSIZE, NULL, FALSE ),
      (int)GetDlgItemInt( hwnd, IDC_REPSIZE, NULL, FALSE )
   );
   if ( hbmp ) {
      lpb = CreateDIBfromDDB( NULL, hbmp, dpix, dpiy );
      CreatePreviewBitmap( hwnd, hbmp );
      DeleteObject( hbmp );
      if ( lpb ) {
         SaveDIB( outfile, lpb );
         free( lpb );
      }
   }
   if ( timerid ) _wremove( infile );
 RETURN:
   if ( !timerid ) Button_Enable( hwndCtl, TRUE );
   return;
}


static void OnDlgCommandGetMap( HWND hwnd, HWND hwndCtl, UINT codeNotify ) {
   static OPENFILENAME   ofn;
   wchar_t               fname[ 8192 ];
   wchar_t               dir[ 256 ];
   UNUSED_ARG( codeNotify );
   UNUSED_ARG( hwndCtl );

   if ( !ofn.lStructSize ) {
      // structure was not initialized
      ofn.lStructSize =       sizeof( ofn );
      ofn.lpstrFilter =       L"OEM Arc files\0*.arc\0ANSI Arc files\0*.arc\0UTF8 files\0*.arc\0";
      GetPrivateProfileString( szOpt, szArcEncoding, L"OEM", dir, sizeof(dir)/sizeof(dir[0]), szIni );
      if ( !_wcsicmp( dir, L"UTF8" ) ) {
         ofn.nFilterIndex =      3;
      } else if ( !_wcsicmp( dir, L"ANSI" ) ) {
         ofn.nFilterIndex =      2;
      } else {
         ofn.nFilterIndex =      1;
      }
      dir[0] = L'\0';
      ofn.lpstrTitle =        L"Select wanted .ARC files";
      ofn.Flags =             OFN_LONGNAMES | OFN_ALLOWMULTISELECT | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ENABLESIZING | OFN_EXPLORER;
   }
   ofn.hwndOwner =         hwnd;
   ofn.lpstrFile =         fname;
   ofn.nMaxFile =          sizeof( fname )/sizeof(fname[0]);
   ofn.lpstrInitialDir =   dir;
   GetPrivateProfileString( szOpt, szArcDir, L".", dir, sizeof(dir)/sizeof(dir[0]), szIni );
   GetDlgItemText( hwnd, IDC_MAP, fname, sizeof(fname)/sizeof(fname[0]) );
   SetLastError( ERROR_SUCCESS );
   if ( GetOpenFileName( &ofn ) ) {
      // proccess selected files
      FreeAllArcFiles();
      SetDlgItemText( hwnd, IDC_MAP, MapLoad( hwnd, fname, ofn.nFilterIndex, ofn.nFileOffset ) ? fname : L"" );
      WritePrivateProfileString( szOpt, szArcDir, fname, szIni );
      WritePrivateProfileString( szOpt, szArcEncoding, szEncodings[ 0 < ofn.nFilterIndex && ofn.nFilterIndex < 4 ? ofn.nFilterIndex - 1 : 0 ], szIni );
      CheckDlgItems( hwnd );
   }
}


static void OnDlgCommandGetRep( HWND hwnd, HWND hwndCtl, UINT codeNotify ) {
   static OPENFILENAME  ofn;
   wchar_t              fname[ 8192 ];
   wchar_t              dir[ 256 ];
   UNUSED_ARG( codeNotify );
   UNUSED_ARG( hwndCtl );

   if ( !ofn.lStructSize ) {
      // structure was not initialized
      ofn.lStructSize =       sizeof( ofn );
      ofn.lpstrFilter =       L"OEM REP files\0*.rep\0ANSI REP files\0*.rep\0UTF8 REP files\0*.rep\0";
      GetPrivateProfileString( szOpt, szRepEncoding, L"OEM", dir, sizeof(dir)/sizeof(dir[0]), szIni );
      if ( !_wcsicmp( dir, L"UTF8" ) ) {
         ofn.nFilterIndex =      3;
      } else if ( !_wcsicmp( dir, L"ANSI" ) ) {
         ofn.nFilterIndex =      2;
      } else {
         ofn.nFilterIndex =      1;
      }
      dir[0] = L'\0';
      ofn.lpstrTitle =        L"Select wanted .REP files";
      ofn.Flags =             OFN_ALLOWMULTISELECT | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ENABLESIZING | OFN_EXPLORER;
   }
   ofn.hwndOwner =         hwnd;
   ofn.lpstrFile =         fname;
   ofn.nMaxFile =          sizeof( fname )/sizeof( fname[0] );
   ofn.lpstrInitialDir =   dir;
   GetPrivateProfileString( szOpt, szRepDir, L".", dir, sizeof(dir)/sizeof(dir[0]), szIni );
   GetDlgItemText( hwnd, IDC_REP, fname, sizeof(fname)/sizeof( fname[0] ) );
   
   FreeAllRepFiles();

   if ( GetOpenFileName( &ofn ) ) {
      // proccess selected files
      SetDlgItemText( hwnd, IDC_REP, RepLoad( hwnd, fname, ofn.nFilterIndex, ofn.nFileOffset ) ? fname : L"" );
      WritePrivateProfileString( szOpt, szRepDir, fname, szIni );
      WritePrivateProfileString( szOpt, szRepEncoding, szEncodings[ 0 < ofn.nFilterIndex && ofn.nFilterIndex < 4 ? ofn.nFilterIndex - 1 : 0 ], szIni );
      CheckDlgItems( hwnd );
   } else {
      SetDlgItemText( hwnd, IDC_REP, L"" );
   }
}


static void OnDlgCommandGetInput( HWND hwnd, HWND hwndCtl, UINT codeNotify ) {
   static OPENFILENAME  ofn;
   wchar_t              fname[ 256 ];
   wchar_t              dir[ 256 ];
   wchar_t              *p;
   UNUSED_ARG( codeNotify );
   UNUSED_ARG( hwndCtl );

   if ( !ofn.lStructSize ) {
      // structure was not initialized
      ofn.lStructSize =       sizeof( ofn );
      ofn.lpstrFilter =       L"OEM Input files\0*.dat\0ANSI Input files\0*.dat\0UTF8 Input files\0*.dat\0";
      GetPrivateProfileString( szOpt, szInEncoding, L"OEM", dir, sizeof(dir)/sizeof(dir[0]), szIni );
      if ( !_wcsicmp( dir, L"UTF8" ) ) {
         ofn.nFilterIndex =      3;
      } else if ( !_wcsicmp( dir, L"ANSI" ) ) {
         ofn.nFilterIndex =      2;
      } else {
         ofn.nFilterIndex =      1;
      }
      dir[0] = L'\0';
      ofn.lpstrTitle =        L"Choose/enter input file";
      ofn.Flags =             OFN_NOTESTFILECREATE | OFN_ENABLESIZING | OFN_EXPLORER;
   }
   ofn.hwndOwner =         hwnd;
   ofn.lpstrFile =         fname;
   ofn.nMaxFile =          sizeof( fname )/sizeof( fname[0] );
   ofn.lpstrInitialDir =   dir;
   GetPrivateProfileString( szOpt, szInDir, L"", dir, sizeof(dir)/sizeof(dir[0]), szIni );
   wcscpy( fname, dir );
   p = dir + wcslen( dir );
   while ( p > dir ) {
      --p;
      if ( *p == L'\\' ) break;
      if ( *p == L':' ) { p++; break; }
   }
   *p= 0;
   if ( p == dir ) wcscpy( dir, L"." );
   if ( GetOpenFileName( &ofn ) ) {
      // set input file
      SetDlgItemText( hwnd, IDC_INPUT, fname );
      SetDlgItemInt( hwnd, IDC_ENCODING, ofn.nFilterIndex, FALSE );
      WritePrivateProfileString( szOpt, szInDir, fname, szIni );
      WritePrivateProfileString( szOpt, szInEncoding, szEncodings[ 0 < ofn.nFilterIndex && ofn.nFilterIndex < 4 ? ofn.nFilterIndex - 1 : 0 ], szIni );
      CheckDlgItems( hwnd );
   }
}


static void OnDlgCommandGetOutput( HWND hwnd, HWND hwndCtl, UINT codeNotify ) {
   static OPENFILENAME  ofn;
   wchar_t              fname[ 256 ];
   wchar_t              dir[ 256 ];
   wchar_t              *p;
   UNUSED_ARG( codeNotify );
   UNUSED_ARG( hwndCtl );

   if ( !ofn.lStructSize ) {
      // structure was not initialized
      ofn.lStructSize =       sizeof( ofn );
      ofn.lpstrFilter =       L"Output bitmap\0*.bmp\0";
      ofn.nFilterIndex =      1;
      ofn.lpstrTitle =        L"Choose/enter output file";
      ofn.Flags =             OFN_OVERWRITEPROMPT | OFN_ENABLESIZING | OFN_EXPLORER;
   }
   ofn.hwndOwner =         hwnd;
   ofn.lpstrFile =         fname;
   ofn.nMaxFile =          sizeof( fname )/sizeof( fname[0] );
   ofn.lpstrInitialDir =   dir;
   GetPrivateProfileString( szOpt, szOutDir, L"", dir, sizeof(dir)/sizeof(dir[0]), szIni );
   wcscpy( fname, dir );
   p = dir + wcslen( dir );
   while ( p > dir ) {
      --p;
      if ( *p == L'\\' ) break;
      if ( *p == L':' ) { p++; break; }
   }
   *p= 0;
   if ( p == dir ) wcscpy( dir, L"." );
   if ( GetSaveFileName( &ofn ) ) {
      // set output file
      SetDlgItemText( hwnd, IDC_OUTPUT, fname );
      WritePrivateProfileString( szOpt, szOutDir, fname, szIni );
      CheckDlgItems( hwnd );
   }
}


static void OnDlgCommandMarkFont( HWND hwnd, HWND hwndCtl, UINT codeNotify ) {
   static CHOOSEFONT    cf;
   UNUSED_ARG( hwndCtl );
   UNUSED_ARG( codeNotify );

   if ( !cf.lStructSize ) {
      cf.lStructSize =  sizeof(cf);
      cf.hDC =          NULL;
      cf.lpLogFont =    &lfMark;
      cf.Flags =        CF_BOTH | CF_INITTOLOGFONTSTRUCT | CF_NOSIMULATIONS | CF_FORCEFONTEXIST | CF_ANSIONLY;
         // CF_TTONLY | CF_ANSIONLY | CF_BOTH
   }
   cf.hwndOwner = hwnd;
   cf.hDC =       GetDC( hwnd );
   lfMark.lfHeight =  lfMarkSize * GetDeviceCaps( cf.hDC, LOGPIXELSY ) / ( PPI * 10 );
   lfMark.lfWidth =   0;
   if ( ChooseFont( &cf ) ) lfMarkSize = cf.iPointSize;
   ReleaseDC( hwnd, cf.hDC );
   lfMark.lfWidth =   0;
}


static void OnDlgCommandRepFont( HWND hwnd, HWND hwndCtl, UINT codeNotify ) {
   static CHOOSEFONT    cf;
   UNUSED_ARG( hwndCtl );
   UNUSED_ARG( codeNotify );

   if ( !cf.lStructSize ) {
      cf.lStructSize =  sizeof(cf);
      cf.hDC =          NULL;
      cf.lpLogFont =    &lfRep;
      cf.Flags =        CF_BOTH | CF_INITTOLOGFONTSTRUCT | CF_NOSIMULATIONS | CF_FORCEFONTEXIST | CF_ANSIONLY;
         // CF_TTONLY | CF_ANSIONLY | CF_BOTH
   }
   cf.hwndOwner = hwnd;
   cf.hDC =       GetDC( hwnd );
   lfRep.lfHeight =  lfRepSize * GetDeviceCaps( cf.hDC, LOGPIXELSY ) / ( PPI * 10 );
   lfRep.lfWidth =   0;
   if ( ChooseFont( &cf ) ) lfRepSize = cf.iPointSize;
   ReleaseDC( hwnd, cf.hDC );
   lfRep.lfWidth =   0;
}


static void OnDlgCommandClose( HWND hwnd, HWND hwndCtl, UINT codeNotify ) {
   wchar_t        txt[ 128 ];
   static wchar_t szUFmt[] = L"%u";
   UNUSED_ARG( hwndCtl );
   UNUSED_ARG( codeNotify );
   
   WritePrivateProfileString( szOpt, szFontMarkName, lfMark.lfFaceName, szIni );
   swprintf( txt, sizeof(txt)/sizeof(txt[0]), szUFmt, lfMarkSize );
   WritePrivateProfileString( szOpt, szFontMarkSize, txt, szIni );
   WritePrivateProfileString( szOpt, szFontRepName, lfRep.lfFaceName, szIni );
   swprintf( txt, sizeof(txt)/sizeof(txt[0]), szUFmt, lfRepSize );
   WritePrivateProfileString( szOpt, szFontRepSize, txt, szIni );

   Edit_GetText( GetDlgItem( hwnd, IDC_MARKSIZE ), txt, sizeof(txt)/sizeof(txt[0]) );
   WritePrivateProfileString( szOpt, szMrk, txt, szIni );
   Edit_GetText( GetDlgItem( hwnd, IDC_REPSIZE ), txt, sizeof(txt)/sizeof(txt[0]) );
   WritePrivateProfileString( szOpt, szRep, txt, szIni );
   Edit_GetText( GetDlgItem( hwnd, IDC_X ), txt, sizeof(txt)/sizeof(txt[0]) );
   WritePrivateProfileString( szOpt, szWidth, txt, szIni );
   Edit_GetText( GetDlgItem( hwnd, IDC_Y ), txt, sizeof(txt)/sizeof(txt[0]) );
   WritePrivateProfileString( szOpt, szHeight, txt, szIni );

   swprintf( txt, sizeof(txt)/sizeof(txt[0]), szUFmt, Button_GetCheck( GetDlgItem( hwnd, IDC_UPDPICTURE ) ) );
   WritePrivateProfileString( szOpt, szUpdatePicture, txt, szIni );
   
   EndDialog( hwnd, 0 );
}


static void OnDlgCommandUpdatePicture( HWND hwnd, HWND hwndCtl, UINT codeNotify ) {
   UNUSED_ARG( hwnd );
   UNUSED_ARG( codeNotify );
   fUpdatePicture = (BOOL)Button_GetCheck( hwndCtl );
}


static void OnDlgCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify ) {
   switch ( id ) {
   case IDOK:           OnDlgCommandClose( hwnd, hwndCtl, codeNotify ); break;
   case IDC_PRINTER:    ChooseDPI( hwnd, DPI_MANUAL );   break;
   case IDC_UNITS:      OnDlgCommandUnits( hwnd, hwndCtl, codeNotify ); break;
   case IDC_AUTO:       OnDlgCommandAuto( hwnd, hwndCtl, codeNotify ); break;
   case IDC_READ:       OnDlgCommandRead( hwnd, hwndCtl, codeNotify ); break;
   case IDC_GETMAP:     OnDlgCommandGetMap( hwnd, hwndCtl, codeNotify ); break;
   case IDC_GETREP:     OnDlgCommandGetRep( hwnd, hwndCtl, codeNotify ); break;
   case IDC_GETINPUT:   OnDlgCommandGetInput( hwnd, hwndCtl, codeNotify ); break;
   case IDC_GETOUTPUT:  OnDlgCommandGetOutput( hwnd, hwndCtl, codeNotify ); break;
   case IDC_MARKFONT:   OnDlgCommandMarkFont( hwnd, hwndCtl, codeNotify ); break;
   case IDC_REPFONT:    OnDlgCommandRepFont( hwnd, hwndCtl, codeNotify ); break;
   case IDC_UPDPICTURE: OnDlgCommandUpdatePicture( hwnd, hwndCtl, codeNotify ); break;
   default:          break;
   }
}


static BOOL OnDlgInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam ) {
   HWND     hw;
   HFONT    hfont;
   HDC      hdc;
   int      y;
   wchar_t  txt[ 64 ];
   UNUSED_ARG( hwndFocus );
   UNUSED_ARG( lParam );

   GetPrivateProfileString( szOpt, szMrk, L"15", txt, sizeof(txt)/sizeof(txt[0]), szIni );
   Edit_SetText( GetDlgItem( hwnd, IDC_MARKSIZE ), txt );
   GetPrivateProfileString( szOpt, szRep, L"20", txt, sizeof(txt)/sizeof(txt[0]), szIni );
   Edit_SetText( GetDlgItem( hwnd, IDC_REPSIZE ), txt );
   GetPrivateProfileString( szOpt, szHeight, L"10", txt, sizeof(txt)/sizeof(txt[0]), szIni );
   Edit_SetText( GetDlgItem( hwnd, IDC_Y ), txt );
   GetPrivateProfileString( szOpt, szWidth, L"10", txt, sizeof(txt)/sizeof(txt[0]), szIni );
   Edit_SetText( GetDlgItem( hwnd, IDC_X ), txt );

   hw = GetDlgItem( hwnd, IDC_UNITS );
   ComboBox_AddString( hw, L"screen pixels" );
   ComboBox_AddString( hw, L"screen centimeters" );
   ComboBox_AddString( hw, L"screen inches" );
   ComboBox_AddString( hw, L"printer centimeters" );
   ComboBox_AddString( hw, L"printer inches" );
   ComboBox_AddString( hw, L"printer dots" );
   ComboBox_AddString( hw, L"printer points" );
   ComboBox_AddString( hw, L"twips" );
   ComboBox_SetCurSel( hw, GetPrivateProfileInt( szOpt, szUnits, 3, szIni ) );

   SetFocus( GetDlgItem( hwnd, IDC_GETMAP ) );
   ChooseDPI( hwnd, DPI_AUTO );

   GetPrivateProfileString( szOpt, szFontMarkName, L"Arial", lfMark.lfFaceName, sizeof(lfMark.lfFaceName)/sizeof(lfMark.lfFaceName[0]), szIni );
   lfMarkSize = GetPrivateProfileInt( szOpt, szFontMarkSize, 80, szIni );
   GetPrivateProfileString( szOpt, szFontRepName, L"Arial", lfRep.lfFaceName, sizeof(lfRep.lfFaceName)/sizeof(lfRep.lfFaceName[0]), szIni );
   lfRepSize = GetPrivateProfileInt( szOpt, szFontRepSize, 80, szIni );
   hdc = GetDC( hwnd );
   y = lfMarkSize * GetDeviceCaps( hdc, LOGPIXELSY ) / ( PPI * 10 );
   hfont = CreateFont(
      y,0, 0,0, 400, FALSE,FALSE,FALSE,
      ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
      DEFAULT_PITCH | FF_SWISS, lfMark.lfFaceName
   );
   GetObject( hfont, sizeof(lfMark), &lfMark );
   lfMark.lfWidth =   0;
   y = lfRepSize * GetDeviceCaps( hdc, LOGPIXELSY ) / ( PPI * 10 );
   hfont = CreateFont(
      y,0, 0,0, 400, FALSE,FALSE,FALSE,
      ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
      DEFAULT_PITCH | FF_SWISS, lfRep.lfFaceName
   );
   GetObject( hfont, sizeof(lfRep), &lfRep );
   lfRep.lfWidth =   0;
   DeleteObject( hfont );
   ReleaseDC( hwnd, hdc );

   Button_SetCheck(
      GetDlgItem( hwnd, IDC_UPDPICTURE ),
      (int)(fUpdatePicture = GetPrivateProfileInt( szOpt, szUpdatePicture, 1, szIni ) ? TRUE : FALSE)
   );

   // disable start and autostart options until maps loaded
   CheckDlgItems( hwnd );
   return FALSE;
}


BOOL OnDlgEraseBkgnd( HWND hwnd, HDC hdc ) {
   HBRUSH      hbrold;
   RECT        rc;

   GetClientRect( hwnd, &rc );
   hbrold = (HBRUSH)SelectObject( hdc, GetStockBrush( LTGRAY_BRUSH ) );
   PatBlt( hdc, 0,0, rc.right,rc.bottom, PATCOPY );
   SelectObject( hdc, hbrold );
   return TRUE;
}


HBRUSH OnDlgCtlColor( HWND hwnd, HDC hdc, HWND hwndChild, int type ) {
   HBRUSH   hbr;
   switch ( type ) {
   case CTLCOLOR_BTN:   hbr = FORWARD_WM_CTLCOLORBTN( hwnd, hdc, hwndChild, DefDlgProc ); break;
   case CTLCOLOR_STATIC:hbr = FORWARD_WM_CTLCOLORSTATIC( hwnd, hdc, hwndChild, DefDlgProc ); break;
   default:             hbr = (HBRUSH)GetStockObject( LTGRAY_BRUSH ); break;
   }

   switch ( type  ) {
   case CTLCOLOR_BTN:
   case CTLCOLOR_STATIC:
      hbr = (HBRUSH)GetClassLong( hwnd, GCL_HBRBACKGROUND );
      SelectObject( hdc, hbr );
      SetBkColor( hdc, RGB(192,192,192) );
      break;

   default:
      break;
   }
   return hbr;
}


static void OnDlgDestroy( HWND hwnd ) {
   FreeAllArcFiles();
   ChooseDPI( hwnd, DPI_RELEASE );
   if ( timerid ) { KillTimer( hwnd, timerid ); timerid= 0; }

   mustbeinited = TRUE;
}


static void OnDlgPaletteChanged( HWND hwnd, HWND hwndPaletteChange ) {
   if ( hwnd != hwndPaletteChange ) {
      FORWARD_WM_QUERYNEWPALETTE( hwnd, SendMessage );
   }
}


static BOOL OnDlgQueryNewPalette( HWND hwnd ) {
   return FORWARD_WM_QUERYNEWPALETTE( GetDlgItem( hwnd, IDC_BITMAP ), SendMessage );
}


static void OnDlgActivate( HWND hwnd, UINT state, HWND hwndActDeact, BOOL fMinimized ) {
   UNUSED_ARG( hwnd );
   UNUSED_ARG( hwndActDeact );
   UNUSED_ARG( fMinimized );
   
   active = state ? TRUE : FALSE;
   if ( mustbeinited && state ) {
      mustbeinited = FALSE;
      OnDlgInitDialog( hwnd, (HWND)NULL, 0L );
   }
}


static void OnDlgTimer( HWND hwnd, UINT id ) {
   HFILE    hf;
   wchar_t  text[ 256 ];
   UNUSED_ARG( id );

   if ( inwork ) return;
   GetDlgItemText( hwnd, IDC_INPUT, text, sizeof(text)/sizeof(text[0]) );
   if ( text[0] ) {
      hf = _wopen( text, OF_READ | OF_SHARE_EXCLUSIVE );
      if ( hf != HFILE_ERROR ) {
         _lclose( hf );
         inwork = TRUE;
         FORWARD_WM_COMMAND( hwnd, IDC_READ, GetDlgItem( hwnd, IDC_READ ), BN_CLICKED, PostMessage );
      }
   }
}


LONG WINAPI DlgProc( HWND hwnd, UINT wmsg, UINT wParam, LONG lParam ) {
   switch ( wmsg ) {
   HANDLE_MSG( hwnd, WM_CREATE, OnDlgCreate );
   HANDLE_MSG( hwnd, WM_ERASEBKGND, OnDlgEraseBkgnd );
   HANDLE_MSG( hwnd, WM_COMMAND, OnDlgCommand );
   HANDLE_MSG( hwnd, WM_CLOSE, OnDlgClose );
   HANDLE_MSG( hwnd, WM_DESTROY, OnDlgDestroy );
   HANDLE_MSG( hwnd, WM_CTLCOLORBTN, OnDlgCtlColor );
   HANDLE_MSG( hwnd, WM_CTLCOLORSTATIC, OnDlgCtlColor );
   HANDLE_MSG( hwnd, WM_PALETTECHANGED, OnDlgPaletteChanged );
   HANDLE_MSG( hwnd, WM_QUERYNEWPALETTE, OnDlgQueryNewPalette );
   HANDLE_MSG( hwnd, WM_ACTIVATE, OnDlgActivate );
   HANDLE_MSG( hwnd, WM_TIMER, OnDlgTimer );
   }
   return DefDlgProc( hwnd, wmsg, wParam, lParam );
}


int PASCAL WinMain( HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpszCmd, int nCmdShow ) {
   WNDCLASS       wc;
   UNUSED_ARG( lpszCmd );
   UNUSED_ARG( nCmdShow );
   
   hInstance = hInst;
   GetModuleFileName( (HMODULE)hInst, szIni, sizeof(szIni)/sizeof(szIni[0]) );
   wcscpy( szIni + wcslen(szIni) - 3, L"ini" );
   if ( !hPrev ) {
      // Initialize application
      wc.style=         0;
      wc.lpfnWndProc=   WndProc;
      wc.cbClsExtra=    0;
      wc.cbWndExtra=    2 * sizeof(LONG);
      wc.hInstance=     hInst;
      wc.hIcon=         NULL;
      wc.hCursor=       LoadCursor( NULL, IDC_ARROW );
      wc.hbrBackground= (HBRUSH)GetStockObject( WHITE_BRUSH );
      wc.lpszMenuName=  NULL;
      wc.lpszClassName= szBmpClass;
      RegisterClass( &wc );

      wc.lpfnWndProc=   DlgProc;
      wc.cbWndExtra=    DLGWINDOWEXTRA;
      wc.hIcon=         LoadIcon( hInst, L"Aearth" );
      wc.hbrBackground= (HBRUSH)GetStockObject( LTGRAY_BRUSH );
      wc.lpszClassName= szDlgClass;
      RegisterClass( &wc );

      wc.lpfnWndProc=   ResProc;
      wc.hIcon=         NULL;
      wc.lpszClassName= szResClass;
      RegisterClass( &wc );

      DialogBox( hInst, L"MAP", NULL, (DLGPROC)NULL );
   }
   return 0;
}


