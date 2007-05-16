/* This file is part of the FaCT++ DL reasoner
Copyright (C) 2003-2006 by Dmitry Tsarkov

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

// ---------------------------------------------------------------------------
//  Includes
// ---------------------------------------------------------------------------
#include <string>
#include <cassert>

#include <xercesc/sax/AttributeList.hpp>
#include <xercesc/sax/SAXParseException.hpp>
#include <xercesc/sax/SAXException.hpp>

#include "strx.h"
#include "globaldef.h"
#include "procTimer.h"
#include "logging.h"

#ifdef RKG_PRINT_DIG_MESSAGES
#	include <fstream>
#endif

// uncomment the following to use LL in DIG programs
#ifdef _USE_LOGGING
//#	define USE_DIG_LL
#endif

#include "DIGParserHandlers.h"

using namespace xercesc;
using namespace std;

// general error info output
inline void outMessage ( const char* mName, ostream& o, unsigned int Number, const char* Reason, const char* Note )
{
	complexXMLEntry m ( mName, o );
	o << " code=\"" << Number << "\" message=\"" << Reason << "\"";
	m.closeStart();
	o << Note;
}

// out error (external linkage)
void outError ( ostream& o, const DIGParserException& e )
{
	outMessage ( "error", o, e.number(), e.reason(), e.note() );
}

// out error via outMessage
void DIGParseHandlers :: outError ( unsigned int Number, const char* Reason, const char* Note )
{
	outMessage ( "error", *o, Number, Reason, Note );
}

// out warning
void DIGParseHandlers :: outWarning ( unsigned int Number, const char* Reason, const char* Note )
{
	outMessage ( "warning", *o, Number, Reason, Note );
}

void DIGParseHandlers :: classifyCurrentKB ( void )
{
#ifdef RKG_PRINT_DIG_MESSAGES
	// save time of classification
	TsProcTimer t;
	pKernel->useVerboseOutput();
	bool alreadyClassified = pKernel->isKBRealised();
	if ( !alreadyClassified )
		t.Start();
#endif

	// classify the KB if necessary
	pKernel->realiseKB();

#ifdef RKG_PRINT_DIG_MESSAGES
	if ( !alreadyClassified )
	{
		t.Stop();
		ofstream dl ("dl.res");
		pKernel->writeReasoningResult(dl,t);
	}
#endif
}

// ---------------------------------------------------------------------------
//  DIGParseHandlers: Constructors and Destructor
// ---------------------------------------------------------------------------
DIGParseHandlers::DIGParseHandlers()
	: o(NULL)
	, pEnv (NULL)
	, kFactory ()
	, useData (false)
	, inTell (false)
	, inAsk (false)
	, wasError (false)
{
	// don't need this for DIG reasoners (too big debug messages)
#ifdef USE_DIG_LL
	if ( LLM.initLogger ( 20, "reasoning.log" ) )	// DEBUG_ONLY
		cerr << "Could not init LeveLogger\n";
#endif
}

DIGParseHandlers::~DIGParseHandlers()
{
	// pEnv writes to o, so delete it first
	delete pEnv;
	delete o;
}


// ---------------------------------------------------------------------------
//  DIGParseHandlers: Implementation of the SAX DocumentHandler interface
// ---------------------------------------------------------------------------
void DIGParseHandlers :: startDocument()
{
	inTell = false;
	inAsk = false;
	wasError = false;
}

void DIGParseHandlers :: endDocument()
{
	// close XML env.
	delete pEnv;
	pEnv = NULL;

	LL.flush();
}

void DIGParseHandlers :: resetDocument()
{
	// close env. (if necessary)
	delete pEnv;
	pEnv = NULL;

	// reset output stream;
	delete o;
	o = new ostringstream;

	inTell = false;
	inAsk = false;
	wasError = false;
}

void DIGParseHandlers :: startElement ( const XMLCh* const name, AttributeList& attributes )
{
	// transform element name to a tag
	DIGTag tag = getTag ( StrX(name).localForm () );

	// choose proper action
	if ( tag >= dig_General_Begin && tag < dig_General_End )
		startCommand ( tag, attributes );
	else if ( tag >= dig_CName_Begin && tag < dig_CName_End )
		startConcept ( tag, attributes );
	else if ( tag >= dig_Axioms_Begin && tag < dig_Axioms_End )
		startAxiom ( tag, attributes );
	else if ( tag >= dig_Ask_Begin && tag < dig_Ask_End )
		startAsk ( tag, attributes );
	else
		throw DIGParserException ( 102, "XML error: Non-DIG element found", StrX(name).localForm() );
}

void DIGParseHandlers :: endElement ( const XMLCh* const name )
{
	// transform element name to a tag
	DIGTag tag = getTag ( StrX(name).localForm () );

	// choose proper action
	if ( tag >= dig_General_Begin && tag < dig_General_End )
		endCommand ( tag );
	else if ( tag >= dig_CName_Begin && tag < dig_CName_End )
		endConcept ( tag );
	else if ( tag >= dig_Axioms_Begin && tag < dig_Axioms_End )
		endAxiom ( tag );
	else if ( tag >= dig_Ask_Begin && tag < dig_Ask_End )
		endAsk ( tag );
	else
		throw DIGParserException ( 102, "XML error: Non-DIG element found", StrX(name).localForm() );
}

void DIGParseHandlers :: processingInstruction ( const XMLCh* const target, const XMLCh* const data )
{
	cerr << "\nprocessing instruction " << StrX(target) << " with data " << StrX(data);
}

void DIGParseHandlers :: characters ( const XMLCh* const chars, const unsigned int length ATTR_UNUSED )
{
	if ( useData )
		data = StrX(chars).localForm();
}

// ---------------------------------------------------------------------------
//  DIGParseHandlers: Overrides of the SAX ErrorHandler interface
// ---------------------------------------------------------------------------
void DIGParseHandlers :: error ( const SAXParseException& e )
{
	cerr << "\nError at (file " << StrX(e.getSystemId())
		 << ", line " << e.getLineNumber()
		 << ", char " << e.getColumnNumber()
		 << "): " << StrX(e.getMessage()) << endl;

	simpleXMLEntry err ( "error", *o );
	*o << " code=\"102\" message=\"Malformed Request (XML error at line "
	   << e.getLineNumber() << ", char " << e.getColumnNumber()
	   << ")\">" << StrX(e.getMessage());
}

void DIGParseHandlers::fatalError ( const SAXParseException& e )
{
	cerr << "\nFatal Error at (file " << StrX(e.getSystemId())
		 << ", line " << e.getLineNumber()
		 << ", char " << e.getColumnNumber()
		 << "): " << StrX(e.getMessage()) << endl;

	simpleXMLEntry err ( "error", *o );
	*o << " code=\"102\" message=\"Malformed Request (XML error at line "
	   << e.getLineNumber() << ", char " << e.getColumnNumber()
	   << ")\">" << StrX(e.getMessage());
}

void DIGParseHandlers::warning ( const SAXParseException& e )
{
	cerr << "\nWarning at (file " << StrX(e.getSystemId())
		 << ", line " << e.getLineNumber()
		 << ", char " << e.getColumnNumber()
		 << "): " << StrX(e.getMessage()) << endl;
}