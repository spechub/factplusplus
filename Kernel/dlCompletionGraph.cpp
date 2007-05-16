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

#include "dlCompletionGraph.h"

DlCompletionTreeArc*
DlCompletionGraph :: createEdge (
	DlCompletionTree* from,
	DlCompletionTree* to,
	bool isUpLink,
	const TRole* roleName,
	const DepSet& dep )
{
	// create 2 arcs between FROM and TO
	DlCompletionTreeArc* forward = CTEdgeHeap.get();
	forward->init ( roleName, dep, to );
	DlCompletionTreeArc* backward = CTEdgeHeap.get();
	backward->init ( roleName->inverse(), dep, from );

	// connect them to nodes
	saveNode ( from, branchingLevel );
	saveNode ( to, branchingLevel );
	// set the reverse link
	forward->setReverse(backward);

	if ( isUpLink )
	{
		from->addParent(forward);
		to->addChild(backward);
	}
	else	// goes to child
	{
		from->addChild(forward);
		to->addParent(backward);
	}

	return forward;
}

DlCompletionTreeArc*
DlCompletionGraph :: moveEdge ( DlCompletionTree* node, DlCompletionTreeArc* edge,
								bool isUpLink, const DepSet& dep )
{
	DlCompletionTreeArc* ret = NULL;

	// skip already purged edges
	if ( edge->isIBlocked())
		return ret;

	// skip edges not leading to nominal nodes
	if ( !isUpLink && !edge->getArcEnd()->isNominalNode() )
		return ret;

	// no need to copy reflexive edges: they would be recreated
	if ( !edge->isReflexiveEdge() )
	{
		// try to find for NODE->TO (TO->NODE) whether we
		// have TO->NODE (NODE->TO) edge already
		DlCompletionTree* to = edge->getArcEnd();
		DlCompletionTree::const_edge_iterator
			p = isUpLink ? node->begins() : node->beginp(),
			p_end = isUpLink ? node->ends() : node->endp();

		for ( ; p < p_end; ++p )
			if ( (*p)->getArcEnd() == to )
			{
				ret = addRoleLabel ( node, to, !isUpLink, edge->getRole(), dep );
				break;
			}

		if ( ret == NULL )
			ret = addRoleLabel ( node, to, isUpLink, edge->getRole(), dep );
	}

	// invalidate old edge
	invalidateEdge(edge);

	return ret;
}

// merge labels; see SHOIN paper for detailed description
void DlCompletionGraph :: Merge ( DlCompletionTree* from, DlCompletionTree* to,
								  const DepSet& dep,
								  std::vector<DlCompletionTreeArc*>& edges )
{
	DlCompletionTree::const_edge_iterator q, q_endp = from->endp(), q_ends = from->ends();
	DlCompletionTreeArc* temp;

	edges.clear();

	// 1. For all x: x->FROM make x->TO
	// FIXME!! no optimisations (in case there exists an edge TO->x labelled with R-)
	bool isUpLink = true;	// copying predecessors
	for ( q = from->beginp(); q < q_endp; ++q )
	{
		temp = moveEdge ( to, *q, isUpLink, dep );
		if ( temp != NULL )
			edges.push_back(temp);
	}

	// 2. For all nominal x: FROM->x make TO->x
	// FIXME!! no optimisations (in case there exists an edge x->TO labelled with R-)
	isUpLink = false;	// copying successors
	for ( q = from->begins(); q < q_ends; ++q )
	{
		temp = moveEdge ( to, *q, isUpLink, dep );
		if ( temp != NULL )
			edges.push_back(temp);
	}

	// 4. For all x: FROM \neq x, add TO \neq x
	updateIR ( to, from, dep );

	// 5. Purge FROM
	Purge ( from, to, dep );
}

void DlCompletionGraph :: Purge ( DlCompletionTree* p, const DlCompletionTree* root, const DepSet& dep )
{
	if ( p->isPBlocked() )
		return;

	saveRare ( p->setPBlocked ( root, dep ) );

	// update successors
	for ( DlCompletionTree::const_edge_iterator q = p->begins(); q != p->ends(); ++q )
		if ( (*q)->getArcEnd()->isBlockableNode() )
			Purge ( (*q)->getArcEnd(), root, dep );	// purge all blockable successors
		else
			invalidateEdge(*q);	// invalidate links to nominal successor
}

// save/restore

void DlCompletionGraph :: save ( void )
{
	SaveState* s = Stack.push();
	s->nNodes = endUsed;
	s->sNodes = SavedNodes.size();
	++branchingLevel;
}

void DlCompletionGraph :: restore ( unsigned int level )
{
	assert ( level > 0 );
	branchingLevel = level;
	RareStack.restore(level);
	SaveState* s = Stack.pop(level);
	endUsed = s->nNodes;
	unsigned int nSaved = s->sNodes;
	for ( iterator p = SavedNodes.begin()+nSaved, p_end = SavedNodes.end(); p < p_end; ++p )
		if ( (*p)->getId() < endUsed )	// don't restore nodes that are dead anyway
			restoreNode ( (*p), level );
	SavedNodes.resize(nSaved);
}

// printing CGraph

// indent to print CGraph nodes
unsigned int CGPIndent;
// bitmap to remember which node was printed
std::vector<bool> CGPFlag;

/// print proper indentation
static void PrintIndent ( std::ostream& o )
{
	o << "\n|";
	for ( unsigned int i = 1; i < CGPIndent; ++i )
		o << " |";
}

static void PrintNode ( const DlCompletionTree* node, std::ostream& o );
static void PrintEdge ( DlCompletionTree::const_edge_iterator edge,
						const DlCompletionTree* parent, std::ostream& o );

void DlCompletionGraph :: Print ( std::ostream& o ) const
{
	// init indentation and node labels
	CGPIndent = 0;
	CGPFlag.resize(endUsed);
	for ( std::vector<bool>::iterator p = CGPFlag.begin(); p != CGPFlag.end(); ++p )
		*p = false;

#ifdef ENABLE_CHECKING
	if ( getActualRoot()->beginp() != getActualRoot()->endp() )
	{
		o << "\n|-";
		(*getActualRoot()->beginp())->Print(o);
		o << " from node " << (*getActualRoot()->beginp())->getArcEnd()->getId();
	}
#endif
	o << "\n";
	// print tree starting from actual root node
	PrintNode ( getActualRoot(), o );
	o << "\n";
}

	/// print node of the graph with proper indentation
static void PrintNode ( const DlCompletionTree* node, std::ostream& o )
{
	if ( CGPFlag[node->getId()] )	// don't print nodes twice
		return;

	CGPFlag[node->getId()] = true;	// mark node printed
	if ( CGPIndent )
	{
		PrintIndent(o);
		o << "-";
	}
	node->PrintBody(o);				// print node's label

	// print all children
	++CGPIndent;
	for ( DlCompletionTree::const_edge_iterator p = node->begins(); p != node->ends(); ++p )
		PrintEdge ( p, node, o );
	--CGPIndent;
}

	/// print edge of the graph with proper indentation
static void PrintEdge ( DlCompletionTree::const_edge_iterator edge,
						const DlCompletionTree* parent, std::ostream& o )
{
	const DlCompletionTree* node = (*edge)->getArcEnd();
	if ( CGPFlag[node->getId()] && node != parent )	// don't print nodes twice
		return;

	PrintIndent(o);
	for ( DlCompletionTree::const_edge_iterator p = edge; p != parent->ends(); ++p )
		if ( (*p)->getArcEnd() == node )
			o << " ", (*p)->Print(o);		// print edge's label

	if ( node == parent )	// print loop
	{
		PrintIndent(o);
		o << "-loop to node " << parent->getId();
	}
	else
		PrintNode ( node, o );
}