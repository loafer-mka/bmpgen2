#pragma once

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <Windows.h>

#define LINE_BUFFER_SIZE	1024

struct EFILE {
	FILE    *fp;
	UINT   encoding;
};

int		e_fopen( struct EFILE *ep, wchar_t *fname, UINT encoding );
wchar_t*	e_fgets( wchar_t *buf, UINT size, struct EFILE *ep );
int		e_fsetvbuf( struct EFILE *ep, char* buffer, int mode, size_t size );
long		e_fseek( struct EFILE *ep, long pos, int mode );
int		e_feof( struct EFILE *ep );
void		e_fclose( struct EFILE *ep );
