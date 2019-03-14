#include "stdafx.h"
#include "geo.h"
#include "e-file.h"

static repfile_P	repfile_root;
static mapfile_P	mapfile_root;

wchar_t	*bow( wchar_t *str )
{
	wchar_t	*p;
	wchar_t	c;

	for ( c = *( p = str ); c && ( c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' ); c = *p ) p++;		/* skip spaces */
	return p;
}


wchar_t	*nextw( wchar_t *str )
{
	wchar_t	*p;
	wchar_t	c;

	for ( c = *( p = str ); c && !( c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' ); c = *p ) p++;

	if ( *p ) {
		*p++ = L'\0';
		p = bow( p );
	}
	return p;
}


void	trimline( wchar_t *str )
{
	wchar_t	*p;
	wchar_t	c;

	p = str + wcslen( str );
	while ( p > str ) {
		--p;
		c = *p;
		if ( !( c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' ) ) { p[ 1 ] = L'\0'; break; }
	}
}


geo_t	atogeo( wchar_t *str )
{
	// returns value in degrees;
	//    source may be xxx.yyyyyy - decimal form, or xxx.yy - degrees and minutes, or xxx.yyyy - degress and seconds
	wchar_t	*p;
	int	neg;
	geo_t	degree, fraction;
	int	n;
	int	c;

	p = bow( str );					/* skip spaces */
	neg = 0; if ( *p == L'-' ) { p++; neg = 1; }	/* check sign */

	degree = fraction = geo_0;
	for ( c = (int)*p++; c >= L'0' && c <= L'9'; c = (int)*p++ ) {
		degree = degree * geo_10 + (geo_t)( c - (int)L'0' );
	}

	if ( c == (int)L'.' || c == (int)L',' ) {
		for ( n = 0, c = (int)*p++; c >= L'0' && c <= L'9'; n++, c = (int)*p++ ) {
			fraction = fraction * geo_10 + (geo_t)( c - (int)L'0' );
		}

		/* convert to degrees */
		if ( n <= 2 ) {				/* minutes */
			fraction /= geo_60;
//		} else if ( n <= 4 ) {			/* seconds */
//			fraction /= geo_3600;
		} else {
			fraction /= (geo_t)pow( 10.0, (double)n );
		}
	}
	degree = (geo_t)fmod( (double)( degree + fraction ), (double)geo_360 );	/* return +- 360 degrees */
	return neg ? -degree : degree;
}


int	iround( double v )
{
	return (int)floor( v + 0.5 );
}


wchar_t	*geotoa( geo_t v )
{
	static int	n = 0;
	static wchar_t	data[ 16 ][ 32 ];

	n %= 16;
	swprintf( data[ n ], 32, L"%.3f", v );
	return (wchar_t*)( data[ n++ ] );
}


wchar_t	*spatialtoa( geo_t x )
{
	static int	n = 0;
	static wchar_t	data[ 16 ][ 32 ];
	double		degrees, minutes, seconds, frac;
	wchar_t		sign;
	wchar_t		*p;

	n %= 16;
	if ( x > geo_180 ) x -= geo_360; else if ( x <= -geo_180 ) x += geo_360;
	if ( x < 0L ) { x = -x; sign = L'-'; } else sign = L'+';

	x = floor( x * 3600000.0 + 0.5 );
	x -= seconds = fmod( x, 60.0*1000.0 );
	x -= minutes = fmod( x, 3600.0*1000.0 );
	degrees = floor( x / 3600.0 / 1000.0 + 0.5 );
	minutes = floor( minutes / 60.0 / 1000.0 + 0.5 );
	seconds = floor( seconds + 0.5 ) / 1000.0;

	p = data[ n ];
	if ( sign == L'-' ) *p++ = L'-';
	if ( seconds < 0.001 ) {
		if ( minutes < 0.1 ) {
			swprintf( p, 32, L"%i%lc", (int)degrees, 176 );
		} else {
			swprintf( p, 32, L"%i%lc%i%lc", (int)degrees, 176, (int)minutes, 145 );
		}
	} else {
		frac = seconds - floor( seconds + 0.5 );
		if ( frac < 0.0 ) frac = -frac;
		if ( frac < 0.001 ) {
			if ( minutes < 0.1 ) {
				swprintf( p, 32, L"%i%lc%i%lc", (int)degrees, 176, (int)seconds, 147 );
			} else {
				swprintf( p, 32, L"%i%lc%i%lc%i%lc", (int)degrees, 176, (int)minutes, 145, (int)seconds, 147 );
			}
		} else {
			if ( minutes < 0.1 ) {
				swprintf( p, 32, L"%i%lc%.3f%lc", (int)degrees, 176, (double)seconds, 147 );
			} else {
				swprintf( p, 32, L"%i%lc%i%lc%.3f%lc", (int)degrees, 176, (int)minutes, 145, seconds, 147 );
			}
		}
	}
	return (wchar_t*)( data[ n++ ] );
}


wchar_t *wcs_place( wchar_t **pbuf, size_t *pcount, wchar_t *src )
{
	wchar_t		*ps = *pbuf;
	size_t		len;

	if ( *src && 0 != *pcount ) {
		len = wcslen( src ) + 1;
		if ( *pcount >= len ) {
			wcscpy( ps, src );
			*pbuf = ps + len;
			*pcount -= len;
		} else {
			len = *pcount;
			if ( len > 1 ) wcsncpy( ps, src, len - 1 );
			*pbuf += len;
			*pcount = 0U;
		}
	} else {
		ps = (wchar_t*)0;
	}
	return ps;

}


int AddRepFile( wchar_t *fname, DWORD encoding )
{
	/* file.rep syntax:
		51.533333	   0.166666	40000	40000	100	London
		55.900000	  -3.066666	40000	40000	70	Edinburg
		latitude	longitude	d-name	d-point	point-size	name

	   where d-name and d-point : maximal grid step (ignore name/point when actual grid step is more than this limit)
	   point-size : percentage of point size (1..100)
	*/
	struct EFILE	ef = { (FILE*)0, CP_OEMCP };
	char		temp_buf[ FILE_BUFFER_SIZE ];
	wchar_t		line_buf[ LINE_BUFFER_SIZE ];
	wchar_t		*p;
	size_t		sz, cnt, len;
	repfile_P	pri;
	reppoint_P	prp;
	wchar_t		*ps;

	if ( !e_fopen( &ef, fname, encoding ) ) { _set_errno( ENOENT ); return 0; }
	e_fsetvbuf( &ef, temp_buf, _IOFBF, sizeof( temp_buf ) );

	/* first pass - determine size of structures */
	sz = sizeof( repfile_t );
	cnt = 0;
	while ( !e_feof( &ef ) ) {
		if ( (wchar_t*)0 == e_fgets( line_buf, sizeof( line_buf ) / sizeof( line_buf[ 0 ] ), &ef ) ) {
			if ( !e_feof( &ef ) ) cnt = 0;
			break;
		}
		p = bow( line_buf );	/* latitude */
		p = nextw( p );		/* longitude */
		p = nextw( p );		/* dn */
		p = nextw( p );		/* dp */
		p = nextw( p );		/* rsize */
		if ( L'\0' == *p ) continue;	/* skip strange line */
		p = nextw( p );		/* name */
		trimline( p );

		if ( *p ) {
			sz += sizeof( reppoint_t ) + ( wcslen( p ) + 1 ) * sizeof( *p );
		} else {
			sz += sizeof( reppoint_t ) + 0;
		}
		cnt++;
	}
	if ( !cnt ) { e_fclose( &ef ); _set_errno( EINVAL ); return 0; }

	/* second pass - allocate memory and load data */
	pri = (repfile_P)malloc( sz );
	if ( !pri ) { e_fclose( &ef ); _set_errno( ENOMEM ); return 0; }

	pri->next = repfile_root;
	pri->points = cnt;
	pri->dp = 0L;
	prp = (reppoint_P)( pri + 1 );
	ps = (wchar_t*)( prp + cnt );
	sz -= sizeof( repfile_t ) + cnt * sizeof( reppoint_t );
	sz /= sizeof( wchar_t );

	e_fseek( &ef, 0L, SEEK_SET );
	while ( !e_feof( &ef ) ) {
		if ( (wchar_t*)0 == e_fgets( line_buf, sizeof( line_buf ) / sizeof( line_buf[ 0 ] ), &ef ) ) {
			if ( e_feof( &ef ) ) break;
			e_fclose( &ef );
			free( pri );
			_set_errno( EINVAL );
			return 0;
		}
		prp->pt.latitude = atogeo( p = bow( line_buf ) );	/* latitude */
		prp->pt.longitude = atogeo( p = nextw( p ) );	/* longitude */
		prp->dn = _wtol( p = nextw( p ) );			/* dn */
		prp->dp = _wtol( p = nextw( p ) );			/* dp */
		prp->rsize = _wtol( p = nextw( p ) );			/* rsize */
		if ( '\0' == *p ) continue;	/* skip strange line */
		p = nextw( p );					/* name */
		trimline( p );
		prp->name = wcs_place( &ps, &sz, p );
		if ( prp->dp > pri->dp ) pri->dp = prp->dp;
		prp++;
		--cnt;
	}
	repfile_root = pri;
	e_fclose( &ef );

	return 1;
}


void FreeAllRepFiles( void )
{
	repfile_P   next;

	while ( repfile_root ) {
		next = repfile_root->next;
		free( (void*)repfile_root );
		repfile_root = next;
	}
}


static void FreeArc( mapfile_P pmf )
{
	geopoint_P	ppt;
	size_t		i;

	if ( pmf ) {
		for ( i = 0L; i < pmf->outlines; i++ ) {
			ppt = pmf->outline[ i ].point;
			if ( ppt ) free( ppt );
		}
		free( pmf );
	}
}



static outlinesegment_P	addinfo0;
static void split_outline( outlinesegment_P parent, size_t points, geopoint_P ppt )
{
	size_t			i, j, n, max;
	size_t			pps;     // point per segment
	size_t			rest;    // undispatched points
	outlinesegment_P	lpa, lpn;
	geo_t			W, E, S, N, x, y;

	/* dispatch points per segments */
	if ( parent ) {
		points = parent->last - ( n = parent->first ) + 1;
		parent->next = addinfo0;
	} else {
		n = 0;
	}

	/* allocate SEGMENTMIN structures */
	lpa = addinfo0;   addinfo0 += SPLITPOW;

	if ( points > SEGMENTMIN ) {
		pps = points / SPLITPOW;
		rest = points % SPLITPOW;
		max = SPLITPOW;
	} else {
		pps = points;
		rest = 0;
		max = 1;
	}

	for ( i = 0; i < SPLITPOW; i++ ) {
		lpa[ i ].next = (outlinesegment_P)0;
		if ( i >= max ) {
			lpa[ i ].first = lpa[ i ].last = 0;
			lpa[ i ].area.west = lpa[ i ].area.east = lpa[ i ].area.north = lpa[ i ].area.south = 0;
			continue;
		}
		lpa[ i ].first = n;
		n += pps; if ( rest ) { n++; --rest; } if ( !i ) --n;
		lpa[ i ].last = n;
		if ( lpa[ i ].last - lpa[ i ].first + 1 > SEGMENTMIN ) {
			// node... split and get min & max values
			split_outline( lpa + i, 0, ppt );
			lpn = lpa[ i ].next;
			W = lpn[ 0 ].area.west;   E = lpn[ 0 ].area.east;   S = lpn[ 0 ].area.south;  N = lpn[ 0 ].area.north;
			for ( j = 1; j < SPLITPOW; j++ ) {
				if ( W > lpn[ j ].area.west ) W = lpn[ j ].area.west;
				if ( E < lpn[ j ].area.east ) E = lpn[ j ].area.east;
				if ( S > lpn[ j ].area.south ) S = lpn[ j ].area.south;
				if ( N < lpn[ j ].area.north ) N = lpn[ j ].area.north;
			}
		} else {
			// leaf... get max & min values
			j = lpa[ i ].first;
			W = E = ppt[ j ].longitude;	N = S = ppt[ j ].latitude;
			for ( j++; j <= lpa[ i ].last; j++ ) {
				x = ppt[ j ].longitude;    y = ppt[ j ].latitude;
				if ( W > x ) W = x; else if ( E < x ) E = x;
				if ( S > y ) S = y; else if ( N < y ) N = y;
			}
		}
		lpa[ i ].area.west = W;   lpa[ i ].area.east = E;   lpa[ i ].area.north = N;  lpa[ i ].area.south = S;
	}
}


int AddArcFile( wchar_t *fname, UINT encoding, add_arc_callback_t cbproc, int cbrow, ... )
{
	struct EFILE		ef = { (FILE*)0, CP_OEMCP };
	char			temp_buf[ FILE_BUFFER_SIZE ];
	wchar_t			line_buf[ LINE_BUFFER_SIZE ];
	wchar_t			txt[ LINE_BUFFER_SIZE * 4 ];
	wchar_t			title[ MAP_TITLE_LENGTH ];
	errno_t			ecode = (errno_t)0;
	size_t			outlines, points;
	va_list			ap;
	mapfile_P		pmf = (mapfile_P)0;
	geopoint_P		ppt = (geopoint_P)0;
	geo_t			W, E, N, S;
	wchar_t			*p;
	size_t			i, j, n, segments;

	va_start( ap, cbrow );
	if ( !e_fopen( &ef, fname, encoding ) ) { ecode = ENOENT; goto ONERROR; }
	e_fsetvbuf( &ef, temp_buf, _IOFBF, sizeof( temp_buf ) );

	/* read title */
	if ( (wchar_t*)0 == e_fgets( line_buf, sizeof( line_buf ) / sizeof( line_buf[ 0 ] ), &ef ) ) { ecode = EINVAL; goto ONERROR; }

	trimline( p = bow( line_buf ) );	
	wcsncpy( title, p, sizeof( title ) / sizeof( title[ 0 ] ) );	
	title[ sizeof( title ) / sizeof( title[ 0 ] ) - 1 ] = L'\0';

	/* read count of outlines */
	if ( (wchar_t*)0 == e_fgets( line_buf, sizeof( line_buf ) / sizeof( line_buf[ 0 ] ), &ef ) ) { ecode = EINVAL; goto ONERROR; }
	trimline( p = bow( line_buf ) );	outlines = (size_t)_wtol( p );
	if ( !outlines ) { ecode = EINVAL; goto ONERROR; }

	/* allocate memory */
	pmf = (mapfile_P)malloc( sizeof( mapfile_t ) + sizeof( mapoutline_t )*outlines );	/* reserve one extra mapoutline_t */
	if ( !pmf ) { ecode = ENOMEM; goto ONERROR; }
	wcsncpy( pmf->title, title, sizeof( pmf->title ) / sizeof( pmf->title[ 0 ] ) - 1 );
	pmf->title[ sizeof( pmf->title ) / sizeof( pmf->title[ 0 ] ) - 1 ] = '\0';
	pmf->outlines = outlines;

	/* load outlines */
	for ( n = 0; n < outlines; n++ ) {
		/* read couunt of points and outline's name */
		if ( (wchar_t*)0 == e_fgets( line_buf, sizeof( line_buf ) / sizeof( line_buf[ 0 ] ), &ef ) ) {
			if ( !e_feof( &ef ) ) {
				ecode = EINVAL;
				goto ONERROR;
			} else break;
		}
		points = (size_t)_wtol( p = bow( line_buf ) );

		for ( segments = 0L, i = 1, j = points; j > SEGMENTMIN; j = (size_t)( ( j%SPLITPOW ? 2 : 1 ) + ( j / SPLITPOW ) ) ) {
			segments += (size_t)( i *= SPLITPOW );
		}
		if ( !segments ) segments = SPLITPOW;

		ppt = (geopoint_P)malloc( sizeof( geopoint_t ) * points + sizeof( outlinesegment_t ) * ( segments + 1 ) );
		if ( !ppt ) { ecode = ENOMEM; goto ONERROR; }

		p = nextw( p );
		if ( p[ 0 ] == L'^' ) {
			pmf->outline[ n ].penstyle = _wtoi( p + 1 );
			p = nextw( p );
			if ( pmf->outline[ n ].penstyle < 0 ) pmf->outline[ n ].penstyle = 0;
			if ( pmf->outline[ n ].penstyle > 12 ) pmf->outline[ n ].penstyle = 1;
		} else {
			pmf->outline[ n ].penstyle = 1;
		}
		trimline( p );
		wcsncpy( pmf->outline[ n ].title, p, sizeof( pmf->outline[ n ].title ) / sizeof( pmf->outline[ n ].title[ 0 ] ) );
		pmf->outline[ n ].title[ sizeof( pmf->outline[ n ].title ) / sizeof( pmf->outline[ n ].title[ 0 ] ) - 1 ] = L'\0';
		pmf->outline[ n ].points = points;
		pmf->outline[ n ].point = ppt;

		/* load points */
		for ( i = 0; i < points; i++ ) {
			if ( (wchar_t*)0 == e_fgets( line_buf, sizeof( line_buf ) / sizeof( line_buf[ 0 ] ), &ef ) ) { ecode = EINVAL; goto ONERROR; }
			p = bow( line_buf );
			ppt[ i ].latitude = atogeo( p );
			ppt[ i ].longitude = atogeo( nextw( p ) );
		}

		/* split for segments and fill additional information */
		addinfo0 = (outlinesegment_P)( ppt + points );
		split_outline( (outlinesegment_P)0, points, ppt );

		/* get spatial sizes */
		addinfo0 = (outlinesegment_P)( ppt + points );
		W = E = S = N = geo_0;
		for ( i = 0; i < SPLITPOW; i++ ) {
			if ( addinfo0[ i ].last == 0 ) continue;
			if ( !i ) {
				W = addinfo0[ i ].area.west; E = addinfo0[ i ].area.east;
				S = addinfo0[ i ].area.south;   N = addinfo0[ i ].area.north;
			} else {
				if ( W > addinfo0[ i ].area.west ) W = addinfo0[ i ].area.west;
				if ( E < addinfo0[ i ].area.east ) E = addinfo0[ i ].area.east;
				if ( S > addinfo0[ i ].area.south ) S = addinfo0[ i ].area.south;
				if ( N < addinfo0[ i ].area.north ) N = addinfo0[ i ].area.north;
			}
		}

		pmf->outline[ n ].area.west = W;	pmf->outline[ n ].area.east = E;
		pmf->outline[ n ].area.south = S;	pmf->outline[ n ].area.north = N;
		pmf->outline[ n ].area.width = W < E ? E - W : W - E;
		pmf->outline[ n ].area.height = S < N ? N - S : S - N;
		swprintf(
			txt, sizeof( txt ) / sizeof( txt[ 0 ] ),
			L"   %ls:%ls с.ш. %ls:%ls в.д., %lu точек (%ls)",
			geotoa( S ), geotoa( N ), geotoa( W ), geotoa( E ), (LONG)points,
			pmf->outline[ n ].title
		);
		cbproc( txt, cbrow + n + 1, ap );

		if ( !n ) {
			pmf->area.west = W;
			pmf->area.east = E;
			pmf->area.south = S;
			pmf->area.north = N;
		} else {
			if ( W < pmf->area.west ) pmf->area.west = W;
			if ( E > pmf->area.east ) pmf->area.east = E;
			if ( S < pmf->area.south ) pmf->area.south = S;
			if ( N > pmf->area.north ) pmf->area.north = N;
		}
		pmf->area.width = pmf->area.east - pmf->area.west;
		if ( pmf->area.width < geo_0 ) pmf->area.width = -pmf->area.width;
		pmf->area.height = pmf->area.north - pmf->area.south;
		if ( pmf->area.height < geo_0 ) pmf->area.height = -pmf->area.height;
		ppt = (geopoint_P)0;
	}
	swprintf(
		txt, sizeof( txt ) / sizeof( txt[ 0 ] ),
		L"%ls:%ls с.ш. %ls:%ls в.д., %lu контуров (%ls)",
		geotoa( pmf->area.south ), geotoa( pmf->area.north ),
		geotoa( pmf->area.west ), geotoa( pmf->area.east ),
		outlines, pmf->title
	);
	cbproc( txt, cbrow, ap );

	pmf->next = mapfile_root;
	mapfile_root = pmf;

ONERROR:
	if ( ppt ) free( ppt );
	e_fclose( &ef );
	if ( ecode ) {
		if ( pmf ) FreeArc( pmf );
		_set_errno( ecode );
	}
	va_end( ap );
	return ecode ? 0 : 1;
}


void FreeAllArcFiles( void )
{
	mapfile_P   next;

	while ( mapfile_root ) {
		next = mapfile_root->next;
		FreeArc( mapfile_root );
		mapfile_root = next;
	}
}


int	IsMapFileCreated( void )
{
	return mapfile_root ? 1 : 0;
}

mapfile_P GetRootArc( void )
{
	return mapfile_root;
}


repfile_P GetRootRep( void )
{
	return repfile_root;
}
