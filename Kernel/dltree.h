/* This file is part of the FaCT++ DL reasoner
Copyright (C) 2003-2007 by Dmitry Tsarkov

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

#ifndef __DLTREE_H
#define __DLTREE_H

#include <vector>
#include <iostream>
#include <cassert>

#include "tLexeme.h"
#include "tsttree.h"

typedef TsTTree <TLexeme> DLTree;

	// checking if the tree is in Symplified Normal Form
extern bool isSNF ( const DLTree* t );

	// checks if two trees are the same (syntactically)
extern bool equalTrees ( const DLTree* t1, const DLTree* t2 );

	// builds a copy of the formula t
inline DLTree* clone ( const DLTree* t ) { return (t==NULL) ? NULL : t->clone(); }

	// check if DL tree is a concept constant
inline bool isConst ( const DLTree* t )
{
	if ( t == NULL )
		return false;
	switch (t->Element().getToken())
	{
	case TOP:
	case BOTTOM:
		return true;
	default:
		return false;
	}
}

	// check if DL tree is a concept/individual name
inline bool isName ( const DLTree* t )
{
	if ( t == NULL )
		return false;
	switch (t->Element().getToken())
	{
	case CNAME:
	case INAME:
		return true;
	default:
		return false;
	}
}

	// check if DL tree is a concept-like name
inline bool isCN ( const DLTree* t ) { return isConst(t) || isName(t); }

	/// check whether T is U-Role
inline bool isUniversalRole ( const DLTree* t ) { return t && t->Element().getToken() == UROLE; }

// create SNF from given parts

	/// create inverse of role R
extern DLTree* createInverse ( DLTree* R );

	/// create negation of given formula
extern DLTree* createSNFNot ( DLTree* C );
	/// create conjunction of given formulas
extern DLTree* createSNFAnd ( DLTree* C, DLTree* D );
	/// create disjunction of given formulas
inline DLTree* createSNFOr ( DLTree* C, DLTree* D )
{	// C\or D -> \not(\not C\and\not D)
	return createSNFNot ( createSNFAnd ( createSNFNot(C), createSNFNot(D) ) );
}
	/// create universal restriction of given formulas (\AR.C)
extern DLTree* createSNFForall ( DLTree* R, DLTree* C );
	/// create existential restriction of given formulas (\ER.C)
inline DLTree* createSNFExists ( DLTree* R, DLTree* C )
{	// \ER.C -> \not\AR.\not C
	return createSNFNot ( createSNFForall ( R, createSNFNot(C) ) );
}

	/// create at-least (GE) restriction of given formulas (>= n R.C)
extern DLTree* createSNFGE ( unsigned int n, DLTree* R, DLTree* C );
	/// create at-most (LE) restriction of given formulas (<= n R.C)
inline DLTree* createSNFLE ( unsigned int n, DLTree* R, DLTree* C )
{
	return n ?
		new DLTree ( TLexeme ( LE, n ), R, C ) :
				// <= 0 R.C -> \AR.\not C
		createSNFForall ( R, createSNFNot(C) );
}

// parser access

	/// create and-like formula (C\and D or C\or D)
inline DLTree* createSNFwC ( Token T, DLTree* C, DLTree* D )
{
	assert ( T == AND || T == OR );

	if ( T == AND )
		return createSNFAnd ( C, D );
	else
		return createSNFOr ( C, D );
}
	/// create R-like formula (\ER.C, \AR.C, >= n R.C, <= n R.C)
inline DLTree* createSNFwR ( Token T, unsigned int n, DLTree* R, DLTree* C )
{
	switch(T)
	{
	case LE:
		return createSNFLE ( n, R, C );
	case GE:
		return createSNFGE ( n, R, C );
	case EXISTS:
		return createSNFExists ( R, C );
	case FORALL:
		return createSNFForall ( R, C );
	default:
		assert (0);
		return NULL;
	}
}

	/// create new complex (non-ID) formula from already-SNF
inline DLTree* rebuildSNF ( const TLexeme& lex, DLTree* C1, DLTree* C2 )
{
	switch(lex.getToken())
	{
	case NOT:
		return createSNFNot(C1);
	case AND:
		return createSNFAnd ( C1, C2 );
	case FORALL:
		return createSNFForall ( C1, C2 );
	case LE:
		return createSNFLE ( lex.getData(), C1, C2 );
	default:
		assert(0);
	}
}

// prints formula

extern const char* TokenName ( Token t );
extern std::ostream& operator << ( std::ostream& o, const DLTree *form );

#endif