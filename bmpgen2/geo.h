#ifndef _WIN32_WINNT
#	define _WIN32_WINNT	0x0500
#endif
#include <stdarg.h>

typedef double	geo_t;
#define		geo_0		((geo_t)0.0)
#define		geo_1		((geo_t)1.0)
#define		geo_10		((geo_t)10.0)
#define		geo_60		((geo_t)60.0)
#define		geo_90		((geo_t)90.0)
#define		geo_180		((geo_t)180.0)
#define		geo_360		((geo_t)360.0)
#define		geo_3600	((geo_t)3600.0)

#define MAP_TITLE_LENGTH	512
#define FILE_BUFFER_SIZE	4096

#pragma warning(disable:4201)
typedef struct geopoint_s {
	union {
		struct { geo_t longitude, latitude; };
		struct { geo_t x, y; };
	};
} geopoint_t, *geopoint_P;


typedef struct geoarea_s {
	union {
		struct { geo_t west, south, east, north; };
		struct { geo_t left, bottom, right, top; };
		struct { geopoint_t SW, NE; };
	};
	geo_t	width, height;
} geoarea_t, *geoarea_P;
#pragma warning(default:4201)

struct outlinesegment_s;
typedef struct outlinesegment_s {
	geoarea_t			area;
	size_t				first, last;	/* first and last point of segment */
	struct outlinesegment_s*	next;		/* next level */
} outlinesegment_t, *outlinesegment_P;

#define  SEGMENTMIN	72
#define  SPLITPOW	4
/*
 if outline is less than SEGMENTMIN points, then outlinesegment_t is not included;
 single outline is splitted upto SPLITPOW segments, each is described as outlinesegment_t item;
 when each outlinesegment_t has more than SEGMENTMIN points (last-first+1), then it is splitted again, etc.

 when drawing we look for (w,e,s,n) of outlinesegment_t; if outbounds, then omit, if overbounds, then
 analyse; we will draw entire outlinesegment_t when all SPLITPOW derived outlinesegment_t are overbounds;
*/

typedef struct mapoutline_s {
	wchar_t		title[ MAP_TITLE_LENGTH ];
	geoarea_t	area;
	int		penstyle;
	size_t		points;
	geopoint_P	point;
} mapoutline_t, *mapoutline_P;

struct mapfile_s;
typedef struct mapfile_s {
	struct mapfile_s*	next;
	wchar_t			title[ MAP_TITLE_LENGTH ];
	geoarea_t		area;
	size_t			outlines;
	mapoutline_t		outline[ 1 ];
} mapfile_t, *mapfile_P;


struct repfile_s;
typedef struct repfile_s {
	struct repfile_s*	next;
	size_t			points;
	long			dp;      /* maximal grid step (ignore entire file, when grid step is greather than d) */
} repfile_t, *repfile_P;


typedef struct reppoint_s {
	geopoint_t	pt;	/* coordinates */
	long		dn, dp;	/* maximal grid step (ignore name/point, when grid step is greather than d?) */
	long		rsize;	/* relative diameter 0...100 percents */
	wchar_t		*name;
} reppoint_t, *reppoint_P;

typedef void( *add_arc_callback_t )( wchar_t *str, int row, va_list ap );

wchar_t		*bow( wchar_t *str );
wchar_t		*nextw( wchar_t *str );
void		trimline( wchar_t *str );
int		iround( double );
geo_t		atogeo( wchar_t *str );
wchar_t		*geotoa( geo_t v );
wchar_t		*spatialtoa( geo_t v );

int		AddRepFile( wchar_t *filename, DWORD encoding );
void		FreeAllRepFiles( void );
repfile_P	GetRootRep( void );

int		AddArcFile( wchar_t *fname, UINT encoding, add_arc_callback_t cbproc, int cbrow, ... );
void		FreeAllArcFiles( void );
int		IsMapFileCreated( void );
mapfile_P	GetRootArc( void );
