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

	//ds illegal types (magic keyword "forbidden" triggers error message - this is necessary because undefined types do not indicate an error)
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

void CCheckStyle::dumpTokens( )
{
    //ds loop through all tokens of the current file
    for( const Token* pcCurrent = _tokenizer->tokens( ); pcCurrent != 0; pcCurrent = pcCurrent->next( ) )
    {
    	std::cout << "token name: " << pcCurrent->str( ) << " file id: " << pcCurrent->fileIndex( ) << " varId: " << pcCurrent->varId( ) << std::endl;
    }
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

			//ds check if the function is real (sometimes get 0 pointers from the statement above) and if we don't have it already checked
			if( 0 != pcFunction && false == _isChecked( pcFunction ) )
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
							checkPrefix( pcCurrent, pcFunction );
						}

						//ds check if the function is a method
						else if( Private   == pcFunction->access ||
								 Protected == pcFunction->access )
						{
							checkPrefix( pcCurrent, pcFunction, "method" );
						}
					}
				}

				//ds local function
				else
				{
					checkPrefix( pcCurrent, pcFunction );
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

			//ds check if the variable is real (sometimes get 0 pointers from the statement above) and we don't have it already checked
			if( 0 != pcVariable && false == _isChecked( pcVariable ) )
			{
				//ds do not parse arguments again (parameters, they are parsed right after a function head is detected)
				if( false == pcVariable->isArgument( ) )
				{
					//ds add it to our vector
					m_vecParsedVariableList.push_back( *pcVariable );

					//ds check if the variable is defined inside of a class/struct by determining its scope
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
							checkPrefix( pcCurrent, pcVariable, "global variable" );
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

		//ds check for asserts (not considered real functions)
		else if( "assert" == pcCurrent->str( ) )
		{
			//ds make sure we really caught an assert by checking the brackets
			if( "(" == pcCurrent->next( )->str( ) )
			{
				//ds check if it is linked
				if( 0 != pcCurrent->next( )->link( ) )
				{
					checkAssertion( pcCurrent );
				}
			}

		}
    }
}

void CCheckStyle::checkNamesError( const Token* p_Token, const std::string p_strErrorInformation, const Severity::SeverityType p_cSeverity  )
{
	//ds report the error
    reportError( p_Token, p_cSeverity, "checkNames", p_strErrorInformation );
}

void CCheckStyle::checkPrefix( const Token* p_pcToken, const Function* p_pcFunction, const std::string p_strFunctionScopePrefix )
{
	//ds check input
	if( 0 == p_pcToken  )
	{
		//ds invalid call (TODO throw)
		std::cout << "<CCheckStyle>[checkPrefix] error: received null pointer Token" << std::endl;

		//ds skip processing
		return;
	}

	//ds check input
	if( 0 == p_pcFunction  )
	{
		//ds invalid call (TODO throw)
		std::cout << "<CCheckStyle>[checkPrefix] error: received null pointer Function" << std::endl;

		//ds skip processing
		return;
	}

	//ds for functions we only care about the scope prefix
	if( "method" == p_strFunctionScopePrefix )
	{
		//ds we got an error if first character is not a '_' character
		if( p_pcFunction->name( )[0] != '_' )
		{
			//ds trigger error message
			checkNamesError( p_pcToken, "prefix of " + p_strFunctionScopePrefix + ": " + p_pcFunction->name( ) + " is invalid - correct prefix: " + '_', Severity::style );
		}
	}
	else
	{
		//ds we got and error if first character is a '_' character
		if( p_pcFunction->name( )[0] == '_' )
		{
			//ds trigger error message
			checkNamesError( p_pcToken, "prefix of " + p_strFunctionScopePrefix + ": " + p_pcFunction->name( ) + " is invalid - prefix: " + '_' + " is only allowed for methods", Severity::style );
		}
	}
}

void CCheckStyle::checkPrefix( const Token* p_pcToken, const Variable* p_pcVariable, const std::string p_strVariableScopePrefix )
{
	//ds check input
	if( 0 == p_pcToken  )
	{
		//ds invalid call (TODO throw)
		std::cout << "<CCheckStyle>[checkPrefix] error: received null pointer Token" << std::endl;

		//ds skip processing
		return;
	}

	//ds check input
	if( 0 == p_pcVariable  )
	{
		//ds invalid call (TODO throw)
		std::cout << "<CCheckStyle>[checkPrefix] error: received null pointer Variable" << std::endl;

		//ds skip processing
		return;
	}

	//ds type name buffer
	std::string strTypeName( "" );

	//ds get variable type, first check if its a standard type (always check the variable type even if its a pointer or array which ignore the type name in the prefix)
	if( 0 != p_pcVariable->type( ) )
	{
		//ds we can directly get the type name
		strTypeName = p_pcVariable->type( )->name( );
	}
	else
	{
		//ds we have to determine the type name manually
		strTypeName = _getVariableType( p_pcVariable );
	}

	//ds get the correct type prefix from the type name (e.g. u, str) - we call the whitelist only with filtered variable types, e.g no int******
	std::string strCorrectTypePrefix( m_mapWhitelist[_filterVariableType( strTypeName )] );

	//ds check no type prefix could not be found
	if( true == strCorrectTypePrefix.empty( ) )
	{
		//ds check if we are handling a class without special prefixes (unlike std::string)
		if( true == p_pcVariable->isClass( ) )
		{
			//ds trigger the class prefix
			strCorrectTypePrefix = m_mapWhitelist["class"];
		}
		else
		{
			//ds trigger error message (only informative error)
			checkNamesError( p_pcToken, "no matching prefix found for type: " + strTypeName, Severity::information );

			//ds no return here since the scope prefix still can be checked (e.g. m_ is not allowed for local variables no matter what type)
		}
	}
	else
	{
		//ds check if the type is not forbidden
		if( "forbidden" != strCorrectTypePrefix )
		{
			//ds check if its a pointer or array (e.g. p or arr) in this case we have to overwrite the type prefix (u becomes p/arr, i becomes p/arr )
			if( true == p_pcVariable->isPointer( ) )
			{
				strCorrectTypePrefix = m_mapWhitelist["pointer"];
			}
			else if( true == p_pcVariable->isArray( ) )
			{
				strCorrectTypePrefix = m_mapWhitelist["array"];
			}

			//ds add the scope prefix
			strCorrectTypePrefix = m_mapWhitelist[p_strVariableScopePrefix] + strCorrectTypePrefix;
		}
		else
		{
			//ds use of forbidden types
			checkNamesError( p_pcToken, "use of forbidden type: " + strTypeName + " in " + p_strVariableScopePrefix + ": " + strTypeName + " " + p_pcVariable->name( ), Severity::style );

			//ds skip processing
			return;
		}
	}

	//ds check if there is a variable name (waited until now to display type information)
	if( 0 == p_pcVariable->name( ).length( ) )
	{
		//ds trigger error message (only informative error)
		checkNamesError( p_pcToken, "no variable name for type: " + strTypeName, Severity::style );

		//ds skip further processing
		return;
	}

	//ds check if variable is longer than the correct prefix (else too short variable name, this causes no error for loop variables e.g. u or i)
	if( p_pcVariable->name( ).length( ) >= strCorrectTypePrefix.length( ) )
	{
		//ds compare the prefix with the variable name
		for( unsigned int u = 0; u < strCorrectTypePrefix.length( ); ++u )
		{
			//ds if only one character is not matching we have a violation
			if( strCorrectTypePrefix[u] != p_pcVariable->name( )[u] )
			{
				//ds trigger error message
				checkNamesError( p_pcToken, "prefix of " + p_strVariableScopePrefix + ": " +  strTypeName + " " + p_pcVariable->name( ) + " is invalid - correct prefix: " + strCorrectTypePrefix, Severity::style );
			}
		}
	}
	else
	{
		//ds trigger error message
		checkNamesError( p_pcToken, "name of " + p_strVariableScopePrefix + ": " +  strTypeName + " " + p_pcVariable->name( ) + " is too short - correct prefix: " + strCorrectTypePrefix, Severity::style );

		//ds skip processing
		return;
	}
}

void CCheckStyle::checkAssertion( const Token* p_pcToken )
{
	//ds check input
	if( 0 == p_pcToken )
	{
		//ds invalid call (TODO throw)
		std::cout << "<CCheckStyle>[checkAssertion] error: received null pointer Token" << std::endl;

		//ds skip
		return;
	}

	//ds check if a bracket follows the token, checkAssertion must be called from the "assert" Token so a "(" Token has to follow
	if( "(" != p_pcToken->next( )->str( ) )
	{
		//ds invalid call (TODO throw)
		std::cout << "<CCheckStyle>[checkAssertion] error: invalid function call" << std::endl;

		//ds skip
		return;
	}

	//ds check all arguments between the two links of the assert call
	for( const Token* itToken = p_pcToken->next( )->next( ); itToken != p_pcToken->next( )->link( ); itToken = itToken->next( ) )
	{
		//ds no increment/decrement operations allowed
		if( "++" == itToken->str( ) || "--" == itToken->str( ) )
		{
			//ds trigger error message
			checkNamesError( p_pcToken, "assert statement includes forbidden operation: " + itToken->str( ), Severity::style );
		}

		//ds no function calls allowed
		if( Token::eFunction == itToken->type( ) )
		{
			//ds trigger error message
			checkNamesError( p_pcToken, "assert statement includes forbidden function call: " + itToken->str( ), Severity::style );
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

		//ds no type name found
		return "";
	}

	//ds buffer for the type
	std::string strType( "" );

	//ds check if we got an unsigned type (always has to be the first token)
	if( true == p_pcVariable->typeStartToken( )->isUnsigned( ) )
	{
		//ds add prefix
		strType += "unsigned ";
	}

	//ds get the complete variable type by looping over the tokens between the function opening-( and close-) (may be more than 1 token)
	for( const Token* itToken = p_pcVariable->typeStartToken( ); itToken != p_pcVariable->typeEndToken( )->next( ); itToken = itToken->next( ) )
	{
		//ds check if we have received an invalid type ending from cppcheck (this happens with function heads like f( char*, short )) by getting a end bracket
		if( ")" == itToken->str( ) || ";" == itToken->str( ) || "}" == itToken->str( ) )
		{
			//ds we reached the end of the function head, no more types are defined inside
			break;
		}
		else
		{
			//ds build up the complete type
			strType += itToken->str( );
		}
	}

	//ds return the complete type name
	return strType;
}

const std::string CCheckStyle::_filterVariableType( const std::string p_strType ) const
{
	std::string strTypeFiltered( "" );

	//ds loop through the whole string and remove *'s and &'s
    for( std::string::const_iterator itString = p_strType.begin( ); itString != p_strType.end( ); ++itString )
    {
    	//ds only add nonillegal characters
        if( '*' != *itString && '&' != *itString )
        {
        	//ds build the filtered string
        	strTypeFiltered += *itString;
        }
    }

	return strTypeFiltered;
}

bool CCheckStyle::_isChecked( const Function* p_cFunction ) const
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

	for( std::vector< Function >::const_iterator itFunction = m_vecParsedFunctionList.begin( ); itFunction != m_vecParsedFunctionList.end( ); ++itFunction )
	{
		//ds check the name and type
		if( itFunction->name( ) == p_cFunction->name( ) &&
			itFunction->type    == p_cFunction->type    )
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

bool CCheckStyle::_isChecked( const Variable* p_cVariable ) const
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

	for( std::vector< Variable >::const_iterator itVariable = m_vecParsedVariableList.begin( ); itVariable != m_vecParsedVariableList.end( ); ++itVariable )
	{
		//ds check the name
		if( itVariable->name( ) == p_cVariable->name( ) )
		{
			//ds if we have a type
			if( 0 != itVariable->type( ) && 0 != p_cVariable->type( ) )
			{
				//ds check it
				if( itVariable->type( )->name( ) == p_cVariable->type( )->name( ) )
				{
					bHasBeenFound = true;
				}
			}

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
