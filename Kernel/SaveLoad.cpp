/* This file is part of the FaCT++ DL reasoner
Copyright (C) 2008 by Dmitry Tsarkov

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

//-------------------------------------------------------
//-- Saving/restoring internal state of the FaCT++
//-------------------------------------------------------

#include <istream>

#include "eFPPSaveLoad.h"
#include "Kernel.h"

const char* ReasoningKernel :: InternalStateFileHeader = "FaCT++InternalStateDump1.0";

const int bytesInInt = sizeof(int);

static inline
void expectChar ( istream& i, const char C )
{
	char c;
	i >> c;
	if ( c != C )
		throw EFPPSaveLoad(C);
}

#if 0
// FIXME!! try to avoid recursion later on
static inline
void saveUIntAux ( ostream& o, unsigned int n, const int rest )
{
	if ( rest > 1 )
		saveUIntAux ( o, n/256, rest-1 );
	o << (unsigned char)n%256;
}

static inline
void saveUInt ( ostream& o, unsigned int n )
{
	saveUIntAux ( o, n, bytesInInt );
}

static inline
void saveSInt ( ostream& o, int n ) { saveUInt(o,n); }

static inline
unsigned int loadUInt ( istream& i )
{
	static unsigned int ret = 0;
	unsigned char byte;
	for ( int j = bytesInInt-1; j >= 0; --j )
	{
		i >> byte;
		ret *= 256;
		ret += byte;
	}
	return ret;
}

static inline
int loadSInt ( istream& i ) { return (int)loadUInt(i); }
#else
static inline
void saveUInt ( ostream& o, unsigned int n )
{
	o << "(" << n << ")";
}

static inline
void saveSInt ( ostream& o, int n )
{
	o << "(" << n << ")";
}

static inline
unsigned int loadUInt ( istream& i )
{
	unsigned int ret;
	expectChar(i,'(');
	i >> ret;
	expectChar(i,')');
	return ret;
}

static inline
int loadSInt ( istream& i )
{
	int ret;
	expectChar(i,'(');
	i >> ret;
	expectChar(i,')');
	return ret;
}

#endif	// 0
//----------------------------------------------------------
//-- Implementation of the Kernel methods (Kernel.h)
//----------------------------------------------------------

#undef CHECK_FILE_STATE
#define CHECK_FILE_STATE() if ( !o.good() ) throw(EFPPSaveLoad(name,/*save=*/true))

void
ReasoningKernel :: Save ( const char* name ) const
{
	std::ofstream o(name);
	CHECK_FILE_STATE();
	SaveHeader(o);
	CHECK_FILE_STATE();
	SaveOptions(o);
	CHECK_FILE_STATE();
	SaveKB(o);
	CHECK_FILE_STATE();
}

#undef CHECK_FILE_STATE
#define CHECK_FILE_STATE() if ( !i.good() ) throw(EFPPSaveLoad(name,/*save=*/false))

void
ReasoningKernel :: Load ( const char* name )
{
	std::ifstream i(name);
	CHECK_FILE_STATE();
	releaseKB();	// we'll start a new one if necessary
	if ( LoadHeader(i) )
		throw(EFPPSaveLoad(name,/*save=*/false));
	CHECK_FILE_STATE();
	LoadOptions(i);
	CHECK_FILE_STATE();
	LoadKB(i);
	CHECK_FILE_STATE();
}

//-- save/load header (Kernel.h)

void
ReasoningKernel :: SaveHeader ( ostream& o ) const
{
	o << InternalStateFileHeader << "\n" << Version << "\n" << bytesInInt << "\n";
}

bool
ReasoningKernel :: LoadHeader ( istream& i )
{
	string str;
	i >> str;
	if ( str != InternalStateFileHeader )
		return true;
	i >> str;
	// FIXME!! we don't check version equivalence for now
//	if ( str != Version )
//		return true;
	int n;
	i >> n;
	if ( n != bytesInInt )
		return true;
	return false;
}

//-- save/load options (Kernel.h)

void
ReasoningKernel :: SaveOptions ( ostream& o ) const
{
	o << "Options\n";
}

void
ReasoningKernel :: LoadOptions ( istream& i )
{
	std::string options;
	i >> options;
}

//-- save/load KB (Kernel.h)

void
ReasoningKernel :: SaveKB ( ostream& o ) const
{
	saveUInt(o,(unsigned int)getStatus());
	switch ( getStatus() )
	{
	case kbEmpty:	// nothing to do
		return;
	case kbLoading:
		throw EFPPSaveLoad("Can't load internal state of the unclassified reasoner");
	default:
		getTBox()->Save(o);
		break;
	}
}

void
ReasoningKernel :: LoadKB ( istream& i )
{
	KBStatus status = (KBStatus)loadUInt(i);
	initCacheAndFlags();	// will be done
	if ( status == kbEmpty )
		return;
	newKB();
	getTBox()->Load(i,status);
}

//----------------------------------------------------------
//-- Implementation of the TBox methods (dlTBox.h)
//----------------------------------------------------------

void
TBox :: Save ( ostream& o ) const
{
	o << "\nC";
	Concepts.Save(o);
	o << "I";
	Individuals.Save(o);
	o << "KB";
}

void
TBox :: Load ( istream& i, KBStatus status )
{
	Status = status;
	string KB;
	expectChar(i,'C');
	Concepts.Load(i);
	expectChar(i,'I');
	Individuals.Load(i);
	expectChar(i,'K');
	expectChar(i,'B');
}

//----------------------------------------------------------
//-- Implementation of the TNECollection methods (tNECollection.h)
//----------------------------------------------------------

/// Save all the objects in the collection
template<class T>
void
TNECollection<T> :: Save ( ostream& o ) const
{
	const_iterator p, p_beg = begin(), p_end = end();
	// get the max length of the identifier in the collection
	unsigned int maxLength = 0, curLength;

	for ( p = p_beg; p < p_end; ++p )
		if ( maxLength < (curLength = strlen((*p)->getName())) )
			maxLength = curLength;

	// save number of entries and max length of the entry
	saveUInt(o,size());
	saveUInt(o,maxLength);

	// save names of all entries
	for ( p = p_beg; p < p_end; ++p )
		o << (*p)->getName() << "\n";

	// save the entries itself
	for ( p = p_beg; p < p_end; ++p )
		(*p)->Save(o);
}
/// Load all the objects into the collection
template<class T>
void
TNECollection<T> :: Load ( istream& i )
{
	// sanity check: Load shall be done for the empty collection and only once
	assert ( size() == 0 );

	unsigned int collSize, maxLength;
	collSize = loadUInt(i);
	maxLength = loadUInt(i);
	++maxLength;
	char* name = new char[maxLength];

	// register all the named entries
	for ( unsigned int j = 0; j < collSize; ++j )
	{
		i.getline ( name, maxLength, '\n' );
		get(name);
	}

	delete [] name;

	// load all the named entries
	for ( iterator p = begin(); p < end(); ++p )
		(*p)->Load(i);
}

//----------------------------------------------------------
//-- Implementation of the TNamedEntry methods (tNamedEntry.h)
//----------------------------------------------------------

void
TNamedEntry :: Save ( ostream& o ATTR_UNUSED ) const
{
}

void
TNamedEntry :: Load ( istream& i ATTR_UNUSED )
{
}