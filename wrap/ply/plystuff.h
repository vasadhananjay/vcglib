/****************************************************************************
* VCGLib                                                            o o     *
* Visual and Computer Graphics Library                            o     o   *
*                                                                _   O  _   *
* Copyright(C) 2004                                                \/)\/    *
* Visual Computing Lab                                            /\/|      *
* ISTI - Italian National Research Council                           |      *
*                                                                    \      *
* All rights reserved.                                                      *
*                                                                           *
* This program is free software; you can redistribute it and/or modify      *   
* it under the terms of the GNU General Public License as published by      *
* the Free Software Foundation; either version 2 of the License, or         *
* (at your option) any later version.                                       *
*                                                                           *
* This program is distributed in the hope that it will be useful,           *
* but WITHOUT ANY WARRANTY; without even the implied warranty of            *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
* GNU General Public License (http://www.gnu.org/licenses/gpl.txt)          *
* for more details.                                                         *
*                                                                           *
****************************************************************************/
/****************************************************************************
Acknowlegments
Portions of this file were based on the original code of the Ply library 
of Greg Turk and on the work of Claudio Rocchini

****************************************************************************/

/****************************************************************************
  History

$Log: not supported by cvs2svn $
Revision 1.2  2005/03/15 11:46:52  cignoni
Cleaning of the automatic bbox caching support for ply files. First working version.


*/

//****************** Gestione cache *****************

#ifndef __VCG_PLYLIB_STUFF
#define __VCG_PLYLIB_STUFF 

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <io.h>

#include <vcg/space/box3.h>
#include <wrap/ply/plylib.h>
using namespace vcg;

#ifdef WIN32
#include <direct.h>
#define pb_mkdir(n)  _mkdir(n)
#define pb_access _access
#define pb_stat   _stat
#define pb_fstat   _fstat
#define pb_open   _open
#define pb_close  _close
#else
#define pb_mkdir(n)  mkdir(n,0)
#define pb_access access
#define pb_stat   stat
#define pb_fstat   fstat
#define pb_open   open
#define pb_close  close
#endif


namespace vcg {
namespace ply {

const int MAXBPATH = 256;

	// Stringhe per la cache
const char * cachedir = "vcg_cache";
const char * bboxcacheext = ".bbox_cache";
const char * bboxheader = "BBOXCACH";

static bool GetDirFromPath( const char * path, char * dir, char * name )
{
	strcpy(dir,path);
	char * p;
	
	p = strrchr(dir,'\\');
	if(p==0) p=strrchr(dir,'/');
	if(p==0)
	{
		dir[0] = 0;
		strcpy(name,path);
	}
	else
	{
		strcpy(name,p+1);
		*p = 0;
	}
	return true;
}


static bool CheckCacheDirectory( const char * dir )
{
	if( pb_access(dir,0)!=0 )
	{
		if( pb_mkdir(dir)==-1 )
			return false;
	}
	return true;
}


bool CheckCacheTime( const char * fname, const char * cname )
{

	if( pb_access(fname,4)==-1 ) return false;
	if( pb_access(cname,4)==-1 ) return false;

	int h,r;
	struct pb_stat st;
	time_t ft,bt;

	h = pb_open(fname,_O_BINARY|_O_RDONLY);
	if(h==0) return false;
	r = pb_fstat(h,&st);
	pb_close(h);
	if(r==-1) return false;
	ft = st.st_mtime;

	h = pb_open(cname,_O_BINARY|_O_RDONLY);
	if(h==0) return false;
	r = pb_fstat(h,&st);
	//_read(h,&box,sizeof(box));
	pb_close(h);
	if(r==-1) return false;
	bt = st.st_mtime;

	if( difftime(bt,ft)>=0 ) return true;
	else			         return false;
}

// restituisce true se il file con la cache del bbox della mesh e' piu' recente del file ply
// se fname2 != 0, allora deve essere piu recente anche di fname2.
static bool CheckBBoxCache( const char * fname, Box3d & box, const char *fname2=0 )
{
	char d[MAXBPATH];
	char n[MAXBPATH];
	char h[8];

		// Estrazione dati
	if( ! GetDirFromPath(fname,d,n) ) return false;

		// Controllo esistenza directory delle cache
	if(d[0]!=0)
		strcat(d,"\\");
	strcat(d,cachedir);
	if( !CheckCacheDirectory(d) ) return false;

		// Controllo esistenza e data file cache
	strcat(d,"\\");
	strcat(d,n);
	strcat(d,bboxcacheext);
	if( CheckCacheTime(fname,d)  &&
		 (fname2==0 || CheckCacheTime(fname2,d)) )
	{
			// Lettura bbox e controllo
		FILE * fp = fopen(d,"rb");
		if(fp==0) return false;
		if( fread(h,1,8,fp)!=8 )
		{
			fclose(fp);
			return false;
		}
		if( fread(&box,sizeof(Box3d),1,fp)!=1 )
		{
			fclose(fp);
			return false;
		}
		fclose(fp);
		if( strncmp(h,bboxheader,8) )
			return false;
		else
			return true;
	}
	else
		return false;
}


bool GetCacheName( const char * fname, const char * ext_name, char * cname )
{
	static char n[MAXBPATH];

		// Estrazione dati
	if( ! GetDirFromPath(fname,cname,n) ) return false;

		// Controllo esistenza directory delle cache
	if(cname[0]!=0)
		strcat(cname,"\\");
	strcat(cname,cachedir);
	if( !CheckCacheDirectory(cname) ) return false;

	strcat(cname,"\\");
	strcat(cname,n);
	strcat(cname,ext_name);
	return true;
}


static bool SaveBBoxCache( const char * fname, const Box3d & box )
{
	char d[MAXBPATH];

	if( !GetCacheName(fname,bboxcacheext,d) )
		return false;

		// Lettura bbox e controllo
	FILE * fp = fopen(d,"wb");
	if(fp==0) return false;
	if( fwrite(bboxheader,1,8,fp)!=8 )
	{
		fclose(fp);
		return false;
	}
	if( fwrite(&box,sizeof(Box3d),1,fp)!=1 )
	{
		fclose(fp);
		return false;
	}
	fclose(fp);
	return true;
}

struct PlyPoint3d
{
	double x;
	double y;
	double z;
};


	// Calcola il bbox di un file ply

bool ScanBBox( const char * fname, Box3d & box, bool use_cache=true )
{

	if(use_cache)
	{
		if( CheckBBoxCache(fname,box) )
			return true;
	}

	static const PropDescriptor pv[3]=
	{
		{"vertex","x",T_FLOAT,T_DOUBLE,offsetof(PlyPoint3d,x),0,0,0,0,0},
		{"vertex","y",T_FLOAT,T_DOUBLE,offsetof(PlyPoint3d,y),0,0,0,0,0},
		{"vertex","z",T_FLOAT,T_DOUBLE,offsetof(PlyPoint3d,z),0,0,0,0,0},
	};


	PlyFile pf;

	if( pf.Open(fname,PlyFile::MODE_READ)==-1 )
	{
		fprintf(stderr,"Warning: File %s not found\n",fname);
		return false;
	}

	if( pf.AddToRead(pv[0])==-1 ) { fprintf(stderr,"Warning: Read error\n"); return false; }
	if( pf.AddToRead(pv[1])==-1 ) { fprintf(stderr,"Warning: Read error\n"); return false; }
	if( pf.AddToRead(pv[2])==-1 ) { fprintf(stderr,"Warning: Read error\n"); return false; }

	box.SetNull();
	char dummyspace[1024]; // sperando basti...

	for(int i=0;i<int(pf.elements.size());++i)
	{
		int n = pf.ElemNumber(i);
		pf.SetCurElement(i);
			
		if( !strcmp( pf.ElemName(i),"vertex" ) )
		{
			for(int j=0;j<n;++j)
			{
				PlyPoint3d t;

				pf.Read( (void *)(&t) );
				box.Add( Point3d(t.x,t.y,t.z) );
			}
		}
		else
		{
			for(int j=0;j<n;++j)
				//pf.Read( 0 ); // prima era cosi' e faceva un'assert e scrivema plausibilimente a caso in mem
				pf.Read( dummyspace );
		}
	}

	if(use_cache)
	{
		SaveBBoxCache(fname,box);
	}

	return true;
}

// Come la precedente ma applica la matrice m ai punti prima di calcolare il bbox.
// Visto che la matrice di solito e' tenuta in un qualche file, se si vuole usare la cache 
// si puo' passare anche un'altro filename da controllare
bool ScanBBox( const char * fname, Box3d & box, const Matrix44d & m, bool use_cache, const char *matrixfname)
{

	if(use_cache)
	{
		if ( CheckBBoxCache(fname,box,matrixfname) ) return true;
	}

	static const PropDescriptor pv[3]=
	{
		{"vertex","x",T_FLOAT,T_DOUBLE,offsetof(PlyPoint3d,x),0,0,0,0,0},
		{"vertex","y",T_FLOAT,T_DOUBLE,offsetof(PlyPoint3d,y),0,0,0,0,0},
		{"vertex","z",T_FLOAT,T_DOUBLE,offsetof(PlyPoint3d,z),0,0,0,0,0},
	};


	PlyFile pf;

	if( pf.Open(fname,PlyFile::MODE_READ)==-1 )
	{
		fprintf(stderr,"Warning: File %s not found\n",fname);
		return false;
	}

	if( pf.AddToRead(pv[0])==-1 ) { fprintf(stderr,"Warning: Read error\n"); return false; }
	if( pf.AddToRead(pv[1])==-1 ) { fprintf(stderr,"Warning: Read error\n"); return false; }
	if( pf.AddToRead(pv[2])==-1 ) { fprintf(stderr,"Warning: Read error\n"); return false; }

	box.SetNull();
	char dummyspace[1024]; // sperando basti...

	for(int i=0;i<int(pf.elements.size());++i)
	{
		int n = pf.ElemNumber(i);
		pf.SetCurElement(i);
			
		if( !strcmp( pf.ElemName(i),"vertex" ) )
		{
			for(int j=0;j<n;++j)
			{
				PlyPoint3d t;

				pf.Read( (void *)(&t) );
				box.Add( m*Point3d(t.x,t.y,t.z) );
			}
		}
		else
		{
			for(int j=0;j<n;++j)
				//pf.Read( 0 ); // prima era cosi' e faceva un'assert e scrivema plausibilimente a caso in mem
				pf.Read( dummyspace );
		}
	}

	if(use_cache)
	{
		SaveBBoxCache(fname,box);
	}

	return true;
}

void __interpret_texture_name(const char*a, const char*fn, char*output){
	int ia=0,io=0;
	output[0]=0;
	while (a[ia]!=0){
		if (a[ia]=='<') {
			if (strlen(a)>ia+5) {
				if ( ( (a[ia+1]=='t') || (a[ia+1]=='T') ) &&
						 ( (a[ia+2]=='h') || (a[ia+2]=='H') ) &&
						 ( (a[ia+3]=='i') || (a[ia+3]=='I') ) &&
						 ( (a[ia+4]=='s') || (a[ia+4]=='S') ) &&
						 ( a[ia+5]=='>' ) )
				{
					// substitute "<this>" with filename:
					// 1) remove path from filename 
					int lastbar=0;
					int ifn=0;
					while (fn[ifn]!=0) { if ((fn[ifn]=='/') || (fn[ifn]=='\\')) lastbar=ifn+1; ifn++;}
					ifn=lastbar;
					char fn2[255];
					while (fn[ifn]!=0) { fn2[ifn-lastbar]=fn[ifn]; ifn++;}
					fn2[ifn-lastbar]=0;

					// 2) remove ".ply" extention from filename
					int l=ifn-lastbar;
					if ((fn2[l-4]=='.') 
						&& ((fn2[l-3]=='P') || (fn2[l-3]=='p')) 
						&& ((fn2[l-2]=='L') || (fn2[l-2]=='l')) 
						&& ((fn2[l-1]=='Y') || (fn2[l-1]=='y')) )
					fn2[l-4]=0;

					// 3) append
					output[io]=0;
					sprintf(output,"%s%s",output,fn2);
					io=strlen(output);
					ia+=6; //skip the "<this>"
					continue;
				};
			}
		}
		output[io++]=a[ia++]; 
	};
	output[io]=0;
};

}
}
#endif

