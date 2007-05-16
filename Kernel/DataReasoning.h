/* This file is part of the FaCT++ DL reasoner
Copyright (C) 2005-2007 by Dmitry Tsarkov

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


#ifndef _DATAREASONING_H
#define _DATAREASONING_H


#include <map>
#include <vector>

#include "tDataEntry.h"
#include "DataTypeComparator.h"
#include "ConceptWithDep.h"
#include "dlDag.h"

class DataTypeAppearance
{
public:		// typedefs
	typedef std::pair<const TDataEntry*, DepSet> DepDTE;

protected:	// classes
		/// class for keeping single values
	class SingleValues
	{
	protected:	// types
			/// base type
		typedef std::vector<DepDTE> DEVector;
	protected:	// members
			/// array of values
		DEVector Base;
	public:		// interface
			/// empty c'tor
		SingleValues ( void ) {}
			/// empty d'tor
		~SingleValues ( void ) {}

			/// add new value
		void add ( const DepDTE& value ) { Base.push_back(value); }
			/// clear all values
		void clear ( void ) { Base.clear(); }

			/// check if VALUE is found in values; fills DEP with a dep-set
		bool find ( const ComparableDT& value, DepSet& dep ) const;
			/// check if [from..to] is completely covered by values
		bool covers ( long from, long to, DepSet& dep ) const;
	}; // SingleValues

		/// data interval with dep-sets
	class DepInterval
	{
	protected:	// members
			/// interval itself
		TDataInterval Constraints;
			/// dep-set for the min border of the interval
		DepSet minDep;
			/// dep-set for the max border of the interval
		DepSet maxDep;
	public:		// interface
			/// empty c'tor
		DepInterval ( void ) {}
			/// empty d'tor
		~DepInterval ( void ) {}

			/// update MIN border of an TYPE's interval with VALUE wrt EXCL
		bool update ( bool min, bool excl, const TDataEntry* value, const DepSet& dep )
		{
			if ( !Constraints.update ( min, excl, value ) )
				return false;
			maxDep = dep;
			return true;
		}
			/// correct MIN and MAX operands of a type
		bool checkMinMaxClash ( const SingleValues& values, DepSet& dep ) const;
			/// check if interval is covered by VALUES
		bool isCovered ( const SingleValues& values, DepSet& dep ) const;
			/// clear the interval
		void clear ( void ) { Constraints.clear(); }
	}; // DepInterval

		/// datatype restriction is a set of intervals
	typedef std::vector<DepInterval> DTConstraint;
	typedef DTConstraint::iterator iterator;
	typedef DTConstraint::const_iterator const_iterator;

public:		// members
		/// positive type appearance
	DepDTE PType;
		/// negative type appearance
	DepDTE NType;

protected:	// members
		/// interval of possible values
	DTConstraint Constraints;
		/// negated single values
	SingleValues negValues;
		/// accumulated dep-set
	DepSet accDep;

public:		// methods
		/// empty c'tor
	DataTypeAppearance ( void ) {}
		/// empty d'tor
	~DataTypeAppearance ( void ) {}

		/// clear the appearance flags
	void clear ( void )
	{
		PType = NType =
			std::make_pair(static_cast<const TDataEntry*>(NULL),DepSet());
		Constraints.clear();
		Constraints.push_back(DepInterval());
		negValues.clear();
		accDep.clear();
	}

	// presence interface

		/// check if type is present positively in the node
	bool hasPType ( void ) const { return PType.first != NULL; }
		/// check if type is present negatively in the node
	bool hasNType ( void ) const { return NType.first != NULL; }
		/// set the precense of the PType
	void setPType ( const DepDTE& type )
	{
		if ( !hasPType() )
			PType = type;
	}
		/// set the precense of the PType
	void setPType ( const TDataEntry* value, const DepSet& dep )
	{
		if ( !hasPType() )
			PType = std::make_pair(value->getType(),dep);
	}
		/// add (not VALUE) constraint
	void addNegValue ( const DepDTE& value ) { negValues.add(value); }

	// complex methods

		/// update MIN border of an TYPE's interval with VALUE wrt EXCL
	bool update ( bool min, bool excl, const TDataEntry* value, const DepSet& dep );
		/// add restrictions [POS]P to intervals
	bool addInterval ( bool pos, const TDataInterval& p, const DepSet& dep );
		/// @return true iff PType and (possibly inferred) NType leads to clash
	bool checkPNTypeClash ( void ) const;
}; // DataTypeAppearance

class DataTypeReasoner
{
protected:	// types
		/// vector of data types
	typedef std::vector<DataTypeAppearance> DTAVector;
		/// map from positive BPs (DT pNames) to corresponding data type
	typedef std::map<const TDataEntry*,size_t> TypeMap;
		/// complex DTE type copied from DTA
	typedef DataTypeAppearance::DepDTE DepDTE;

protected:	// members
		/// vector of a types
	DTAVector Types;
		/// map Type.pName->Type appearance
	TypeMap Map;
		/// external DAG
	const DLDag& DLHeap;

protected:	// methods

	// aux functions used by addDataEntry function
	bool processNegativeDV ( const DepDTE& c )
	{
		getDTAbyValue(c.first).addNegValue(c);
		return false;
	}
		/// set (C,DEP) as a MIN value of a type's interval
	bool processRestriction ( bool pos, bool min, bool excl, const TDataEntry* c, const DepSet& dep )
	{
		DataTypeAppearance& type = getDTAbyValue(c);
		DepDTE C(c,dep);	// real concept to be added

		if (pos)
			type.setPType(C);

		// update maximal border of the interval; @return true if TYPE was changed
		return type.update ( min, excl, c, dep );
	}
		/// process data expr
	bool processDataExpr ( bool pos, const TDataEntry* c, const DepSet& dep )
	{
		const TDataInterval& constraints = *c->getFacet();
		if ( constraints.empty() )
			return false;
		DataTypeAppearance& type = getDTAbyValue(c);
		if (pos)
			type.setPType(DepDTE(c,dep));
		return type.addInterval ( pos, constraints, dep );
	}

		/// get data entry structure by a BP
	const TDataEntry* getDataEntry ( BipolarPointer p ) const
		{ return static_cast<const TDataEntry*>(DLHeap[p].getConcept()); }
		/// get TDE with a dep-set by a CWD
	DepDTE getDTE ( const ConceptWDep& c ) const
		{ return std::make_pair(getDataEntry(c.bp()),c.getDep()); }

	// get access to proper DataTypeAppearance

		/// get DTA by given data-type pointer
	DataTypeAppearance& getDTAbyType ( const TDataEntry* dataType )
	{
#	ifdef ENABLE_CHECKING
		assert ( Map.find(dataType) != Map.end() );
#	endif
		return Types[Map[dataType]];
	}
		/// get DTA by given data-value pointer
	DataTypeAppearance& getDTAbyValue ( const TDataEntry* dataValue )
	{
#	ifdef ENABLE_CHECKING
		assert ( !dataValue->isBasicDataType() );
#	endif
		return getDTAbyType(dataValue->getType());
	}

public:		// interface
		/// c'tor: save DAG
	DataTypeReasoner ( const DLDag& dag ) : DLHeap(dag) {}
		/// empty d'tor
	~DataTypeReasoner ( void ) {}

	// managing DTR

		/// add data type to the reasoner
	void registerDataType ( const TDataEntry* p )
	{
		Map[p] = Types.size();
		Types.push_back(DataTypeAppearance());
	}
		/// prepare types for the reasoning
	void clear ( void )
	{
		for ( DTAVector::iterator p = Types.begin(), p_end = Types.end(); p < p_end; ++p )
			p->clear();
	}

	// filling structures and getting answers

		/// add data entry to the DTAVector; @return true iff data-data clash was found
	bool addDataEntry ( const ConceptWDep& c );
		/// @return true iff data inconsistency was found in a structure; ClashSet would be set approprietry
	bool checkClash ( void );
}; // DataTypeReasoner


#endif