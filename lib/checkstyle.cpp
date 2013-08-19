/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2013 Daniel Marjamäki and Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "checkstyle.h"
#include "mathlib.h"
#include "templatesimplifier.h"

#include <cmath> // fabs()
#include <stack>
#include <algorithm> // find_if()



//ds register this check class (by creating a static instance of it)
namespace
{
    CCheckStyle instance;
}

//ds constructor for registration
CCheckStyle::CCheckStyle( ):Check( "Style" )
{
    //ds nothing to do
}

//ds constructor for test runs
CCheckStyle::CCheckStyle( const Tokenizer* p_Tokenizer, const Settings* p_Settings, ErrorLogger* p_ErrorLogger ):Check( "Style", p_Tokenizer, p_Settings, p_ErrorLogger )
{
    //ds initialize whitelist

	//ds illegal types (keyword forbidden triggers error message)
	m_mapWhitelist[ "long" ]           = "forbidden";
	m_mapWhitelist[ "unsigned long" ]  = "forbidden";
	m_mapWhitelist[ "short" ]          = "forbidden";
	m_mapWhitelist[ "unsigned short" ] = "forbidden";

	//ds hungarian notation
	m_mapWhitelist[ "bool" ]          = "b";   //1
	m_mapWhitelist[ "TByteStream" ]   = "bs";  //2
	m_mapWhitelist[ "class" ]         = "c";   //3
	m_mapWhitelist[ "struct" ]        = "c";   //4
	m_mapWhitelist[ "char" ]          = "ch";  //5
	m_mapWhitelist[ "unsigned char" ] = "ch";  //6
	m_mapWhitelist[ "double" ]        = "d";   //7
	m_mapWhitelist[ "enum" ]          = "e";   //8
	m_mapWhitelist[ "HANDLE" ]        = "h";   //9
	m_mapWhitelist[ "int" ]           = "i";   //10
	m_mapWhitelist[ "size_type" ]     = "n" ;  //11
	m_mapWhitelist[ "TPath" ]         = "pth"; //12
	m_mapWhitelist[ "unsigned int" ]  = "u";   //13
	m_mapWhitelist[ "string" ]        = "str"; //14
	m_mapWhitelist[ "std::string" ]   = "str"; //14.1
	m_mapWhitelist[ "TString" ]       = "str"; //15
	m_mapWhitelist[ "type" ]          = "t";   //16
    m_mapWhitelist[ "TTime" ]         = "tm";  //17
    m_mapWhitelist[ "word" ]          = "w";   //18
    m_mapWhitelist[ "pointer" ]       = "p";   //19

    //ds containers
    m_mapWhitelist[ "vector" ]   = "vec";  //20
    m_mapWhitelist[ "map" ]      = "map";  //21
    m_mapWhitelist[ "multimap" ] = "mmap"; //22
    m_mapWhitelist[ "list" ]     = "lst";  //23
    m_mapWhitelist[ "pair" ]     = "pr";   //24
    m_mapWhitelist[ "set" ]      = "set";  //25
    m_mapWhitelist[ "tuple" ]    = "tpl";  //26

    //ds custom
    m_mapWhitelist[ "array" ]              = "arr"; //27
    m_mapWhitelist[ "method" ]             = "_";   //28
    m_mapWhitelist[ "attribute" ]          = "m_";  //29
    m_mapWhitelist[ "parameter" ]          = "p_";  //30
    m_mapWhitelist[ "global variable" ]    = "g_";  //31
}

void CCheckStyle::checkNames( )
{
    //ds loop through all tokens of the current file
    for( const Token* pcCurrent = _tokenizer->tokens( ); pcCurrent != 0; pcCurrent = pcCurrent->next( ) )
    {
		//ds check if we got a function (always precedes a variable)
		if( Token::eFunction == pcCurrent->type( ) )
		{
			//ds get the function handle
			const Function* pcFunction = pcCurrent->function( );

			//ds check if the function is real (sometimes get 0 pointers from the statement above) and we don't have it already parsed
			if( 0 != pcFunction && false == _isAlreadyParsed( m_vecParsedFunctionList, pcFunction ) )
			{
				//ds add it to our vector
				m_vecParsedFunctionList.push_back( *pcFunction );

				//ds check if the function is class/struct based by determining its scope
				if( true == pcCurrent->scope( )->isClassOrStruct( ) )
				{
					//ds do not parse constructors/destructors
					if( false == pcFunction->isConstructor( ) && false == pcFunction->isDestructor( ) )
					{
						//ds check if the function is public (getter/setter)
						if( Public == pcFunction->access )
						{
							//ds function is not allowed to start with an _
							if( '_' == pcFunction->name( )[0] )
							{
								//ds trigger error message
								checkNamesError( pcCurrent, "prefix of class function: " + pcFunction->name( ) + "( ) contains invalid character: '_'", Severity::style );
							}
						}

						//ds check if the function is a method
						else if( Private   == pcFunction->access ||
								 Protected == pcFunction->access )
						{
							//ds function has to start with an _
							if( '_' != pcFunction->name( )[0] )
							{
								//ds trigger error message
								checkNamesError( pcCurrent, "prefix of class method: " + pcFunction->name( ) + "( ) is invalid - correct prefix: '_'", Severity::style );
							}
						}
					}
				}

				//ds local function
				else
				{
					//ds function is not allowed to start with an _
					if( '_' == pcFunction->name( )[0] )
					{
						//ds trigger error message
						checkNamesError( pcCurrent, "prefix of local function: " + pcFunction->name( ) + "( ) contains invalid character: '_'", Severity::style );
					}
				}

				//ds for each argument of the function
				for( std::list< Variable >::const_iterator itVariable = pcFunction->argumentList.begin( ); itVariable != pcFunction->argumentList.end( ); ++itVariable )
				{
					//ds get the variable from the iterator
					Variable cVariable = *itVariable;

					//ds check for a parameter variable
					checkPrefix( pcCurrent, &cVariable, "parameter" );
				}
			}
		}

    	//ds check if we got a single variable
		else if( Token::eVariable == pcCurrent->type( ) )
		{
			//ds get the variable handle
			const Variable* pcVariable = pcCurrent->variable( );

			//ds check if the variable is real (sometimes get 0 pointers from the statement above) and we don't have it already parsed
			if( 0 != pcVariable && false == _isAlreadyParsed( m_vecParsedVariableList, pcVariable ) )
			{
				//ds do not parse arguments (parameters, they are parsed right after a function head is detected)
				if( false == pcVariable->isArgument( ) )
				{
					//ds add it to our vector
					m_vecParsedVariableList.push_back( *pcVariable );

					//ds type buffer
					std::string strType( "" );

					//ds check if its a pointer or array first (we don't need the complete type for the naming convention)
					if( true == pcVariable->isPointer( ) )
					{
						strType = "pointer";
					}
					else if( true == pcVariable->isArray( ) )
					{
						strType = "array";
					}
					else
					{
						//ds get the variable type
						strType = _getVariableType( pcVariable );
					}

					//ds check if the variable is class/struct based by determining its scope
					if( pcCurrent->scope( )->isClassOrStruct( ) )
					{
						//ds check for an attribute name (m_ scope name)
						checkPrefix( pcCurrent, pcVariable, "attribute" );
					}
					else
					{
						//ds check if the variable is global or not
						if( true == pcVariable->isGlobal( ) )
						{
							//ds check the variable name (global scope)
							checkPrefix( pcCurrent, pcVariable, "global" );
						}
						else
						{
							//ds check the variable name (no scope name)
							checkPrefix( pcCurrent, pcVariable );
						}
					}
				}
			}
		}
    }

    std::cout << "end" << std::endl;
}

void CCheckStyle::checkNamesError( const Token* p_Token, const std::string p_strErrorInformation, const Severity::SeverityType p_cSeverity  )
{
	//ds report the error
    reportError( p_Token, p_cSeverity, "checkNames", p_strErrorInformation );
}

void CCheckStyle::checkPrefix( const Token* p_pcToken, const Variable* p_pcVariable, const std::string p_strVariableScopePrefix )
{
	//ds check input
	if( 0 == p_pcToken  )
	{
		//ds invalid call (TODO throw)
		std::cout << "<CCheckStyle>[checkPrefix] error: received null pointer Token" << std::endl;

		return;
	}

	//ds check input
	if( 0 == p_pcVariable  )
	{
		//ds invalid call (TODO throw)
		std::cout << "<CCheckStyle>[checkPrefix] error: received null pointer Variable" << std::endl;

		return;
	}

	//ds build the correct prefix for the current type, first get the scope prefix (e.g. m_
	std::string strPrefix( m_mapWhitelist[p_strVariableScopePrefix] );

	//ds check if its a pointer or array (e.g. p or arr)
	if( true == p_pcVariable->isPointer( ) )
	{
		strPrefix += m_mapWhitelist["pointer"];
	}
	else if( true == p_pcVariable->isArray( ) )
	{
		strPrefix += m_mapWhitelist["array"];
	}

	//ds get variable type
	const std::string strType( _getVariableType( p_pcVariable ) );

	//ds get the type prefix (e.g. u, str)
	const std::string strTypePrefix( m_mapWhitelist[strType] );

	//ds check no type prefix could not be found
	if( true == strTypePrefix.empty( ) )
	{
		//ds trigger error message (only informative error)
		checkNamesError( p_pcToken, "no matching prefix found for type: " + strType, Severity::information );

		//ds no return here since the scope prefix still can be checked
	}
	else
	{
		//ds check if the type is not forbidden
		if( "forbidden" != strTypePrefix )
		{
			//ds add it to get the final prefix
			strPrefix += strTypePrefix;
		}
		else
		{
			//ds use of forbidden types
			checkNamesError( p_pcToken, "use of forbidden type: " + strType + " in " + p_strVariableScopePrefix + ": " + strType + " " + p_pcVariable->name( ), Severity::style );

			return;
		}
	}

	//ds check if variable is longer than the prefix
	if( p_pcVariable->name( ).length( ) >= strPrefix.length( ) )
	{
		//ds compare the prefix with the variable name
		for( unsigned int u = 0; u < strPrefix.length( ); ++u )
		{
			//ds if only one character is not matching we have a violation
			if( strPrefix[u] != p_pcVariable->name( )[u] )
			{
				//ds trigger error message
				checkNamesError( p_pcToken, "prefix of " + p_strVariableScopePrefix + ": " +  strType + " " + p_pcVariable->name( ) + " is invalid - correct prefix: " + strPrefix, Severity::style );
			}
		}
	}
}

const std::string CCheckStyle::_getVariableType( const Variable* p_pcVariable ) const
{
	//ds check input
	if( 0 == p_pcVariable )
	{
		//ds invalid call (TODO throw)
		std::cout << "<CCheckStyle>[_getVariableType] error: received null pointer Variable" << std::endl;

		return "undefined";
	}

	//ds buffer for the type
	std::string strType( "" );

	//ds check if we got an unsigned type (has to be the first token)
	if( true == p_pcVariable->typeStartToken( )->isUnsigned( ) )
	{
		//ds add prefix
		strType += "unsigned ";
	}

	//ds get the complete variable type by looping over the tokens between the function opening-( and close-) (may be more than 1 token)
	for( const Token* itToken = p_pcVariable->typeStartToken( ); itToken != p_pcVariable->typeEndToken( )->next( ); itToken = itToken->next( ) )
	{
		//ds check if we reached an end bracket (may happen with some c files when cppcheck typeEndToken returns tokens behind function bodies)
		if( ")" == itToken->str( ) )
		{
			//ds skip further checking
			break;
		}

		//ds add all type strings except stars * and references &, they get registered separately
		if( "*" != itToken->str( ) && "&" != itToken->str( ) )
		{
			strType += itToken->str( );
		}
	}

	//ds return the complete type name
	return strType;
}

bool CCheckStyle::_isAlreadyParsed( std::vector< Function >& p_vecFunctionList, const Function* p_cFunction ) const
{
	//ds check input
	if( 0 == p_cFunction )
	{
		//ds invalid call (TODO throw)
		std::cout << "<CCheckStyle>[_isAlreadyParsed] error: received null pointer Function" << std::endl;

		return false;
	}

	//ds success bool
	bool bHasBeenFound( false );

	for( std::vector< Function >::const_iterator itFunction = p_vecFunctionList.begin( ); itFunction != p_vecFunctionList.end( ); ++itFunction )
	{
		//ds check the name
		if( itFunction->name( ) == p_cFunction->name( ) )
		{
			//ds if we have a scope check it
			if( 0 != itFunction->functionScope && 0 != p_cFunction->functionScope )
			{
				//ds check the scope name
				if( itFunction->functionScope->className == p_cFunction->functionScope->className )
				{
					bHasBeenFound = true;
				}
			}
			else
			{
				//ds no scope
				bHasBeenFound = true;
			}
		}
	}

	return bHasBeenFound;
}

bool CCheckStyle::_isAlreadyParsed( std::vector< Variable >& p_vecVariableList, const Variable* p_cVariable ) const
{
	//ds check input
	if( 0 == p_cVariable )
	{
		//ds invalid call (TODO throw)
		std::cout << "<CCheckStyle>[_isAlreadyParsed] error: received null pointer Variable" << std::endl;

		return false;
	}

	//ds success bool
	bool bHasBeenFound( false );

	for( std::vector< Variable >::const_iterator itVariable = p_vecVariableList.begin( ); itVariable != p_vecVariableList.end( ); ++itVariable )
	{
		//ds check the name
		if( itVariable->name( ) == p_cVariable->name( ) )
		{
			//ds if we have a scope check it
			if( 0 != itVariable->scope( ) && 0 != p_cVariable->scope( ) )
			{
				//ds check the scope name
				if( itVariable->scope( )->className == p_cVariable->scope( )->className )
				{
					bHasBeenFound = true;
				}
			}
			else
			{
				//ds no scope
				bHasBeenFound = true;
			}
		}
	}

	return bHasBeenFound;
}
