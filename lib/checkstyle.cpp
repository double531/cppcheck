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
CCheckStyle::CCheckStyle( const Tokenizer* p_Tokenizer, const std::vector< std::string >& p_vecComments, const Settings* p_Settings, ErrorLogger* p_ErrorLogger )
                  :Check( "Style", p_Tokenizer, p_Settings, p_ErrorLogger ), m_vecComments( p_vecComments )
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
    m_mapWhitelist[ "iterator" ] = "it";   //26.1

    //ds custom
    m_mapWhitelist[ "array" ]              = "arr"; //27
    m_mapWhitelist[ "method" ]             = "_";   //28
    m_mapWhitelist[ "attribute" ]          = "m_";  //29
    m_mapWhitelist[ "parameter" ]          = "p_";  //30
    m_mapWhitelist[ "global variable" ]    = "g_";  //31
}

void CCheckStyle::dumpTokens( )
{
    std::cout << "<CCheckStyle>[dumpTokens]( ) display non-simplified token list" << std::endl;

    //ds loop through all tokens of the current file
    for( const Token* pcCurrent = _tokenizer->tokens( ); pcCurrent != 0; pcCurrent = pcCurrent->next( ) )
    {
        std::cout << pcCurrent->str( ) << std::endl;
    }
}

void CCheckStyle::checkNames( )
{
    std::cout << "<CCheckStyle>[checkNames]( ) checking non-simplified token list" << std::endl;

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

                //ds check each argument of the function as variable
                for( std::list< Variable >::const_iterator itVariable = pcFunction->argumentList.begin( ); itVariable != pcFunction->argumentList.end( ); ++itVariable )
                {
                    //ds get the variable from the iterator (this avoids the overloading of checkPrefix( ) for a const_iterator Variable)
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

        //ds always check for asserts (not considered real functions)
        if( "assert" == pcCurrent->str( ) )
        {
            //ds make sure we really caught an assert by checking the brackets
            if( "(" == pcCurrent->next( )->str( ) )
            {
                //ds check if it is linked
                if( 0 != pcCurrent->next( )->link( ) )
                {
                    //ds call the check procedure
                    checkAssert( pcCurrent );
                }
            }

        }

        //ds always check for boost pointer initializations (unfortunately cppcheck does not recognize boost::shared_ptr< char > test( new char[123] );)
        if( "shared_ptr" == pcCurrent->str( ) || "scoped_ptr" == pcCurrent->str( ) )
        {
            //ds call the check procedure
            checkBoostPointer( pcCurrent );
        }
    }
}

void CCheckStyle::checkNamesError( const Token* p_Token, const std::string p_strErrorInformation, const Severity::SeverityType p_cSeverity  )
{
    //ds report the error
    reportError( p_Token, p_cSeverity, "checkNames", p_strErrorInformation );
}

void CCheckStyle::checkComments( )
{
    std::cout << "<CCheckStyle>[checkComments]( ) checking comment list" << std::endl;

    for( unsigned int uIndex = 0; uIndex < m_vecComments.size( ); ++uIndex )
    {
        std::cout << m_vecComments[uIndex] << std::endl;
    }
}

void CCheckStyle::checkCommentsError( const Token* p_Token, const std::string p_strErrorInformation, const Severity::SeverityType p_cSeverity )
{
    //ds report the error
    reportError( p_Token, p_cSeverity, "checkComments", p_strErrorInformation );
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
            checkNamesError( p_pcToken, "prefix of " + p_strFunctionScopePrefix + ": " + p_pcFunction->name( ) + "( ) is invalid - correct prefix: " + '_', Severity::style );
        }
    }
    else
    {
        //ds we got and error if first character is a '_' character
        if( p_pcFunction->name( )[0] == '_' )
        {
            //ds trigger error message
            checkNamesError( p_pcToken, "prefix of " + p_strFunctionScopePrefix + ": " + p_pcFunction->name( ) + "( ) is invalid - prefix: " + '_' + " is only allowed for methods", Severity::style );
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

    //ds determine the complete variable type (we do not use p_pcVariable->type( )->name( ) since this function does not work consistent at all)
    const std::string strTypeName( _getVariableType( p_pcVariable ) );

    //ds get the correct type prefix from the type name (e.g. u, str) - we call the whitelist only with filtered variable types, e.g no int******
    std::string strCorrectTypePrefix( m_mapWhitelist[_filterVariableType( strTypeName )] );

    //ds check no type prefix could not be found - this is the case for all user defined classes/structs not covered in the whitelist
    if( true == strCorrectTypePrefix.empty( ) )
    {
        //ds check if we are handling a class without special prefixes (unlike std::string), unfortunately this does not work for class* and unresolved classes
        if( true == p_pcVariable->isClass( ) )
        {
            //ds trigger the class prefix
            strCorrectTypePrefix = m_mapWhitelist["class"];
        }

        //ds check if its a pointer (if the token before the name is a * because the isPointer( ) flag is not set for unknown types) and if it is an unknown class
        else if( "*"  == p_pcVariable->typeEndToken( )->str( )              &&
                false == p_pcVariable->typeStartToken( )->isStandardType( ) )
        {
            //ds trigger the class prefix
            strCorrectTypePrefix = m_mapWhitelist["class"];
        }
        else
        {
            //ds trigger error message (only informative error)
            checkNamesError( p_pcToken, "no matching prefix found for type: " + strTypeName, Severity::information );
        }
    }

    //ds check if the type is not forbidden
    if( "forbidden" != strCorrectTypePrefix )
    {
        //ds check if its a pointer (e.g. p) in this case we have to overwrite the type prefix (u becomes p, i becomes p )
        if( true == p_pcVariable->isPointer( ) )
        {
            //ds overwrite the prefix (e.g. i becomes p)
            strCorrectTypePrefix = m_mapWhitelist["pointer"];

            //ds check if its a class, then boost_shared pointers are preferred if not already present
            if( true == p_pcVariable->isClass( ) )
            {
                //ds inform user
                checkNamesError( p_pcToken, "forbidden use of pointer for class: " + strTypeName + " " + p_pcVariable->name( ) + " - please use: boost::shared_ptr< " + _filterVariableTypeSoft( strTypeName ) + " >", Severity::style );
            }

            //ds case for types entered in the whitelist but not detected as classes
            if( false == p_pcVariable->typeStartToken( )->isStandardType( ) )
            {
                //ds inform user
                checkNamesError( p_pcToken, "forbidden use of pointer for class: " + strTypeName + " " + p_pcVariable->name( ) + " - please use: boost::shared_ptr< " + _filterVariableTypeSoft( strTypeName ) + " >", Severity::style );
            }
        }

        //ds check if its a boost pointer
        else if( true == _isBoostPointer( strTypeName ) )
        {
            //ds overwrite the prefix (e.g. i becomes p)
            strCorrectTypePrefix = m_mapWhitelist["pointer"];
        }

        //ds check if its a array or boost array
        else if( true == p_pcVariable->isArray( ) || true == _isBoostArray( strTypeName ) )
        {
            //ds overwrite the prefix (e.g. i becomes arr)
            strCorrectTypePrefix = m_mapWhitelist["array"];
        }

        //ds add the scope prefix (has no effect if the overhanded scope prefix can not be found)
        strCorrectTypePrefix = m_mapWhitelist[p_strVariableScopePrefix] + strCorrectTypePrefix;
    }
    else
    {
        //ds use of forbidden types
        checkNamesError( p_pcToken, "use of forbidden type: " + strTypeName + " in " + p_strVariableScopePrefix + ": " + strTypeName + " " + p_pcVariable->name( ), Severity::style );

        //ds fatal - skip processing
        return;
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

void CCheckStyle::checkAssert( const Token* p_pcToken )
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

    //ds information string which covers the complete assert statement
    std::string strAssertStatement( "" );

    //ds get the complete statement here because the next loop breaks when an error is found
    for( const Token* itToken = p_pcToken->next( )->next( ); itToken != p_pcToken->next( )->link( ); itToken = itToken->next( ) )
    {
        //ds add all characters with a space
        strAssertStatement += itToken->str( );
        strAssertStatement += " ";
    }

    //ds check all arguments between the two links of the assert call
    for( const Token* itToken = p_pcToken->next( )->next( ); itToken != p_pcToken->next( )->link( ); itToken = itToken->next( ) )
    {
        //ds no increment/decrement operations allowed
        if( "++" == itToken->str( ) || "--" == itToken->str( ) )
        {
            //ds trigger error message
            checkNamesError( p_pcToken, "assert statement: assert( " + strAssertStatement + ") includes forbidden operation: " + itToken->str( ), Severity::style );
        }

        //ds no function calls allowed
        if( Token::eFunction == itToken->type( ) )
        {
            //ds trigger error message
            checkNamesError( p_pcToken, "assert statement: assert( " + strAssertStatement + ") includes forbidden function call: " + itToken->str( ) + "( )", Severity::style );
        }
    }
}

//ds TODO implement the parsing of boost::shared_ptr< char > pPointer1( new char[10] ); then the UGLY checkBoostPointer( ) can be removed
void CCheckStyle::checkBoostPointer( const Token* p_pcToken )
{
    //ds configurations are assumed:
    //ds 1: boost::shared_ptr< char > pPointer1( new char[10] ); -> cppcheck can not parse this
    //ds 2: boost::shared_ptr< char > pPointer1 = new char[10]; -> cppcheck parses to: boost::shared_ptr< char > pPointer1; pPointer1 = new char[10];

    //ds get the pointer type (shared or scoped)
    const std::string strPointerName( p_pcToken->str( ) );

    //ds get the end of the type definition
    const Token* pcEndToken = p_pcToken->findsimplematch( p_pcToken, ";" );

    //ds escape if no end was found
    if( 0 == pcEndToken )
    {
        return;
    }

    //ds pointer name
    std::string strVariableName( "" );

    //ds determine which configuration we have - first is the one cppcheck can not parse
    if( ")" == pcEndToken->previous( )->str( ) )
    {
        //ds the end token does not have to be shifted - we can directly get the pointers name
        strVariableName = pcEndToken->previous( )->link( )->previous( )->str( );

    }
    else
    {
        //ds the pointers name must be right before the first ;
        strVariableName = pcEndToken->previous( )->str( );

        //ds there is a second definition - shift the end token
        pcEndToken = pcEndToken->next( );
        pcEndToken = pcEndToken->findsimplematch( pcEndToken, ";" );
    }

    //ds type name (gets built up during array search - this is possible because the type must appear before the new call)
    std::string strTypeName( "" );

    //ds bracket counter in order to recursive information
    unsigned int uOpenBrackets( 0 );

    //ds check if new call was found
    bool bIsNewCallFound( false );

    //ds find the argument before the end token
    for( const Token* itToken = p_pcToken->next( ); itToken != pcEndToken; itToken = itToken->next( ) )
    {
        //ds check if an opening bracket is found
        if( "<" == itToken->str( ) )
        {
            ++uOpenBrackets;

            //ds go on
            continue;
        }

        //ds check if a closing bracket is found
        if( ">" == itToken->str( ) )
        {
            --uOpenBrackets;

            //ds go on
            continue;
        }

        //ds look for a new call (this is no violation yet)
        if( "new" == itToken->str( ) )
        {
            bIsNewCallFound = true;
        }

        //ds as long as there are open brackets record the information inbetween
        if( 0 != uOpenBrackets )
        {
            strTypeName += itToken->str( );
        }

        //ds escape if we reach a function or parameter end
        if( ")" == itToken->str( ) )
        {
            return;
        }

        //ds once the new call is found a following array opener [ is fatal
        if( true == bIsNewCallFound && "[" == itToken->str( ) )
        {
            //ds trigger error message
            checkNamesError( p_pcToken, "forbidden array initialization of class: boost::" + strPointerName + "< " + strTypeName + " > " + strVariableName + " - please use: boost::shared_array< " + strTypeName  + " >", Severity::style );
        }
    }
}

bool CCheckStyle::_isBoostPointer( const std::string strTypeName ) const
{
    //ds check all valid cases
    if( std::string::npos != strTypeName.find( "boost::shared_ptr" ) )
    {
        return true;
    }
    else if( std::string::npos != strTypeName.find( "shared_ptr" ) )
    {
        return true;
    }
    else if( std::string::npos != strTypeName.find( "boost::scoped_ptr" ) )
    {
        return true;
    }
    else if( std::string::npos != strTypeName.find( "scoped_ptr" ) )
    {
        return true;
    }
    else
    {
        //ds nothing found
        return false;
    }
}

bool CCheckStyle::_isBoostArray( const std::string strTypeName ) const
{
    //ds check all valid cases
    if( std::string::npos != strTypeName.find( "boost::shared_array" ) )
    {
        return true;
    }
    else if( std::string::npos != strTypeName.find( "shared_array" ) )
    {
        return true;
    }
    else if( std::string::npos != strTypeName.find( "boost::scoped_array" ) )
    {
        return true;
    }
    else if( std::string::npos != strTypeName.find( "scoped_array" ) )
    {
        return true;
    }
    else
    {
        //ds nothing found
        return false;
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

    //ds check for a template inside of the type
    const long unsigned int uStartTemplate( p_strType.find( '<' ) );

    //ds get the container type if present
    if( std::string::npos != uStartTemplate )
    {
        //ds first check if an iterator is present (since it overwrites all prefixes)
        if( "iterator" == p_strType.substr( p_strType.length( ) - 8, 8 ) )
        {
            //ds always iterator
            strTypeFiltered = "iterator";
        }
        else
        {
            //ds check for the namespace
            if( "std::" == p_strType.substr( 0, 5 ) )
            {
                //ds only get the type of the container right after the namespace
                strTypeFiltered = p_strType.substr( 5, uStartTemplate - 5 );
            }
            else
            {
                //ds no namespace to consider
                strTypeFiltered = p_strType.substr( 0, uStartTemplate );
            }
        }
    }
    else
    {
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
    }

    return strTypeFiltered;
}

const std::string CCheckStyle::_filterVariableTypeSoft( const std::string p_strType ) const
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
        if( itFunction->name( )      == p_cFunction->name( )     &&
            itFunction->type         == p_cFunction->type        &&
            itFunction->initArgCount == itFunction->initArgCount )
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
        if( itVariable->name( )  == p_cVariable->name( )  &&
            itVariable->index( ) == p_cVariable->index( ) )
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
