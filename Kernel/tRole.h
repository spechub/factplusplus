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

#ifndef _TROLE_H
#define _TROLE_H

#include <set>
#include <vector>

#include "globaldef.h"	// sorted reasoning
#include "BiPointer.h"
#include "dltree.h"
#include "taxNamEntry.h"
#include "tLabeller.h"
#include "RAutomaton.h"

#ifdef RKG_USE_SORTED_REASONING
#	include "mergableLabel.h"
#endif

class TBox;
class RoleMaster;

/// get index of role by its bipolar representation
inline unsigned int getRoleIndex ( BipolarPointer i ) { return ( (i>0) ? 2*i : -2*i+1 ); }

/// Define class with all information about DL role
class TRole: public ClassifiableEntry
{
	friend class RoleMaster;

private:	// no copy
	TRole ();
	TRole ( const TRole& );
	TRole& operator = ( const TRole& );

public:		// types
	typedef std::vector<TRole*> roleSet;
	typedef std::vector<const TRole*> constRoleSet;
	typedef roleSet::const_iterator iterator;
		/// roles that are disjoint with a given one
	typedef std::set<TRole*> DisjointRoles;

protected:	// members
		/// is role transitive or not(default:not)
	bool transitive;
		/// is role reflexive or not(default:not)
	bool reflexive;
		/// pointer to role's functional definition DAG entry (or just TOP)
	BipolarPointer Functional;
		/// is role simple or not (not simple if role or any of its sub-roles is transitive)
	bool simple;
		/// if role is not simple, set number of transitive subroles here
	unsigned int nTransSubroles;

		/// role that are inverse of given one
	TRole* Inverse;

		/// Domain of role as a concept description; default NULL
	DLTree* pDomain;
		/// Domain of role as a pointer to DAG entry
	BipolarPointer bpDomain;

		/// flag of data role
	bool DataRole;
		/// is role relevant to current query
	TLabeller::LabType rel;

#ifdef RKG_USE_SORTED_REASONING
		/// label of a domain (inverse role is used for a range label)
	mergableLabel domLabel;
#endif

	// for later filling
	// FIXME!! const was removed for Relevance setting
	roleSet Ancestor, Descendant;
		/// set of the most functional super-roles
	roleSet TopFunc;
		/// set of the roles that are disjoint with given
	DisjointRoles Disjoint;
		/// all compositions in the form R1*R2*\ldots*Rn [= R
	std::vector<roleSet> subCompositions;

		/// bit-vector of all parents
	std::vector<bool> AncMap;
		/// bit-vector of all roles disjoint with current
	std::vector<bool> DJRoles;

		/// automaton for role
	RoleAutomaton A;

protected:	// methods
		/// check if R is relative of this role wrt direction (ancestor/descendant)
		// make sure that both THIS and R are not synonyms
	bool isRelative ( const TRole* r, bool need_ancestors ) const;

	// support for Anc/Desc filling and such

		/// check if role or any of its sub-roles is transitive (ie non-simple)
	void setSimple ( void );
		/// check if the role is REALLY topmost-functional (internal-use only)
	bool isRealTopFunc ( void ) const;
		/// set up TopFunc member properly (internal-use only)
	void initTopFunc ( void );
		/// init map of all ancestors (N is a size of a bitmap)
	void initAncMap ( unsigned int n );
		/// init map of all disjoint roles
	void initDJMap ( void );

	// support for automaton construction

		/// replace RoR [= R with Trans(R); @return true if So...oRo...oT [= R were found
	bool preprocessComposition ( roleSet& RS );
		/// add transition to automaton with the role
	void addTrivialTransition ( const TRole* r )
		{ A.addTransitionSafe ( A.initial(), A.final(), r ); }
		/// add automaton of a sub-role to a given one
	void addSubRoleAutomaton ( const TRole* R )
	{
		if ( this == R )
			return;

		if ( R->isSimple() )
			A.addSimpleRA(R->getAutomaton());
		else
			A.addRA(R->getAutomaton());
	}
		/// get an automaton by a (possibly synonymical) role
	const RoleAutomaton& completeAutomatonByRole ( TRole* R ) const
	{
		assert ( !R->isSynonym() );	// no synonyms here
		assert ( R != this );		// no case ...*S*... [= S
		R->completeAutomaton();	// FIXME!! check for the cycles later on
		return R->getAutomaton();
	}
		/// init non-empty chain of role automata
	template<class Iterator>
	void createChain ( Iterator begin, Iterator end )
	{
		A.initChain(completeAutomatonByRole(*begin));
		for ( Iterator p = begin+1; p != end; ++p )
			A.addToChain(completeAutomatonByRole(*p));
	}
		/// add automaton for a role composition
	void addSubCompositionAutomaton ( const roleSet& RS )
	{
		if ( RS.empty() )	// fallout from transitivity axiom
			return;
		if ( RS.front()->resolveSynonym() == this )
		{
			createChain ( RS.begin()+1, RS.end() );
			A.addRBegRA();
		}
		else if ( RS.back()->resolveSynonym() == this )
		{
			createChain ( RS.begin(), RS.end()-1 );
			A.addREndRA();
		}
		else
		{
			createChain ( RS.begin(), RS.end() );
			A.addChainRA();
		}
	}

		/// check (and correct) case whether R != S for R [= S
	void checkHierarchicalDisjoint ( TRole* R );

public:		// interface
		/// the only c'tor
	TRole ( const std::string& name );
		/// d'tor
	~TRole ( void );

	// synonym operations

		/// returns associated synonym or role itself if none found (non-const version)
	TRole* resolveSynonym ( void ) { return isSynonym() ? (TRole*)getSynonym(): this; }
		/// returns associated synonym or role itself if none found (const version)
	const TRole* resolveSynonym ( void ) const { return isSynonym() ? (const TRole*)getSynonym(): this; }

		/// copy role information (like transitivity, functionality, R&D etc) to synonym
	void addFeaturesToSynonym ( void );

	// inverse of the role

		/// get inverse of given role (non-const version)
	TRole* inverse ( void ) { assert (Inverse != NULL); return Inverse->resolveSynonym(); }
		/// get inverse of given role (const version)
	const TRole* inverse ( void ) const { assert (Inverse != NULL); return Inverse->resolveSynonym(); }
		/// set inverse to given role
	void setInverse ( TRole* p ) { assert (Inverse == NULL); Inverse = p; }

	// transitivity

		/// check if the role is transitive
	bool isTransitive ( void ) const { return transitive; }
		/// set the transitivity of role
	void setTransitive ( void ) { transitive = true; inverse()->transitive = true; }
		/// get number of transitive suroles
	unsigned int getNTransSubroles ( void ) const { return nTransSubroles; }
		/// get the simple attribute
	bool isSimple ( void ) const { return simple; }

	// reflexivity

		/// check if the role is reflexive
	bool isReflexive ( void ) const { return reflexive; }
		/// set the reflexivity of role
	void setReflexive ( void ) { reflexive = true; inverse()->reflexive = true; }

	// functionality

		/// test if role is functional (ie, have some functional ancestors)
	bool isFunctional ( void ) const { return !TopFunc.empty(); }
		/// check if the role is topmost-functional (ie, has no functional ancestors)
	bool isTopFunc ( void ) const { return isFunctional() && *TopFunc.begin() == this; }
		/// mark node (topmost) functional
	void setFunctional ( void ) { if ( TopFunc.empty() ) TopFunc.push_back(this); }
		/// set functional attribute to given value (functional DAG vertex)
	void setFunctional ( BipolarPointer fNode ) { Functional = fNode; }
		/// get the Functional DAG vertex
	BipolarPointer getFunctional ( void ) const { return Functional; }

	// relevance

		/// is given role relevant to given Labeller's state
	bool isRelevant ( const TLabeller& lab ) const { return lab.isLabelled(rel); }
		/// make given role relevant to given Labeller's state
	void setRelevant ( const TLabeller& lab ) { lab.set(rel); }

#ifdef RKG_USE_SORTED_REASONING
	// Sorted reasoning interface

		/// get label of the role's domain
	mergableLabel& getDomainLabel ( void ) { return domLabel; }
		/// get label of the role's range
	mergableLabel& getRangeLabel ( void ) { return inverse()->getDomainLabel(); }
		/// merge label of given role and all its super-roles
	void mergeSupersDomain ( void );
#endif

	// domain and range

		/// add p to domain of the role
	void setDomain ( DLTree* p )
	{
		if ( equalTrees ( pDomain, p ) )
			deleteTree(p);	// usual case when you have a name for inverse role
		else
			pDomain = createSNFAnd ( pDomain, p );
	}
		/// add p to range of the role
	void setRange ( DLTree* p ) { inverse()->setDomain(p); }

		/// get domain-as-a-tree of the role
	DLTree* getTDomain ( void ) const { return pDomain; }
		/// get range-as-a-tree of the role
	DLTree* getTRange ( void ) const { return inverse()->pDomain; }

		/// merge to Domain all domains from super-roles
	void collectDomainFromSupers ( void );

		/// set domain-as-a-bipointer to a role
	void setBPDomain ( BipolarPointer p ) { bpDomain = p; }
		/// get domain-as-a-bipointer of the role
	BipolarPointer getBPDomain ( void ) const { return bpDomain; }
		/// get range-as-a-bipointer of the role
	BipolarPointer getBPRange ( void ) const { return inverse()->bpDomain; }

	// data flag

		/// check if role is Data role
	bool isDataRole ( void ) const { return DataRole; }
		/// set the Data role flag
	bool setDataRole ( void );

	// disjoint roles

		/// set R and THIS as a disjoint; use it after Anc/Desc are determined
	void addDisjointRole ( TRole* R )
	{
		Disjoint.insert(R);
		for ( iterator p = R->begin_desc(), p_end = R->end_desc(); p != p_end; ++p )
		{
			Disjoint.insert(*p);
			(*p)->Disjoint.insert(this);
		}
	}
		/// check (and correct) case whether R != S for R [= S
	void checkHierarchicalDisjoint ( void )
	{
		checkHierarchicalDisjoint(this);
		if ( isReflexive() )	// for reflexive roles check for R^- is necessary
			checkHierarchicalDisjoint(inverse());
	}
		/// check whether a role is disjoint with anything
	bool isDisjoint ( void ) const { return !Disjoint.empty(); }
		/// check whether a role is disjoint with R
	bool isDisjoint ( const TRole* r ) const { return DJRoles[getRoleIndex(r->getId())]; }

	// role relations checking

		/// two roles are the same iff thy are synonyms of the same role
	bool operator == ( const TRole& r ) const { return resolveSynonym() == r.resolveSynonym(); }
		/// check if role is a strict sub-role of R
//	bool operator < ( const TRole& r ) const { return resolveSynonym()->isRelative ( r.resolveSynonym(), true ); }
	bool operator < ( const TRole& r ) const { return AncMap[getRoleIndex(r.getId())]; }
		/// check if role is a non-strict sub-role of R
	bool operator <= ( const TRole& r ) const { return (*this == r) || (*this < r); }
		/// check if role is a strict super-role of R
	bool operator > ( const TRole& r ) const { return r < *this; }
		/// check if role is a non-strict super-role of R
	bool operator >= ( const TRole& r ) const { return (*this == r) || (*this > r); }

	// iterators

		/// get access to all super-roles via iterator
	iterator begin_anc ( void ) const { return Ancestor.begin(); }
	iterator end_anc ( void ) const { return Ancestor.end(); }
		/// get access to all sub-roles via iterator
	iterator begin_desc ( void ) const { return Descendant.begin(); }
	iterator end_desc ( void ) const { return Descendant.end(); }
		/// get access to the func super-roles w/o func parents via iterator
	iterator begin_topfunc ( void ) const { return TopFunc.begin(); }
	iterator end_topfunc ( void ) const { return TopFunc.end(); }

		/// add composition to a role
	void addComposition ( DLTree* tree )
	{
		roleSet RS;
		fillsComposition ( RS, tree );
		subCompositions.push_back(RS);
	}
		/// get access to a RA for the role
	const RoleAutomaton& getAutomaton ( void ) const { return A; }

	// completing internal constructions

		/// eliminate told role cycle
	TRole* eliminateToldCycles ( void );
		/// replace RoR [= R with Trans(R); @return true if So...oRo...oT [= R were found
	bool preprocessCompositions ( void )
	{
		bool ret = false;
		for ( std::vector<roleSet>::iterator q = subCompositions.begin(),
			  q_end = subCompositions.end(); q < q_end; ++q )
			ret |= preprocessComposition(*q);
		return ret;
	}
		/// init ancestors and descendants using Taxonomy
	void initADbyTaxonomy ( unsigned int ADMapSize );
		/// init other fields that requires Anc/Desc for all roles
	void postProcess ( void );
		/// fills role composition by given TREE
	void fillsComposition ( roleSet& Composition, DLTree* tree ) const;
		/// complete role automaton
	void completeAutomaton ( void );

	// output

		/// print role to given stream
	void Print ( std::ostream& o ) const;
}; // TRole

/// check if t is in form (inv ... (inv R)...) and return either R or -R. @return 0 in any other case
extern TRole* resolveRole ( const DLTree* t );

//--------------------------------------------------
//	TRole implementation
//--------------------------------------------------
inline TRole :: TRole ( const std::string& name )
	: ClassifiableEntry(name)
	, transitive (false)
	, reflexive(false)
	, Functional (bpINVALID)
	, Inverse(NULL)
	, pDomain(NULL)
	, bpDomain(bpINVALID)
	, DataRole (false)
	, rel(0)
{
	setCompletelyDefined (true);	// role hierarchy is completely defined by it's parents
	addTrivialTransition (this);
}

inline TRole :: ~TRole ( void )
{
	deleteTree(pDomain);
	if ( Inverse != NULL && Inverse != this )
	{
		Inverse->Inverse = NULL;
		delete Inverse;
	}
}

inline bool TRole :: setDataRole ( void )
{
	// data role cannot be transitive
	if ( isTransitive() )
		return true;

	// all parents should be data roles
	//FIXME!!DT check it later
	// set data feature
	DataRole = true;
	return false;
}

#endif