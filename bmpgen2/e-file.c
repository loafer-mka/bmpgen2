#include "stdafx.h"
#include "e-file.h"

int e_fopen( struct EFILE *ep, wchar_t *fname, UINT encoding )
{
	wchar_t		*mode;

	switch ( encoding ) {
	default:    // CP_OEMCP
	case 2:	    // CP_ACP
		mode = L"rb";
		break;
	case CP_UTF8:	    // UTF8
		mode = L"rt, ccs=UTF-8";
		break;
	}
	ep->fp = _wfopen( fname, mode );
	ep->encoding = encoding;
	return NULL == ep->fp ? 0 : 1;
}

wchar_t *e_fgets( wchar_t *buf, UINT size, struct EFILE *ep )
{
	char	temp[ LINE_BUFFER_SIZE ];
	char	*p;
	size_t	sz;

	// MultiByteToWideChar( CP_ACP|CP_OEMCP|CP_UTF8, ... )
	switch ( ep->encoding ) {
	case CP_UTF8:
		// read unicode string
		return fgetws( buf, size, ep->fp );
	default:
		break;
	}
	sz = size;
	if ( sz >= sizeof( temp ) ) sz = sizeof( temp );
	p = fgets( temp, sz, ep->fp );
	if ( !p ) return NULL;
	temp[ sz - 1 ] = '\0';
	sz = strlen( temp ) + 1;
	if ( !MultiByteToWideChar( ep->encoding, 0, temp, sz, buf, size ) ) {
		volatile DWORD	dw = GetLastError();
	}
	return buf;
}

void e_fclose( struct EFILE *ep )
{
	if ( NULL != ep->fp ) fclose( ep->fp );
	ep->fp = NULL;
}

int e_fsetvbuf( struct EFILE *ep, char* buffer, int mode, size_t size )
{
	return NULL == ep->fp ? -1 : ( ep->fp, buffer, mode, size );
}

int e_feof( struct EFILE *ep )
{
	return NULL == ep->fp ? 1 : feof( ep->fp );
}

long e_fseek( struct EFILE *ep, long pos, int mode )
{
	return NULL == ep->fp ? 0 : fseek( ep->fp, pos, mode );
}