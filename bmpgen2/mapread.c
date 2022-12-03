#include "bmpgen.h"
#include "e-file.h"
#include "dib.h"

// ----------------------- static data ------------------
static wchar_t		mapread[] = L"Map Reader";
static geoarea_t	fmap, fdraw;			/* same as 'map' and 'draw', but floating point */
static double		fdm_width, fdm_height, fd_top;	/* they are used by g2d() convertor */


// --------------------------------------- .arc files -------------------------------------
static BOOL OnResCreate( HWND hwnd, CREATESTRUCT FAR* lpCreateStruct )
{
	BOOL  a;
	a = FORWARD_WM_CREATE( hwnd, lpCreateStruct, DefDlgProc ) ? FALSE : TRUE;
	PostMessage( hwnd, WM_USER + 1000, (WPARAM)0, (LPARAM)( lpCreateStruct->lpCreateParams ) );
	return a;
}


static void dlg_cb_proc( wchar_t *str, int n, va_list ap )
{
	int		cnt;
	HWND		hwndLB;


	cnt = va_arg( ap, int );
	hwndLB = va_arg( ap, HWND );

	if ( n > cnt ) {
		ListBox_AddString( hwndLB, str );
	} else {
		ListBox_InsertString( hwndLB, n, str );
	}
}

static LPWSTR   dirname_ptr;
static int      dirname_off;
static UINT	dirname_encoding;
static LONG OnResStart( HWND hwnd, HWND hwndLB )
{
	BOOL	a = FALSE;
	wchar_t	*p;
	LPWSTR	file;
	wchar_t	temp[ _MAX_PATH + 512 ];
	wchar_t	txt[ _MAX_PATH + 512 ];
	wchar_t *eot;

	if ( dirname_off ) {
		dirname_ptr[ dirname_off - 1 ] = 0;  // place terminator
	}
	p = (wchar_t*)dirname_ptr + dirname_off;

	a = TRUE;
	eot = temp + sizeof( temp ) / sizeof( temp[ 0 ] ) - 1;
	while ( *p ) {
		wcscpy( temp, dirname_ptr );
		if ( dirname_off < 2 || temp[ dirname_off - 2 ] != L'\\' ) wcscat( temp, L"\\" );
		file = temp + wcslen( temp );
		while ( *p && file < eot ) *file++ = *p++;
		while ( *p++ ) {}
		*file = 0;

		swprintf( txt, sizeof( txt ) / sizeof( txt[ 0 ] ), L"Чтение файла '%s' с картой:", temp );
		ListBox_AddString( hwndLB, txt );
		if ( !AddArcFile( temp, dirname_encoding, dlg_cb_proc, ListBox_GetCount( hwndLB ), ListBox_GetCount( hwndLB ), hwndLB ) ) {
			switch ( errno ) {
			case ENOMEM:
				MessageBox( hwnd, L"Некорректный файл или недостаточно ресурсов", mapread, MB_OK | MB_ICONEXCLAMATION );
				break;
			case ENOENT:
				swprintf( txt, sizeof( txt ) / sizeof( txt[ 0 ] ), L"Ошибка открытия файла '%ls'", temp );
				MessageBox( hwnd, txt, mapread, MB_OK | MB_ICONEXCLAMATION );
				break;
			}
		}
	}
	if ( dirname_off >= 2 && dirname_ptr[ dirname_off - 2 ] == L'\\' ) dirname_ptr[ dirname_off - 2 ] = L'\0';
	return (LONG)a;
}


static void OnResCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
	UNUSED_ARG( hwndCtl );
	UNUSED_ARG( codeNotify );

	switch ( id ) {
	case IDOK:  EndDialog( hwnd, 1 );   break;
	default:    break;
	}
}


LRESULT CALLBACK LoadingResultsProc( HWND hwnd, UINT wmsg, WPARAM wParam, LPARAM lParam )
{
	switch ( wmsg ) {
	case WM_USER + 1000:
		return OnResStart( hwnd, GetDlgItem( hwnd, IDC_RESULTS ) );
	HANDLE_MSG( hwnd, WM_CREATE, OnResCreate );
	HANDLE_MSG( hwnd, WM_COMMAND, OnResCommand );
	HANDLE_MSG( hwnd, WM_ERASEBKGND, OnDlgEraseBkgnd );
	HANDLE_MSG( hwnd, WM_CTLCOLORBTN, OnDlgCtlColor );
	HANDLE_MSG( hwnd, WM_CTLCOLORSTATIC, OnDlgCtlColor );
	}
	return DefDlgProc( hwnd, wmsg, wParam, lParam );
}


BOOL  MapLoad( HWND hwndParent, LPWSTR dirname, UINT encoding, int off )
{
	dirname_ptr = dirname;
	dirname_off = off;
	dirname_encoding = encoding;
	DialogBox( (HINSTANCE)hInstance, L"RESULTS", hwndParent, (DLGPROC)NULL );
	return IsMapFileCreated() ? TRUE : FALSE;
}


// ------------------------------------ .rep files ------------------------------------------
BOOL  RepLoad( HWND hwnd, LPWSTR dirname, UINT encoding, int off )
{
	BOOL      a = FALSE;
	LPWSTR    p;
	LPWSTR    file;
	wchar_t   temp[ 512 ];
	wchar_t   txt[ 512 ];
	wchar_t   *eot;

	if ( off ) {
		dirname[ off - 1 ] = 0;  // place terminator
	}
	p = dirname + off;

	a = TRUE;
	eot = temp + sizeof( temp ) / sizeof( temp[ 0 ] ) - 1;
	while ( *p ) {
		wcscpy( temp, dirname );
		wcscat( temp, L"\\" );
		file = temp + wcslen( temp );
		while ( *p && file < eot ) *file++ = *p++;
		while ( *p++ ) {}
		*file = 0;

		a = AddRepFile( temp, encoding );
		if ( !a ) {
			if ( errno == ENOMEM ) {
				MessageBox( hwnd, L"Not enought memory", mapread, MB_OK | MB_ICONEXCLAMATION );
			} else if ( errno == ENOENT ) {
				swprintf( txt, sizeof( txt ) / sizeof( txt[ 0 ] ), L"Ошибка открытия файла '%ls'", temp );
				MessageBox( hwnd, txt, mapread, MB_OK | MB_ICONEXCLAMATION );
			} else {
				MessageBox( hwnd, L"Strange error", mapread, MB_OK | MB_ICONEXCLAMATION );
			}
		}
	}
	return a;
}


// ------------------------------ draw map on bitmap -----------------------------------
static void g2d( geopoint_P geo, geopoint_P paint )
{
	// convert physical coordinates to coordinates of drawing area
	paint->x = floor( fdraw.left + ( geo->x - fmap.west ) * fdm_width );
	paint->y = floor( fd_top - ( geo->y - fmap.south ) * fdm_height );
}


static void DrawOutline( HDC hdc, outlinesegment_P lpa, geopoint_P lpp )
{
	BOOL			fPoint0;
	geopoint_P		p0, p;
	geopoint_t		pt;
	POINT			wpt;
	outlinesegment_P	lpn;
	size_t			i, j;
	BOOL			all;

	for ( i = 0; i < SPLITPOW; i++, lpa++ ) {
		if ( lpa->last == 0 ) continue;     // empty segment
		if (
			lpa->area.west >= fmap.east ||
			lpa->area.east <= fmap.west ||
			lpa->area.north <= fmap.south ||
			lpa->area.south >= fmap.north
			) continue;                         // segment is out of map area

			// segment is overbound
		lpn = lpa->next;  all = TRUE;
		if ( lpn ) for ( j = 0; j < SPLITPOW; j++, lpn++ ) {
			if (
				lpn->area.west >= fmap.east ||
				lpn->area.east <= fmap.west ||
				lpn->area.north <= fmap.south ||
				lpn->area.south >= fmap.north
				) all = FALSE;
		}
		if ( all == TRUE ) {    // draw all points from segment
			p = p0 = lpp + ( j = lpa->first );  fPoint0 = FALSE;
			while ( j < lpa->last ) {          // loop for points
				j++;  p++;
				if (
					!(
					( p->x < fmap.west && p0->x < fmap.west ) ||
						( p->x > fmap.east && p0->x > fmap.east ) ||
						( p->y < fmap.south && p0->y < fmap.south ) ||
						( p->y > fmap.north && p0->y > fmap.north )
						)
					) {
					if ( !fPoint0 ) { g2d( p0, &pt );  MoveToEx( hdc, (int)pt.x, (int)pt.y, &wpt ); }
					g2d( p, &pt );  LineTo( hdc, (int)pt.x, (int)pt.y );
					fPoint0 = TRUE;
				} else {
					fPoint0 = FALSE;
				}
				p0 = p;
			}
		} else {                // analyse, when some subsegments are outbound
			DrawOutline( hdc, lpa->next, lpp );
		}
	}
}


int MapDraw(
	HDC hdc, wchar_t *fname, UINT encoding, UINT width, UINT height, int dpix, int dpiy,
	int marksize, LPLOGFONT lplf, int lfSize,
	int repsize, LPLOGFONT lplfRep, int lfRepSize,
	BOOL fMetric, BOOL fColored, drawings_t what_draw
)
{
	// width and height are width and height of bitmap, in pixels
//	char		title[ MAP_TITLE_LENGTH ];
	wchar_t		*s;
	wchar_t		*ws;
	geopoint_t	p, p0, pw;
	geo_t		T;
	double		v;
	int		pts, D, Dmx, Dmy, Drx, Dry, Dtx, Dty, i, n;
	int		npen;
	mapfile_P	lpm;
	mapoutline_P	lpo;
	repfile_P	lpri;
	reppoint_P	lprp;
	struct EFILE	infile;
	char		temp_buf[ FILE_BUFFER_SIZE ];
	wchar_t		line_buf[ LINE_BUFFER_SIZE ];
	POINT		pt;
	HPEN		hpen[ 14 ];
	HFONT		hfont;
	TEXTMETRIC	tm;
#define G2G(g,m,s)		((g)+(m)/geo_60+(s)/geo_3600)
	static geo_t	step[] = {
		G2G( 40,0,0 ), G2G( 30,0,0 ), G2G( 20,0,0 ), G2G( 10,0,0 ), G2G( 5,0,0 ), G2G( 4,0,0 ), G2G( 3,0,0 ), G2G( 2,0,0 ),
		G2G( 1,0,0 ), G2G( 0,45,0 ), G2G( 0,30,0 ), G2G( 0,15,0 ), G2G( 0,10,0 ), G2G( 0,6,0 ), G2G( 0,5,0 ), G2G( 0,3,0 ),
		G2G( 0,1,0 ), G2G( 0,0,45 ), G2G( 0,0,30 ), G2G( 0,0,15 ), G2G( 0,0,10 ), G2G( 0,0,6 ), G2G( 0,0,5 ), G2G( 0,0,3 ),
		G2G( 0,0,1 ),
		0.0
	};
#define LEGACY_STEP_SCALE	1000.0

	if ( !e_fopen( &infile, fname, encoding ) ) return 0;
	e_fsetvbuf( &infile, temp_buf, _IOFBF, sizeof( temp_buf ) );

	// determine drawing area: bitmap size without borders
	lplf->lfHeight = lfSize * dpiy / ( PPI * 10 );       lplf->lfWidth = 0;
	lplfRep->lfHeight = lfRepSize * dpiy / ( PPI * 10 );  lplfRep->lfWidth = 0;

	// 'draw' is a drawing area (for drawing surface y-axis is up-to-down!)
	fdraw.left = dpix * lfSize * 4 / ( 30 * PPI );
	if ( fdraw.left > width / 2 ) fdraw.left = 0;
	fdraw.right = width;
	fdraw.top = 0;
	n = dpiy * lfSize * 4 / ( 30 * PPI );  if ( (UINT)n > height / 2 ) n = 0;
	fdraw.bottom = height - n;
	fdraw.width = fdraw.right - fdraw.left;
	fdraw.height = fdraw.bottom - fdraw.top;

	// pass #1 - get minimal/maximal coordinates
	e_fgets( line_buf, sizeof( line_buf ) / sizeof( line_buf[ 0 ] ), &infile );
	trimline( s = bow( line_buf ) );	/* skip title */

	for ( pts = 0; !e_feof( &infile ); pts++ ) {  // enumerate points
		if ( !e_fgets( line_buf, sizeof( line_buf ) / sizeof( line_buf[ 0 ] ), &infile ) ) continue;
		p.latitude = atogeo( s = bow( line_buf ) );
		if ( !*s ) continue;
		p.longitude = atogeo( nextw( s ) );
		if ( !pts ) {
			fmap.west = fmap.east = p.x; fmap.north = fmap.south = p.y;
		} else {
			if ( fmap.west > p.x ) fmap.west = p.x; else if ( fmap.east < p.x ) fmap.east = p.x;
			if ( fmap.south > p.y ) fmap.south = p.y; else if ( fmap.north < p.y ) fmap.north = p.y;
		}
	}

	// expand physical area by 5%
	T = ( fmap.east - fmap.west ) / 20.0;   fmap.west -= T;  fmap.east += T;
	T = ( fmap.north - fmap.south ) / 20.0; fmap.south -= T; fmap.north += T;

	// apply drawing proportions to spatial area
	T = ( ( ( fmap.north - fmap.south ) * dpiy / dpix ) * fdraw.width ) / fdraw.height; /* T is wanted spatial width */
	if ( fMetric ) {
		T = T / cos( ( (double)( fmap.north + fmap.south ) ) / 114.59156 );
		// 114.59156 = 1/2 of N+S, divided by 1000 (N,S are degrees) and divided by 57.296 (degrees per rad)
	}
	if ( T >= ( fmap.east - fmap.west ) ) {   // expand by width
		T = ( T - ( fmap.east - fmap.west ) ) / 2;
		fmap.west -= T;  fmap.east += T;
	} else {                               // expand by height
		T = ( ( ( fmap.east - fmap.west ) * dpix / dpiy ) * fdraw.height ) / fdraw.width;
		if ( fMetric ) {
			T = T * cos( ( (double)( fmap.north + fmap.south ) ) / 114.59156 );
		}
		T = ( T - ( fmap.north - fmap.south ) ) / 2;
		fmap.north += T;   fmap.south -= T;
	}
	fmap.width = fmap.east - fmap.west;
	fmap.height = fmap.north - fmap.south;

	// these variables are used by g2d() function
	fd_top = fdraw.top + fdraw.height;
	fdm_height = fdraw.height / fmap.height;
	fdm_width = fdraw.width / fmap.width;

	// get base unit - 0.1 mm (width of pen, spacing etc)
	D = dpix * 100L / INCH;
	if ( D < 1 ) D = 1;                           // D is in device units (0.1 mm)
	if ( !marksize ) marksize = 1; if ( marksize > 100 ) marksize = 100;
	Dmx = dpix * 100L * marksize / ( 2 * INCH ); // 0.xxx mm - marker point, radius
	if ( Dmx < 1 ) Dmx = 1;
	Dmy = dpiy * 100L * marksize / ( 2 * INCH );
	if ( !repsize ) repsize = 1;   if ( repsize > 150 ) repsize = 150;
	Drx = dpix * 100L * repsize / ( 2 * INCH );  // 0.xxx mm - reper point, radius
	if ( Drx < 1 ) Drx = 1;
	Dry = dpiy * 100L * repsize / ( 2 * INCH );
	if ( Dry < 1 ) Dry = 1;
	hpen[ 0 ] = CreatePen( PS_SOLID, D, RGB( 255, 255, 255 ) );      // #0  white         0.1 mm
	hpen[ 1 ] = CreatePen( PS_SOLID, D, RGB( 0, 0, 0 ) );            // #1  black         0.1 mm
	hpen[ 2 ] = CreatePen( PS_SOLID, D * 2, RGB( 0, 0, 0 ) );        // #2  black         0.2 mm
	hpen[ 3 ] = CreatePen( PS_SOLID, D * 3, RGB( 0, 0, 0 ) );        // #3  black         0.3 mm
	hpen[ 4 ] = CreatePen( PS_SOLID, D * 5, RGB( 0, 0, 0 ) );        // #4  black         0.5 mm
	hpen[ 5 ] = CreatePen( PS_SOLID, D * 7, RGB( 0, 0, 0 ) );        // #5  black         0.7 mm
	hpen[ 6 ] = CreatePen( PS_SOLID, D * 10, RGB( 0, 0, 0 ) );       // #6  black         1.0 mm
	hpen[ 7 ] = CreatePen( PS_SOLID, D * 15, RGB( 0, 0, 0 ) );       // #7  black         1.5 mm
	hpen[ 8 ] = CreatePen( PS_SOLID, D * 20, RGB( 0, 0, 0 ) );       // #8  black         2.0 mm
	hpen[ 9 ] = CreatePen( PS_DOT, 1, RGB( 0, 0, 0 ) );              // #9  dot           thin
	hpen[ 10 ] = CreatePen( PS_DASH, 1, RGB( 0, 0, 0 ) );            // #10 dash          thin
	hpen[ 11 ] = CreatePen( PS_DASHDOT, 1, RGB( 0, 0, 0 ) );         // #11 dash-dot      thin
	hpen[ 12 ] = CreatePen( PS_DASHDOTDOT, 1, RGB( 0, 0, 0 ) );      // #12 dash-dot-dot  thin
	//
	hpen[ 13 ] = CreatePen( PS_SOLID, D, RGB( 128, 128, 128 ) );     // #1  gray          0.1 mm

	// draw map
	if ( what_draw & DRAW_ARCS ) {
		SelectObject( hdc, hpen[ 1 ] );    npen = (WORD)1;
		for ( lpm = GetRootArc(); lpm; lpm = lpm->next ) {   // loop for maps
			if (
				lpm->area.west >= fmap.east ||
				lpm->area.east <= fmap.west ||
				lpm->area.north <= fmap.south ||
				lpm->area.south >= fmap.north
				) continue;                               // entire map is outbound
			for ( n = 0; (size_t)n < lpm->outlines; n++ ) {    // loop for outlines
				lpo = lpm->outline + n;
				if (
					lpo->area.west >= fmap.east ||
					lpo->area.east <= fmap.west ||
					lpo->area.north <= fmap.south ||
					lpo->area.south >= fmap.north
					) continue;                            // entire outline is outbound

					// outline is overbound us map area; check partitions for drawing
				if ( npen != lpo->penstyle ) SelectObject( hdc, hpen[ npen = lpo->penstyle ] );
				//..................................
				DrawOutline( hdc, (outlinesegment_P)( lpo->point + lpo->points ), lpo->point );
				//..................................
			}
		}
	}
	// erase frame and draw border
	if ( fdraw.left ) PatBlt( hdc, 0, 0, (int)fdraw.left, height, PATCOPY );
	if ( height != (UINT)fdraw.bottom ) PatBlt( hdc, 0, (int)fdraw.bottom, width, height - (int)fdraw.bottom, PATCOPY );

	if ( what_draw & DRAW_GRID ) {
		if ( fColored ) {
			SelectObject( hdc, hpen[ 13 ] );    npen = (WORD)13;
			SetTextColor( hdc, RGB( 128, 128, 128 ) );
		} else {
			SelectObject( hdc, hpen[ 1 ] );    npen = (WORD)1;
		}
		MoveToEx( hdc, (int)fdraw.left, (int)fdraw.top + D, &pt ); LineTo( hdc, (int)fdraw.left, (int)fdraw.bottom - D - 1 );
		LineTo( hdc, (int)fdraw.right - D - 1, (int)fdraw.bottom - D - 1 );    LineTo( hdc, (int)fdraw.right - D - 1, (int)fdraw.top + D );
		LineTo( hdc, (int)fdraw.left, (int)fdraw.top + D );
	}

	lplf->lfEscapement = lplf->lfOrientation = 0;
	hfont = CreateFontIndirect( lplf );
	SelectObject( hdc, hfont );
	GetTextExtentPoint( hdc, L"8888888888888", 13, (LPSIZE)&pt );

	p0.x = pt.x / fdm_width;	// average step of grid in 0.001 of degree
	p0.y = pt.x / fdm_height;

	pw.x = pw.y = step[ 0 ];
	for ( n = 0; step[ n ]; n++ ) { pw.x = step[ n ];  if ( p0.x > step[ n + 1 ] ) break; }
	for ( n = 0; step[ n ]; n++ ) { pw.y = step[ n ];  if ( p0.y > step[ n + 1 ] ) break; }
	// now 'pw.x' and 'pw.y' are steps of grid in 0.001 of degree

	// draw grid and type coords
	// we suppose step of grid enought for "888888888" string
	if ( what_draw & DRAW_GRID ) {
		SetBkMode( hdc, TRANSPARENT );
		SetTextAlign( hdc, TA_LEFT | TA_TOP );
		p0.x = fmap.west > 0L ? fmap.west - fmod( fmap.west, pw.x ) : fmap.west + fmod( -fmap.west, pw.x );
		p0.y = 0;	p0.x += pw.x;
		for ( ; p0.x <= fmap.east; p0.x += pw.x ) {
			g2d( &p0, &p );   MoveToEx( hdc, (int)p.x, (int)fdraw.top + D, &pt ); LineTo( hdc, (int)p.x, (int)fdraw.bottom - D );
			ws = spatialtoa( p0.x ); n = (int)wcslen( ws );
			GetTextExtentPoint( hdc, ws, n, (LPSIZE)&pt );
			pt.x /= 2;
			if ( p.x - pt.x < (int)fdraw.left ) p.x = (int)fdraw.left; else {
				if ( p.x + pt.x > fdraw.right ) p.x = fdraw.right - 2 * pt.x; else p.x -= pt.x;
			}
			TextOut( hdc, (int)p.x, (int)fdraw.bottom + D, ws, n );
		}
		lplf->lfEscapement = lplf->lfOrientation = 900;
		hfont = CreateFontIndirect( lplf );
		DeleteObject( SelectObject( hdc, hfont ) );
		SetTextAlign( hdc, TA_LEFT | TA_BOTTOM );
		p0.y = fmap.south > 0L ? fmap.south + pw.y - fmod( fmap.south, pw.y ) : fmap.south + fmod( -fmap.south, pw.y );
		if ( p0.y < -90000L ) p0.y = -90000L;
		p0.x = 0;
		for ( ; p0.y <= fmap.north; p0.y += pw.y ) {
			if ( p0.y > geo_90 ) break;
			g2d( &p0, &p );   MoveToEx( hdc, (int)fdraw.left, (int)p.y, &pt ); LineTo( hdc, (int)fdraw.right - D, (int)p.y );
			ws = spatialtoa( p0.y ); n = (int)wcslen( ws );
			GetTextExtentPoint( hdc, ws, n, (LPSIZE)&pt );
			pt.x /= 2;
			if ( p.y - pt.x < fdraw.top ) p.y = fdraw.top + pt.x * 2; else {
				if ( p.y + pt.x > fdraw.bottom ) p.y = fdraw.bottom; else p.y += pt.x;
			}
			TextOut( hdc, (int)fdraw.left - D, (int)p.y, ws, n );
		}
		if ( fColored ) {
			// revert gray to black
			SelectObject( hdc, hpen[ 1 ] );    npen = (WORD)1;
			SetTextColor( hdc, RGB( 0, 0, 0 ) );
		}
	}

	// draw reper points
	if ( what_draw & DRAW_REPERS ) {
		lplfRep->lfEscapement = lplfRep->lfOrientation = 0;
		hfont = CreateFontIndirect( lplfRep );
		DeleteObject( SelectObject( hdc, hfont ) );
		SetTextAlign( hdc, TA_LEFT | TA_TOP );
		GetTextMetrics( hdc, &tm );
		SelectObject( hdc, hpen[ 2 ] );
		SelectObject( hdc, GetStockObject( HOLLOW_BRUSH ) );
		for ( lpri = GetRootRep(); lpri; lpri = lpri->next ) {     // loop for .rep files
			if ( lpri->dp < (long)( pw.y*LEGACY_STEP_SCALE ) ) continue;
			lprp = (reppoint_P)( lpri + 1 );
			for ( T = (geo_t)lpri->points; T > 0L; --T, lprp++ ) {  // loop for rep. points
				if (
					lprp->pt.x > fmap.west &&
					lprp->pt.x < fmap.east &&
					lprp->pt.y > fmap.south &&
					lprp->pt.y < fmap.north &&
					lprp->dp >= (long)( pw.y*LEGACY_STEP_SCALE )
					) {
					g2d( &( lprp->pt ), &p );
					if ( lprp->rsize ) {
						Dtx = Drx * lprp->rsize / 100;    if ( !Dtx ) Dtx = 1;
						Dty = Dry * lprp->rsize / 100;    if ( !Dty ) Dty = 1;
						Ellipse( hdc, (int)p.x - Dtx, (int)p.y - Dty, (int)p.x + Dtx, (int)p.y + Dty );    // draw reper point
					}
					if ( ( lprp->dn >= (long)( pw.y*LEGACY_STEP_SCALE ) ) && lprp->name ) {   // draw name
						pt.x = pt.y = 0;	n = (int)wcslen( lprp->name );
						GetTextExtentPoint( hdc, lprp->name, n, (LPSIZE)&pt );
						i = (int)( p.y - Dry - D - pt.y + tm.tmDescent );
						if ( i > fdraw.top ) p.y = i; else p.y += Dry;
						if ( p.x + pt.x > fdraw.right ) p.x -= pt.x;
						TextOut( hdc, (int)p.x, (int)p.y, lprp->name, n );
					}
				}
			}
		}
		DeleteObject( SelectObject( hdc, GetStockObject( SYSTEM_FONT ) ) );
		SetBkMode( hdc, OPAQUE );
	}

	// pass #2 - draw mark points
	if ( what_draw & DRAW_MARKERS ) {
		e_fseek( &infile, 0L, SEEK_SET );
		e_fgets( line_buf, sizeof( line_buf ) / sizeof( line_buf[ 0 ] ), &infile );
		trimline( s = bow( line_buf ) );	/* skip title */

		for ( i = 0; !e_feof( &infile ); i++ ) {   // enumerate points
			if ( !e_fgets( line_buf, sizeof( line_buf ) / sizeof( line_buf[ 0 ] ), &infile ) ) continue;
			p.latitude = atogeo( s = bow( line_buf ) );
			if ( !*s ) continue;
			p.longitude = atogeo( s = nextw( s ) );
			v = 1.0;
			npen = 1;
			s = nextw( s );
			while ( *s ) {
				if ( *s == '.' || isdigit( (int)(unsigned char)*s ) ) {
					v = _wtof( s );
					if ( v <= 0.0 ) v = 1.0; else if ( v < 0.1 ) v = 0.1; else if ( v > 10.0 ) v = 10.0;
				} else if ( *s == '^' ) {
					npen = _wtol( s + 1 );
					if ( npen < 0 ) npen = 1;
					else if ( npen > 4 && npen < 11 ) npen = 4;
					else if ( npen > 14 ) npen = 14;
				} else if ( *s == '#' || *s == ';' ) break;
				s = nextw( s );
			}
			g2d( &p, &p );
			/* draw data point on bitmap */
			SelectObject( hdc, GetStockObject( npen > 10 ? LTGRAY_BRUSH : BLACK_BRUSH ) ); // gray or black brush
			SelectObject( hdc, hpen[ npen > 10 ? 1 : 0 ] ); // black or white contour
			switch ( npen ) {
			case 0:
				break;	/* none */
			default:	/* 1,11 = circle */
				Ellipse( hdc, iround( p.x - v * Dmx ), iround( p.y - v * Dmy ), iround( p.x + v * Dmx ), iround( p.y + v * Dmy ) );
				break;
			case 2:	case 12:/* 2,12 = square */
				v *= 0.886227;
				Rectangle( hdc, iround( p.x - v * Dmx ), iround( p.y - v * Dmy ), iround( p.x + v * Dmx ), iround( p.y + v * Dmy ) );
				break;
			case 3: case 13:/* 3 = rhomb */
				{
					POINT	apts[ 4 ];
					v *= 1.4142135623730950488016887242097 * 0.886227;
					apts[ 0 ].x = iround( p.x - v * Dmx );	apts[ 0 ].y = iround( p.y );
					apts[ 1 ].x = iround( p.x );		apts[ 1 ].y = iround( p.y - v * Dmy );
					apts[ 2 ].x = iround( p.x + v * Dmx );	apts[ 2 ].y = iround( p.y );
					apts[ 3 ].x = iround( p.x );		apts[ 3 ].y = iround( p.y + v * Dmy );
					Polygon( hdc, apts, 4 );
				}
				break;
			case 4: case 14:/* 4 = triangle */
				{
					POINT	apts[ 3 ];
					v *= 1.1026578096528985289355763961921;
					apts[ 0 ].x = iround( p.x - v * Dmx );	apts[ 0 ].y = iround( p.y + 2 * 0.2887*v*Dmy );
					apts[ 1 ].x = iround( p.x );		apts[ 1 ].y = iround( p.y - 2 * 0.57735*v*Dmy );
					apts[ 2 ].x = iround( p.x + v * Dmx );	apts[ 2 ].y = iround( p.y + 2 * 0.2887*v*Dmy );
					Polygon( hdc, apts, 3 );
				}
				break;
			}
		}
	}
	SelectObject( hdc, GetStockObject( BLACK_PEN ) );
	for ( i = 0; i < sizeof( hpen ) / sizeof( hpen[ 0 ] ); i++ ) DeleteObject( hpen[ i ] );

	e_fclose( &infile );

	return 1;
}
