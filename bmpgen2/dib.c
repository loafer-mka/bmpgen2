
#include "dib.h"

LPSTR LoadOwnResource( LPOWNRES lpo, LPWSTR name, LPWSTR type )
{
	HRSRC    hr;
	HGLOBAL  hglb = (HGLOBAL)NULL;
	LPSTR    r = (LPSTR)NULL;

	hr = FindResource( (HMODULE)( lpo->hInstance ), name, type );
	if ( hr ) {
		hglb = (HGLOBAL)LoadResource( (HMODULE)( lpo->hInstance ), hr );
		if ( hglb ) {
			r = (LPSTR)LockResource( hglb );
			if ( !r ) hglb = (HGLOBAL)0L;
		}
	}
	lpo->hglb = hglb;
	lpo->ptr = r;
	return r;
}


void FreeOwnResource( LPOWNRES lpo )
{
	lpo->hglb = (HGLOBAL)0L;
	lpo->ptr = (LPSTR)NULL;
}


UINT GetDIBPaletteSize( LPDIB lph )
{
	UINT           palsize = 0;

	if ( lph ) {
		palsize = (WORD)( lph->biClrUsed ) * sizeof( RGBQUAD );
		if ( !palsize ) {
			palsize = lph->biPlanes * lph->biBitCount;
			switch ( (UINT)palsize ) {
			case 1:  case 4:  case 8:  palsize = ( 1 << palsize ) * sizeof( RGBQUAD );  break;
			case 16: case 32:          palsize = 12;  break;
			default:                   palsize = 0;   break;
			}
		}
	}
	return palsize;
}


HPALETTE  CreateDIBPalette( LPDIB lph )
{
	HPALETTE		r = (HPALETTE)0L;
	UINT			palsize;
	LPLOGPALETTE		lpp;
	LPRGBQUAD		from;
	LPPALETTEENTRY	to;
	UINT			i;

	if ( lph ) {
		palsize = GetDIBPaletteSize( lph );
		if ( palsize && palsize != 12 ) {
			lpp = (LPLOGPALETTE)malloc( palsize + sizeof( LOGPALETTE ) );
			if ( lpp ) {
				lpp->palVersion = 0x300;
				lpp->palNumEntries = (WORD)( palsize / sizeof( RGBQUAD ) );
				// warning! color components of PALETTEENTRY and RGBQUAD are
				// placed into different order! we must reorder they when copying.
				to = lpp->palPalEntry;
				from = (LPRGBQUAD)( (LPSTR)(lph)+(WORD)( lph->biSize ) );
				for ( i = 0; i < palsize / sizeof( RGBQUAD ); i++, to++, from++ ) {
					to->peRed = from->rgbRed;
					to->peGreen = from->rgbGreen;
					to->peBlue = from->rgbBlue;
					to->peFlags = 0;
				}
				r = CreatePalette( lpp );
				free( lpp );
			}
		}
	}
	return r;
}


HBITMAP CreateDDBfromDIB( HDC hdc, LPDIB pdib )
{
	UINT	palSize = GetDIBPaletteSize( pdib );

	return CreateDIBitmap(
		hdc,
		(LPBITMAPINFOHEADER)pdib,
		CBM_INIT,
		(LPVOID)( ( (LPSTR)pdib ) + (WORD)( pdib->biSize ) + palSize ),
		(LPBITMAPINFO)pdib,
		DIB_RGB_COLORS
	);
}


DWORD   GetDIBImageSize( WORD bpp, LONG width, LONG height )
{
	DWORD    imagesize;

	imagesize = (DWORD)( width >= 0L ? width : -width );
	imagesize *= bpp;
	imagesize = ( ( imagesize + 31 ) & ~31 ) >> 3;
	imagesize *= height > 0L ? height : -height;
	return imagesize;
}


/* default DPIX, DPIY := 96 */
LPDIB   CreateDIB( WORD bpp, LONG  width, LONG height, UINT DPIX, UINT DPIY )
{
	LPDIB    lpb;
	UINT     palsize;
	LONG     imagesize;

	// check input data
	if (
		!( width && height && ( bpp == 1 || bpp == 4 || bpp == 8 || bpp == 16 || bpp == 24 || bpp == 32 ) )
	) return (LPDIB)NULL;

	switch ( bpp ) {
	case 1:  case 4: case 8:   palsize = ( 1 << bpp ) * sizeof( RGBQUAD );  break;
	case 16: case 32:          palsize = 12;  break;	// color masks (RGB, 3*4)
	default:                   palsize = 0;   break;
	}

	imagesize = GetDIBImageSize( bpp, width, height );

	lpb = (LPDIB)malloc( sizeof( BITMAPINFOHEADER ) + palsize + imagesize );
	if ( lpb ) {
		lpb->biSize = sizeof( BITMAPINFOHEADER );
		lpb->biWidth = width;
		lpb->biHeight = height;
		lpb->biPlanes = 1;
		lpb->biBitCount = bpp;
		lpb->biCompression = bpp == 16 || bpp == 32 ? BI_BITFIELDS : BI_RGB;
		lpb->biSizeImage = imagesize;
		lpb->biXPelsPerMeter = (LONG)( ( DPIX * 1000000UL ) / INCH );
		lpb->biYPelsPerMeter = (LONG)( ( DPIY * 1000000UL ) / INCH );
		lpb->biClrUsed = 0;
		lpb->biClrImportant = 0;
		memset( lpb + 1, 0, palsize );
	}
	return lpb;
}


LPDIB   CreateDIBWithDDB( HDC hdc, HBITMAP *phbmp, WORD bpp, LONG  width, LONG height, UINT DPIX, UINT DPIY )
{
	LPDIB    lpb = (LPDIB)0L;
	UINT     palsize;
	LONG     imagesize, fullsize;
	HANDLE   hmap;
	void    *pdata;

	*phbmp = 0;

	// check input data
	if (
		!( width && height && ( bpp == 1 || bpp == 4 || bpp == 8 || bpp == 16 || bpp == 24 || bpp == 32 ) )
	) return (LPDIB)NULL;

	switch ( bpp ) {
	case 1:  case 4: case 8:   palsize = ( 1 << bpp ) * sizeof( RGBQUAD );  break;
	case 16: case 32:          palsize = 12;  break;	// color masks (RGB, 3*4)
	default:                   palsize = 0;   break;
	}

	imagesize = GetDIBImageSize( bpp, width, height );

	fullsize = sizeof( BITMAPINFOHEADER ) + palsize + imagesize;
	/* SEC_COMMIT */
	hmap = CreateFileMapping( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, fullsize, NULL );
	if ( NULL != hmap ) {
		lpb = (LPDIB)MapViewOfFile( hmap, FILE_MAP_WRITE, 0, 0, fullsize );
		if ( lpb ) {
			UINT			i;
			register RGBQUAD	*prgb;

			lpb->biSize = sizeof( BITMAPINFOHEADER );
			lpb->biWidth = width;
			lpb->biHeight = height;
			lpb->biPlanes = 1;
			lpb->biBitCount = bpp;
			lpb->biCompression = bpp == 16 || bpp == 32 ? BI_BITFIELDS : BI_RGB;
			lpb->biSizeImage = imagesize;
			lpb->biXPelsPerMeter = (LONG)( ( DPIX * 1000000UL ) / INCH );
			lpb->biYPelsPerMeter = (LONG)( ( DPIY * 1000000UL ) / INCH );
			lpb->biClrUsed = 0;
			lpb->biClrImportant = 0;

			switch ( bpp ) {
			case 1:  
				prgb = &( (RGBQUAD*)( lpb + 1 ) )[ 0 ];
				prgb->rgbBlue = prgb->rgbGreen = prgb->rgbRed = (BYTE)0;
				prgb->rgbReserved = 0;
				prgb = &( (RGBQUAD*)( lpb + 1 ) )[ 1 ];
				prgb->rgbBlue = prgb->rgbGreen = prgb->rgbRed = (BYTE)255;
				prgb->rgbReserved = 0;
				break;
			case 4: 
				for ( i = 0; i < 16; i++ ) {
					prgb = &( (RGBQUAD*)( lpb + 1 ) )[ i ];
					prgb->rgbBlue = prgb->rgbGreen = prgb->rgbRed = (BYTE)( i * 17 );
					prgb->rgbReserved = 0;
				}
				break;
			case 8:
				for ( i = 0; i < 256; i++ ) {
					register RGBQUAD	*prgb = &( (RGBQUAD*)( lpb + 1 ) )[ i ];
					prgb->rgbBlue = prgb->rgbGreen = prgb->rgbRed = (BYTE)i;
					prgb->rgbReserved = 0;
				}
				break;
			case 16:
				( (DWORD*)( lpb + 1 ) )[ 0 ] = 0x7C00;	// red
				( (DWORD*)( lpb + 1 ) )[ 1 ] = 0x03E0;	// green
				( (DWORD*)( lpb + 1 ) )[ 2 ] = 0x001F;	// blue
				break;
			case 32:
				( (DWORD*)( lpb + 1 ) )[ 0 ] = 0xFF0000;// red
				( (DWORD*)( lpb + 1 ) )[ 1 ] = 0x00FF00;// green
				( (DWORD*)( lpb + 1 ) )[ 2 ] = 0x0000FF;// blue
				break;
			default:
				break;
			}

			*phbmp = CreateDIBSection( hdc, (PBITMAPINFO)lpb, DIB_RGB_COLORS, &pdata, hmap, sizeof( BITMAPINFOHEADER ) + palsize );
			if ( !*phbmp ) {
				UnmapViewOfFile( lpb );
				lpb = (LPDIB)0L;
			}
		}
		if ( !lpb ) {
			CloseHandle( hmap );
			hmap = (HANDLE)0L;
		}
	}
	return lpb;
}


void FreeDIBWithDDB( LPDIB pdib, HBITMAP hbmp )
{
	if ( hbmp ) {
		DIBSECTION	ds;

		if ( GetObject( hbmp, sizeof( ds ), &ds ) ) {
			if ( ds.dshSection ) {
				if ( pdib ) UnmapViewOfFile( pdib );
				CloseHandle( ds.dshSection );
			}
		}
		DeleteBitmap( hbmp );
	}
}


BOOL SaveDIB( LPWSTR file, LPDIB lpb )
{
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
				(WORD)( lpb->biPlanes * lpb->biBitCount ), lpb->biWidth, lpb->biHeight
			);
			palsize = GetDIBPaletteSize( lpb );
			bmf.bfType = 0x4D42;
			bmf.bfOffBits = sizeof( BITMAPFILEHEADER ) + lpb->biSize + palsize;
			bmf.bfSize = bmf.bfOffBits + imagesize;
			bmf.bfReserved1 = 0L;
			bmf.bfReserved2 = 0L;

			if ( fwrite( (LPSTR)&bmf, 1, sizeof( bmf ), fp ) == sizeof( bmf ) ) {
				bmf.bfSize -= sizeof( BITMAPFILEHEADER );
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


/* default DPIX, DPIY := 96 */
LPDIB CreateDIBfromDDB( HDC hdc, HBITMAP hbmp, UINT DPIX, UINT DPIY )
{
	BITMAP      bmp;
	LPDIB       lpb = (LPDIB)0L;
	HBITMAP     holdbmp;
	HDC         hworkdc = hdc;

	if ( hbmp ) {
		GetObject( hbmp, sizeof( BITMAP ), &bmp );
		lpb = CreateDIB( bmp.bmPlanes*bmp.bmBitsPixel, bmp.bmWidth, bmp.bmHeight, DPIX, DPIY );
		if ( !hworkdc ) {
			hworkdc = CreateCompatibleDC( NULL );
			holdbmp = SelectBitmap( hworkdc, hbmp );
		} else {
			holdbmp = (HBITMAP)0L;
		}
		if ( lpb ) {
			UINT	palSize = GetDIBPaletteSize( lpb );
			GetDIBits(
				hworkdc,
				hbmp,
				0, bmp.bmHeight,
				(LPVOID)( ( (LPSTR)( lpb ) ) + lpb->biSize + palSize ),
				(LPBITMAPINFO)lpb,
				DIB_RGB_COLORS
			);
		}
		if ( holdbmp ) SelectBitmap( hworkdc, holdbmp );
		if ( !hdc ) DeleteDC( hworkdc );
	}
	return lpb;
}
