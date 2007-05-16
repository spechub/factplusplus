/* This file is part of the FaCT++ DL reasoner
Copyright (C) 2006-2007 by Dmitry Tsarkov

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

#ifndef _JNIACTOR_H
#define _JNIACTOR_H

#include <jni.h>
#include "JNISupport.h"
#include "Kernel.h"

/// class for acting with concept taxonomy
template<class AccessPolicy>
class JTaxonomyActor
{
protected:	// types
		/// array of TNEs
	typedef std::vector<DLTree*> SynVector;
		/// array for a set of taxonomy verteces
	typedef std::vector<SynVector> SetOfNodes;

protected:	// members
		/// JNI environment
	JNIEnv* env;
		/// array to return
	SetOfNodes acc;
		/// temporary vector to keep synonyms
	SynVector syn;

protected:	// methods
		/// create vector of Java objects by given SynVector
	jobjectArray getArray ( const SynVector& vec ) const
	{
		const char* objClassName = AccessPolicy::getClassName();
		jclass objClass = env->FindClass(objClassName);
		jobjectArray ret = env->NewObjectArray ( vec.size(), objClass, NULL );
		for ( unsigned int i = 0; i < vec.size(); ++i )
			env->SetObjectArrayElement ( ret, i, retObject ( env, vec[i], objClassName ) );
		return ret;
	}

		/// try current entry
	void tryEntry ( const ClassifiableEntry* p )
	{
		if ( AccessPolicy::applicable(p) )
			syn.push_back(AccessPolicy::buildTree(p));
	}

public:		// interface
		/// c'tor
	JTaxonomyActor ( JNIEnv* e ) : env(e) {}
		/// d'tor
	~JTaxonomyActor ( void ) {}

	// return values

		/// get single vector of synonyms (necessary for Equivalents, for example)
	jobjectArray getSynonyms ( void ) const { return getArray(acc[0]); }
		/// get 2D array of all required elements of the taxonomy
	jobjectArray getElements ( void ) const
	{
		std::string objArrayName("[");
		objArrayName += AccessPolicy::getClassName();
		jclass objClass = env->FindClass(objArrayName.c_str());
		jobjectArray ret = env->NewObjectArray ( acc.size(), objClass, NULL );
		for ( unsigned int i = 0; i < acc.size(); ++i )
			env->SetObjectArrayElement ( ret, i, getArray(acc[i]) );
		return ret;
	}

		/// taxonomy walking method
	bool apply ( const TaxonomyVertex& v )
	{
		syn.clear();
		tryEntry(v.getPrimer());

		for ( TaxonomyVertex::syn_iterator p = v.begin_syn(), p_end=v.end_syn(); p != p_end; ++p )
			tryEntry(*p);

		if ( syn.empty() && AccessPolicy::regular(v.getPrimer()) )
			return false;	// special-case equivalents of temp-concept

		acc.push_back(syn);
		return true;
	}
}; // JTaxonomyActor

// policy elements

/// policy for concepts
class ClassPolicy
{
public:
	static const char* getClassName ( void ) { return "Luk/ac/manchester/cs/factplusplus/ClassPointer;"; }
	static bool applicable ( const ClassifiableEntry* p )
		{ return !p->isSystem() && !static_cast<const TConcept*>(p)->isSingleton(); }
	static bool regular ( const ClassifiableEntry* p )
		{ return !p->isSystem() || strcmp ( p->getName(), "FaCT++.default" ) != 0; }
	static DLTree* buildTree ( const ClassifiableEntry* p )
	{
		if ( p->getId () >= 0 )
			return new DLTree(TLexeme(CNAME,const_cast<ClassifiableEntry*>(p)));

		// top or bottom
		std::string name(p->getName());

		if ( name == std::string("TOP") )
			return new DLTree(TOP);
		else if ( name == std::string("BOTTOM") )
			return new DLTree(BOTTOM);
		else	// error
			return NULL;
	}
}; // ClassPolicy

/// policy for individuals
class IndividualPolicy
{
public:
	static const char* getClassName ( void ) { return "Luk/ac/manchester/cs/factplusplus/IndividualPointer;"; }
	static bool applicable ( const ClassifiableEntry* p )
		{ return !p->isSystem() && static_cast<const TConcept*>(p)->isSingleton(); }
	static bool regular ( const ClassifiableEntry* p ATTR_UNUSED ) { return true; }
	static DLTree* buildTree ( const ClassifiableEntry* p )
		{ return new DLTree(TLexeme(INAME,const_cast<ClassifiableEntry*>(p))); }
}; // IndividualPolicy

/// policy for object properties
class ObjectPropertyPolicy
{
public:
	static const char* getClassName ( void ) { return "Luk/ac/manchester/cs/factplusplus/ObjectPropertyPointer;"; }
	static bool applicable ( const ClassifiableEntry* p )
		{ return !p->isSystem() && p->getId() > 0; }
	static bool regular ( const ClassifiableEntry* p ATTR_UNUSED ) { return true; }
	static DLTree* buildTree ( const ClassifiableEntry* p )
		{ return new DLTree(TLexeme(RNAME,const_cast<ClassifiableEntry*>(p))); }
}; // ObjectPropertyPolicy

/// policy for data properties
class DataPropertyPolicy
{
public:
	static const char* getClassName ( void ) { return "Luk/ac/manchester/cs/factplusplus/DataPropertyPointer;"; }
	static bool applicable ( const ClassifiableEntry* p )
		{ return !p->isSystem() && p->getId() > 0; }
	static bool regular ( const ClassifiableEntry* p ATTR_UNUSED ) { return true; }
	static DLTree* buildTree ( const ClassifiableEntry* p )
		{ return new DLTree(TLexeme(RNAME,const_cast<ClassifiableEntry*>(p))); }
}; // DataPropertyPolicy

#endif