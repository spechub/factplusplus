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

/*******************************************************\
|* Implementation of taxonomy building for the FaCT++  *|
\*******************************************************/

#include "Taxonomy.h"
#include "globaldef.h"
#include "logging.h"

#include <fstream>

//#define TMP_PRINT_TAXONOMY_INFO
using namespace std;

/********************************************************\
|* 			Implementation of class Taxonomy			*|
\********************************************************/
Taxonomy :: ~Taxonomy ( void )
{
	for ( iterator p = Graph.begin(), p_end = Graph.end(); p < p_end; ++p )
		delete *p;

	if ( deleteCurrent )
		delete Current;
}

void Taxonomy :: print ( std::ostream& o ) const
{
	o << "Taxonomy consists of " << nEntries << " entries\n";
	o << "            of which " << nCDEntries << " are completely defined\n\n";
	o << "All entries are in format:\n\"entry\" {n: parent_1 ... parent_n} {m: child_1 child_m}\n\n";

	for ( const_iterator p = itop(), p_end = end(); p < p_end; ++p )
		(*p)->print(o);
	getBottom()->print(o);
}

//---------------------------------------------------
// classification part
//---------------------------------------------------

void Taxonomy :: performClassification ( ClassifiableEntry* p )
{
	assert ( p != NULL );
	++nEntries;

	// set this concept as a current
	setCurrentEntry (p);

	if ( LLM.isWritable(llStartCfyEntry) && needLogging() )
		LL << "\n\nTAX: start classifying entry " << p->getName();

	// if no classification needed -- nothing to do
	if ( immediatelyClassified() )
		return;

	// perform main classification
	generalTwoPhaseClassification();

	// create new vertex
	if ( willInsertIntoTaxonomy )
		insertEntry ();
	else	// check if node is synonym of existing one and copy EXISTING info to Current
	{
		TaxonomyVertex* v = Current->incorporateSynonym(false);
		if ( v != NULL )
		{
//			delete Current;
//			deleteCurrent = false;
//			Current = v;
		}
	}

	// clear all labels
	clearLabels();
}

void Taxonomy :: generalTwoPhaseClassification ( void )
{
	// Top-Down phase

	// setup TD phase (ie, identify parent candidates)
	setupTopDown();

	// run TD phase if necessary (ie, entry is completely defined)
	if ( needTopDown() )
	{
		getTop()->setValued(true);		// C [= TOP == true
		getBottom()->setValued(false);	// C [= BOT == false (catched by UNSAT)
		runTopDown();
	}

	clearLabels();

	// Bottom-Up phase

	// setup BU phase (ie, identify children candidates)
	setupBottomUp();

	// run BU if necessary
	if ( needBottomUp() )
	{
		getBottom()->setValued(true);	// BOT [= C == true
		runBottomUp();
	}

	clearLabels();
}

bool Taxonomy :: classifySynonym ( void )
{
	assert ( curEntry != NULL );

	const ClassifiableEntry* syn = curEntry->getSynonym ();

	if ( syn == NULL )
		return false;	// not a synonym

	// I'm sure that there is impossible to have synonym here, but let's check
	assert ( willInsertIntoTaxonomy );

	// ensure that we found a real representative
	while ( syn->isSynonym() )
		syn = syn->getSynonym();

	// update synonym vertex:
	assert ( syn->getTaxVertex() != NULL );
	syn->getTaxVertex()->addSynonym(curEntry);

	// clean up Current entry
	delete Current;
	Current = NULL;
	curEntry = NULL;

	return true;
}

void Taxonomy :: setNonRedundantCandidates ( const ClassifiableEntry::linkSet& v )
{
	if ( LLM.isWritable(llCDConcept) && needLogging() )
	{
		if ( v.size() == 0 )
			LL << "\nTAX: TOP";
		LL << " completely defines concept " << curEntry->getName();
	}

	// test if some "told subsumer" is not an immediate TS (ie, not a border element)
	for ( ClassifiableEntry::linkSet::const_iterator p = v.begin(), p_end = v.end(); p < p_end; ++p )
	{
		TaxonomyVertex* par = (*p)->getTaxVertex();
		bool stillParent = true;
		TaxonomyVertex :: const_iterator
			q = par->begin(/*upDirection=*/false),
			q_end = par->end(/*upDirection=*/false);

		// if a child is labelled, remove it from parents candidates
		for ( ; q < q_end; ++q )
			if ( (*q)->isValued() )
			{
#			ifdef WARN_EXTRA_SUBSUMPTION
				std::cout << "\nCTAX!!: Definition (implies '" << curEntry->getName()
						  << "','" << (*p)->getName() << "') is extra because of definition (implies '"
						  << curEntry->getName() << "','" << (*q)->getPrimer()->getName() << "')\n";
#			endif
				stillParent = false;
				break;
			}

		if ( stillParent )
			Current->addNeighbour ( /*upDirection=*/true, par );
	}
}

void Taxonomy :: setToldSubsumers ( const ClassifiableEntry::linkSet& v )
{
	if ( LLM.isWritable(llTSList) && needLogging() && v.size() > 0 )
		LL << "\nTAX: told subsumers";

	for ( ClassifiableEntry::linkSet::const_iterator p = v.begin(), p_end = v.end(); p < p_end; ++p )
	{
		if ( !(*p)->isClassified() )	// non-primitive/non-classifiable concept
			continue;	// safety check

		if ( LLM.isWritable(llTSList) && needLogging() )
			LL << " '" << (*p)->getName() << "'";

		(*p)->getTaxVertex()->propagateValueUp(true);
	}
}

void Taxonomy :: insertEntry ( void )
{
	assert ( Current != NULL );

	// check if current concept is synonym to someone
	if ( Current->incorporateSynonym () )
	{
		delete Current;
	}
	else	// just incorporate it as a special entry and save into Graph
	{
		Current->incorporate ();
		Graph.push_back ( Current );
	}

	Current = NULL;
}

//-----------------------------------------------------------------
//--	DFS-based classification methods
//-----------------------------------------------------------------

#ifdef TMP_PRINT_TAXONOMY_INFO
static unsigned int level;

static void printHeader ( void )
{
	std::cout << "\n";
	for ( register unsigned int i = 0; i < level; ++i )
		std::cout << " ";
}
#endif

void Taxonomy :: classifyEntry ( ClassifiableEntry* p )
{
	assert ( waitStack.empty () );	// safety check

#ifdef TMP_PRINT_TAXONOMY_INFO
	std::cout << "\n\nClassifying " << p->getName();
#endif
	waitStack.push (p);	// put entry into stack

	// classify last concept from the stack
	while ( !waitStack.empty () )
		if ( checkToldSubsumers () )	// ensure all TS are classified
			classifyTop();		// classify top of the stack
		else
			classifyCycle();	// TS cycle found -- deal with it

#ifdef TMP_PRINT_TAXONOMY_INFO
	std::cout << "\nDone classifying " << p->getName();
#endif
}

bool Taxonomy :: checkToldSubsumers ( void )
{
	assert ( !waitStack.empty () );	// safety check
	const ClassifiableEntry::linkSet& v = waitStack.top()->getTold();
	register bool ret = true;

#ifdef TMP_PRINT_TAXONOMY_INFO
	++level;
#endif
	for ( ClassifiableEntry::linkSet::const_iterator q = v.begin(), q_end = v.end(); q < q_end; ++q )
	{
		ClassifiableEntry* r = *q;
		assert ( r != NULL );

#ifdef TMP_PRINT_TAXONOMY_INFO
		printHeader();
		std::cout << "try told subsumer " << r->getName() << "... ";
#endif
		if ( !r->isClassified() )	// need to classify *q first
		{
			// check if *q is in stack already
			if ( waitStack.contains (r) )
			{	// cycle found
				waitStack.push (r);		// set last element
				ret = false;			// false = not all told subsumers are collected
				break;
			}

			if ( !needToldClassification(r) )
				continue;
			// else - check all told subsumers of new on
			waitStack.push (r);
			ret = checkToldSubsumers ();
			break;
		}
#ifdef TMP_PRINT_TAXONOMY_INFO
		else
		{
			std::cout << "already classified";
		}
#endif
	}
#ifdef TMP_PRINT_TAXONOMY_INFO
	--level;
#endif
	// all told subsumers are classified => OK
	return ret;
}

void Taxonomy :: classifyTop ( void )
{
	assert ( !waitStack.empty () );	// safety check

	// load last concept
	ClassifiableEntry* p = waitStack.top ();
	assert ( p != NULL );
#ifdef TMP_PRINT_TAXONOMY_INFO
	std::cout << "\nTrying classify" << (p->isCompletelyDefined()? " CD " : " " ) << p->getName() << "... ";
#endif

	doClassification(p);
#ifdef TMP_PRINT_TAXONOMY_INFO
	std::cout << "done";
#endif
	waitStack.pop ();
}

void Taxonomy :: classifyCycle ( void )
{
	assert ( !waitStack.empty () );	// safety check

	// classify the top element
	ClassifiableEntry* p = waitStack.top ();
	classifyTop();

	// inform about concepts
	std::cerr << "\n* Concept definitions cycle found: " << p->getName();

	// make all other elements classified and of the same class
	while ( !waitStack.empty() )
	{
		// inform about cycle:
		std::cerr << ", " << waitStack.top()->getName();

		// add the concept to the same class
		waitStack.top()->setTaxVertex ( p->getTaxVertex() );
		waitStack.pop ();

		// indicate progress
		//pTaxProgress->incIndicator (); -- // FIXME!! later
	}

	std::cerr << "\n";

	//FIXME!! still thinking it is not correct. So..
	assert (0);
}