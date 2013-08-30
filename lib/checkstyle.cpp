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
    //ds clear all containers
    m_vecParsedFunctionList.clear( );
    m_vecParsedVariableList.clear( );
    m_mapWhitelist.clear( );
}

//ds constructor for test runs
CCheckStyle::CCheckStyle( const Tokenizer* p_Tokenizer, const Settings* p_Settings, ErrorLogger* p_ErrorLogger ):Check( "Style", p_Tokenizer, p_Settings, p_ErrorLogger )
{
    //ds clear all containers
    m_vecParsedFunctionList.clear( );
    m_vecParsedVariableList.clear( );
    m_mapWhitelist.clear( );

    //ds initialize whitelist

    //ds illegal types (magic keyword "forbidden" triggers error message - this is necessary because undefined types do not indicate an error)
    m_mapWhitelist[ "long" ]           = "forbidden";
    m_mapWhitelist[ "unsigned long" ]  = "forbidden";
    m_mapWhitelist[ "short" ]          = "forbidden";
    m_mapWhitelist[ "unsigned short" ] = "forbidden";

    //ds hungarian notation
    m_mapWhitelist[ "bool" ]          = "b";
    m_mapWhitelist[ "TByteStream" ]   = "bs";
    m_mapWhitelist[ "class" ]         = "c";
    m_mapWhitelist[ "struct" ]        = "c";
    m_mapWhitelist[ "char" ]          = "ch";
    m_mapWhitelist[ "unsigned char" ] = "ch";
    m_mapWhitelist[ "double" ]        = "d";
    m_mapWhitelist[ "enum" ]          = "e";
    m_mapWhitelist[ "HANDLE" ]        = "h";
    m_mapWhitelist[ "int" ]           = "i";
    m_mapWhitelist[ "size_type" ]     = "n" ;
    m_mapWhitelist[ "TPath" ]         = "pth";
    m_mapWhitelist[ "unsigned int" ]  = "u";
    m_mapWhitelist[ "string" ]        = "str";
    m_mapWhitelist[ "std::string" ]   = "str";
    m_mapWhitelist[ "TString" ]       = "str";
    m_mapWhitelist[ "type" ]          = "t";
    m_mapWhitelist[ "TTime" ]         = "tm";
    m_mapWhitelist[ "word" ]          = "w";
    m_mapWhitelist[ "pointer" ]       = "p";

    //ds containers
    m_mapWhitelist[ "vector" ]         = "vec";
    m_mapWhitelist[ "map" ]            = "map";
    m_mapWhitelist[ "multimap" ]       = "mmap";
    m_mapWhitelist[ "list" ]           = "lst";
    m_mapWhitelist[ "pair" ]           = "pr";
    m_mapWhitelist[ "set" ]            = "set";
    m_mapWhitelist[ "tuple" ]          = "tpl";
    m_mapWhitelist[ "iterator" ]       = "it";
    m_mapWhitelist[ "const_iterator" ] = "it";


    //ds containers with namespaces
    m_mapWhitelist[ "std::vector" ]   = "vec";
    m_mapWhitelist[ "std::map" ]      = "map";
    m_mapWhitelist[ "std::multimap" ] = "mmap";
    m_mapWhitelist[ "std::list" ]     = "lst";
    m_mapWhitelist[ "std::pair" ]     = "pr";
    m_mapWhitelist[ "std::set" ]      = "set";
    m_mapWhitelist[ "std::tuple" ]    = "tpl";

    //ds custom
    m_mapWhitelist[ "array" ]              = "arr";
    m_mapWhitelist[ "method" ]             = "_";
    m_mapWhitelist[ "attribute" ]          = "m_";
    m_mapWhitelist[ "parameter" ]          = "p_";
    m_mapWhitelist[ "global variable" ]    = "g_";
}

CCheckStyle::~CCheckStyle( )
{
    //ds clear all containers
    m_vecParsedFunctionList.clear( );
    m_vecParsedVariableList.clear( );
    m_mapWhitelist.clear( );
}

void CCheckStyle::dumpTokens( )
{
    //ds loop through all tokens of the current file
    for( const Token* pcCurrent = _tokenizer->tokens( ); pcCurrent != 0; pcCurrent = pcCurrent->next( ) )
    {
        std::cout << pcCurrent->str( ) << std::endl;
    }
}

void CCheckStyle::checkComplete( )
{
    //ds loop through all tokens of the current file
    for( const Token* pcCurrent = _tokenizer->getCustomTokenListFront( ); pcCurrent != 0; pcCurrent = pcCurrent->next( ) )
    {
        //ds check if we got a function (always precedes a variable)
        if( Token::eFunction == pcCurrent->type( ) )
        {
            //ds get the function handle
            const Function* pcFunction( pcCurrent->function( ) );

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
                            checkPrefixFunction( pcCurrent, pcFunction );
                        }

                        //ds check if the function is a method
                        else if( Private   == pcFunction->access ||
                                 Protected == pcFunction->access )
                        {
                            checkPrefixFunction( pcCurrent, pcFunction, "method" );
                        }
                    }
                }

                //ds local function
                else
                {
                    checkPrefixFunction( pcCurrent, pcFunction );
                }

                //ds check each argument of the function as variable
                for( std::list< Variable >::const_iterator itVariable = pcFunction->argumentList.begin( ); itVariable != pcFunction->argumentList.end( ); ++itVariable )
                {
                    //ds get the variable from the iterator (this avoids the overloading of checkPrefix( ) for a const_iterator Variable)
                    Variable cVariable( *itVariable );

                    //ds check for a parameter variable
                    checkPrefixVariable( pcCurrent, &cVariable, "parameter" );
                }
            }
        }

        //ds check if we got a single variable
        else if( Token::eVariable == pcCurrent->type( ) )
        {
            //ds get the variable handle
            const Variable* pcVariable( pcCurrent->variable( ) );

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
                        checkPrefixVariable( pcCurrent, pcVariable, "attribute" );
                    }
                    else
                    {
                        //ds check if the variable is global or not
                        if( true == pcVariable->isGlobal( ) )
                        {
                            //ds check the variable name (global scope)
                            checkPrefixVariable( pcCurrent, pcVariable, "global variable" );
                        }
                        else
                        {
                            //ds check the variable name (no scope name)
                            checkPrefixVariable( pcCurrent, pcVariable );
                        }
                    }
                }
            }
        }

        //ds always check for asserts (not considered real functions)
        else if( "assert" == pcCurrent->str( ) )
        {
            //ds get the link start (better readability)
            const Token* pcTokenLinkStart( pcCurrent->next( ) );

            //ds make sure we really caught an assert by checking the brackets
            if( 0 != pcTokenLinkStart && "(" == pcTokenLinkStart->str( ) )
            {
                //ds check if it is linked
                const Token* pcTokenLinkEnd( pcTokenLinkStart->link( ) );

                //ds if it worked
                if( 0 != pcTokenLinkEnd )
                {
                    //ds call the check procedure
                    checkAssert( pcTokenLinkStart, pcTokenLinkEnd );
                }
                else
                {
                    //ds sometimes the link call does not work, we have to search for the ( manually TODO implement safer link search
                    pcTokenLinkEnd = _getLink( pcTokenLinkStart );

                    //ds if we got a link end
                    if( 0 != pcTokenLinkEnd )
                    {
                        //ds call the check procedure
                        checkAssert( pcTokenLinkStart, pcTokenLinkEnd );
                    }
                    else
                    {
                        //ds not a real assert statement (should not happen) TODO throw
                    }
                }
            }
        }

        //ds check if we got a comment
        else if( Token::eComment == pcCurrent->type( ) )
        {
            //ds call comment checking procedure
            checkComment( pcCurrent );
        }

        //ds look for an indent error
        else if( Token::eIndent == pcCurrent->type( ) )
        {
            //ds start indent check
            checkIndent( pcCurrent );
        }

        //ds always check for boost pointer initializations (unfortunately cppcheck does not recognize boost::shared_ptr< char > test( new char[123] );)
        if( "shared_ptr" == pcCurrent->str( ) || "scoped_ptr" == pcCurrent->str( ) )
        {
            //ds call the check procedure
            checkBoostPointer( pcCurrent );
        }
    }
}

void CCheckStyle::checkCompleteError( const Token* p_Token, const std::string& p_strErrorInformation, const Severity::SeverityType& p_cSeverity  )
{
    //ds report the error
    reportError( p_Token, p_cSeverity, "checkNames", p_strErrorInformation );
}

void CCheckStyle::checkPrefixFunction( const Token* p_pcToken, const Function* p_pcFunction, const std::string& p_strFunctionScopePrefix )
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
            checkCompleteError( p_pcToken, "prefix of " + p_strFunctionScopePrefix + ": " + p_pcFunction->name( ) + "( ) is invalid - correct prefix: " + '_', Severity::style );
        }
    }
    else
    {
        //ds we got and error if first character is a '_' character
        if( p_pcFunction->name( )[0] == '_' )
        {
            //ds trigger error message
            checkCompleteError( p_pcToken, "prefix of " + p_strFunctionScopePrefix + ": " + p_pcFunction->name( ) + "( ) is invalid - prefix: " + '_' + " is only allowed for methods", Severity::style );
        }
    }
}

void CCheckStyle::checkPrefixVariable( const Token* p_pcToken, const Variable* p_pcVariable, const std::string& p_strVariableScopePrefix )
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
    std::string strCorrectTypePrefix( m_mapWhitelist[_filterVariableTypeForWhiteList( strTypeName )] );

    //ds check no type prefix could not be found - this is the case for all user defined classes/structs not covered in the whitelist
    if( true == strCorrectTypePrefix.empty( ) )
    {
        //ds check if we are handling a class without special prefixes (unlike std::string), unfortunately this does not work for class* and unresolved classes
        if( true == p_pcVariable->isClass( ) )
        {
            //ds trigger the class prefix
            strCorrectTypePrefix = m_mapWhitelist["class"];
        }

        //ds check if its a pointer or address (if the token before the name is a * or a &) because cppcheck does not recognize unknown classes with these tokens following
        else if( ( "*"  == p_pcVariable->typeEndToken( )->str( ) || "&"  == p_pcVariable->typeEndToken( )->str( ) ) &&
                  false == p_pcVariable->typeStartToken( )->isStandardType( )                                       )
        {
            //ds trigger the class prefix
            strCorrectTypePrefix = m_mapWhitelist["class"];
        }
        else
        {
            //ds trigger error message (only informative error - however this should almost never be the case because every unknown structure gets detected as a class/struct and receives the c prefix)
            checkCompleteError( p_pcToken, "no matching prefix found for type: " + strTypeName, Severity::information );
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
                checkCompleteError( p_pcToken, "forbidden use of pointer for class: " + strTypeName + " " + p_pcVariable->name( ) + " - please use: boost::shared_ptr< " + _filterVariableTypeSimple( strTypeName ) + " >", Severity::style );
            }

            //ds case for types entered in the whitelist but not detected as classes
            if( false == p_pcVariable->typeStartToken( )->isStandardType( ) )
            {
                //ds inform user
                checkCompleteError( p_pcToken, "forbidden use of pointer for class: " + strTypeName + " " + p_pcVariable->name( ) + " - please use: boost::shared_ptr< " + _filterVariableTypeSimple( strTypeName ) + " >", Severity::style );
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
        checkCompleteError( p_pcToken, "use of forbidden type: " + strTypeName + " in " + p_strVariableScopePrefix + ": " + strTypeName + " " + p_pcVariable->name( ), Severity::style );

        //ds fatal - skip processing
        return;
    }

    //ds check if there is a variable name (waited until now to display type information)
    if( 0 == p_pcVariable->name( ).length( ) )
    {
        //ds trigger error message (only informative error)
        checkCompleteError( p_pcToken, "no variable name for type: " + strTypeName, Severity::style );

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
                checkCompleteError( p_pcToken, "prefix of " + p_strVariableScopePrefix + ": " +  strTypeName + " " + p_pcVariable->name( ) + " is invalid - correct prefix: " + strCorrectTypePrefix, Severity::style );
            }
        }
    }
    else
    {
        //ds trigger error message
        checkCompleteError( p_pcToken, "name of " + p_strVariableScopePrefix + ": " +  strTypeName + " " + p_pcVariable->name( ) + " is too short - correct prefix: " + strCorrectTypePrefix, Severity::style );

        //ds skip processing
        return;
    }
}

void CCheckStyle::checkAssert( const Token* p_pcTokenStart, const Token* p_pcTokenEnd )
{
    //ds check input
    if( 0 == p_pcTokenStart || 0 == p_pcTokenEnd )
    {
        //ds invalid call (TODO throw)
        std::cout << "<CCheckStyle>[checkAssert] error: received null pointer Token" << std::endl;

        //ds skip
        return;
    }

    //ds check if a bracket follows the token, checkAssertion must be called from the "assert" Token so a "(" Token has to follow
    if( "(" != p_pcTokenStart->str( ) || ")" != p_pcTokenEnd->str( ) )
    {
        //ds invalid call (TODO throw)
        std::cout << "<CCheckStyle>[checkAssert] error: invalid function call" << std::endl;

        //ds skip
        return;
    }

    //ds information string which covers the complete assert statement
    std::string strAssertStatement( "" );

    //ds get the complete statement here because the next loop breaks when an error is found
    for( const Token* itToken = p_pcTokenStart->next( ); itToken != p_pcTokenEnd; itToken = itToken->next( ) )
    {
        //ds add all characters with a space
        strAssertStatement += itToken->str( );
        strAssertStatement += " ";
    }

    //ds check all arguments between the two links of the assert call
    for( const Token* itToken = p_pcTokenStart->next( ); itToken != p_pcTokenEnd; itToken = itToken->next( ) )
    {
        //ds no increment/decrement operations allowed
        if( "++" == itToken->str( ) || "--" == itToken->str( ) )
        {
            //ds trigger error message
            checkCompleteError( p_pcTokenStart, "assert statement: assert( " + strAssertStatement + ") includes forbidden operation: " + itToken->str( ), Severity::style );
        }

        //ds no function calls allowed
        if( Token::eFunction == itToken->type( ) )
        {
            //ds trigger error message
            checkCompleteError( p_pcTokenStart, "assert statement: assert( " + strAssertStatement + ") includes forbidden function call: " + itToken->str( ) + "( )", Severity::style );
        }

        //ds in case cppcheck does not recognize a function in the statement we reject any ( ) operation
        if( "(" == itToken->str( ) )
        {
            //ds trigger error message
            checkCompleteError( p_pcTokenStart, "assert statement: assert( " + strAssertStatement + ") includes forbidden function call: " + itToken->previous( )->str( ) + "( )", Severity::style );
        }
    }
}

void CCheckStyle::checkComment( const Token* p_pcToken )
{
    //ds check input
    if( 0 == p_pcToken )
    {
        //ds invalid call (TODO throw)
        std::cout << "<CCheckStyle>[checkComment] error: received null pointer Token" << std::endl;

        //ds skip
        return;
    }

    //ds get comment
    const std::string strComment( p_pcToken->str( ) );

    //ds check for too short comments (shortest allowed is //! or /**)
    if( 3 > strComment.length( ) )
    {
        checkCompleteError( p_pcToken, "single-line comment: \"" + strComment + "\" is too short", Severity::style );

        //ds fatal - skip further checks
        return;
    }

    //ds check if single line or not
    if( true == p_pcToken->isSingleLine( ) )
    {
        //ds if there is a space after the opening its invalid (we use substr and not char[] operations because substr can throw)
        if( "// " == strComment.substr( 0, 3 ) )
        {
            checkCompleteError( p_pcToken, "single-line comment: \"" + strComment + "\" has invalid format - correct: \"//xx ...\" or \"//! ...\"", Severity::style );
        }

        //ds //!
        else if( "//!" == strComment.substr( 0, 3 ) )
        {
            //ds there must be a space after the !
            if( 4 <= strComment.length( ) && " " != strComment.substr( 3, 1 ) )
            {
                checkCompleteError( p_pcToken, "single-line comment: \"" + strComment + "\" has invalid format - correct: \"//! ...\"", Severity::style );
            }
        }

        //ds //x or //xx or //xxx..
        else
        {
            //ds there must be a space after the second initial but no space after the first
            if( 5 <= strComment.length( ) && ( " " != strComment.substr( 4, 1 ) || " " == strComment.substr( 3, 1 ) ) )
            {
                checkCompleteError( p_pcToken, "single-line comment: \"" + strComment + "\" has invalid format - correct: \"//xx ...\"", Severity::style );
            }
        }
    }

    //ds multi line comment
    else
    {
        //ds check for the second star
        if( "/**" != strComment.substr( 0, 3 ) )
        {
            checkCompleteError( p_pcToken, "multi-line comment: \"" + strComment + "\" has invalid format - correct: \"/** ...\"", Severity::style );
        }

        //ds TODO implemented further checks
    }
}

void CCheckStyle::checkIndent( const Token* p_pcToken )
{
    //ds check input
    if( 0 == p_pcToken )
    {
        //ds invalid call (TODO throw)
        std::cout << "<CCheckStyle>[checkIndent] error: received null pointer Token" << std::endl;

        //ds skip
        return;
    }

    //ds we do not have to detect an error here, the overhanded token is already an invalid indent - determine the case
    if( "{" == p_pcToken->str( ) )
    {
        //ds inform as detailed as possible
        checkCompleteError( p_pcToken, "invalid indent format for opening character: '{' - please make sure there is exactly one newline character before the opening character", Severity::style );
    }
    else if( "}" == p_pcToken->str( ) )
    {
        //ds inform as detailed as possible
        checkCompleteError( p_pcToken, "invalid indent format for closing character: '}' - please make sure there is one newline character minimum before the closing character", Severity::style );
    }
    else
    {
        //ds invalid call (TODO throw)
        std::cout << "<CCheckStyle>[checkIndent] error: invalid function call" << std::endl;

        //ds skip
        return;
    }
}

//ds TODO implement the parsing of boost::shared_ptr< char > pPointer1( new char[10] ); then the UGLY checkBoostPointer( ) can be removed
void CCheckStyle::checkBoostPointer( const Token* p_pcToken )
{
    //ds check input
    if( 0 == p_pcToken )
    {
        //ds invalid call (TODO throw)
        std::cout << "<CCheckStyle>[checkBoostPointer] error: received null pointer Token" << std::endl;

        //ds skip
        return;
    }

    //ds configurations are assumed:
    //ds 1: boost::shared_ptr< char > pPointer1( new char[10] ); -> cppcheck can not parse this
    //ds 2: boost::shared_ptr< char > pPointer1 = new char[10]; -> cppcheck parses to: boost::shared_ptr< char > pPointer1; pPointer1 = new char[10];

    //ds get the pointer type (shared_ptr or scoped_ptr)
    const std::string strPointerName( p_pcToken->str( ) );

    //ds get the end of the type definition
    const Token* pcEndToken( p_pcToken->findmatch( p_pcToken, ";" ) );

    //ds escape if no end was found
    if( 0 == pcEndToken )
    {
        //ds TODO throw - fatal
        return;
    }

    //ds pointer name
    std::string strVariableName( "" );

    //ds check if we have a previous token to check
    if( 0 != pcEndToken->previous( ) )
    {
        //ds determine which configuration we have - first case: boost::shared_ptr< char > pPointer1( new char[10] )
        if( ")" == pcEndToken->previous( )->str( ) )
        {
            //ds get the link start
            const Token* pcTokenLinkStart( _getLinkInverse( pcEndToken->previous( ) ) );

            //ds the variable name has to be the previous token
            if( 0 != pcTokenLinkStart && pcTokenLinkStart->previous( ) )
            {
                //ds we can directly get the pointers name
                strVariableName = pcTokenLinkStart->previous( )->str( );
            }
            else
            {
                //ds TODO throw - fatal
                return;
            }
        }
        else
        {
            //ds the pointers name must be right before the first ;
            if(  true == pcEndToken->previous( )->isName( ) )
            {
                //ds get the name
                strVariableName = pcEndToken->previous( )->str( );

                //ds there is a second semicolon - shift the end token
                pcEndToken = pcEndToken->next( );

                //ds if we could shift
                if( 0 != pcEndToken )
                {
                    //ds look for the final semicolon
                    pcEndToken = pcEndToken->findmatch( pcEndToken, ";" );
                }
                else
                {
                    //ds TODO throw - fatal
                    return;
                }
            }
        }
    }

    //ds type name (gets built up during array search - this is possible because the type must appear before the new call)
    std::string strTypeName( "" );

    //ds bracket counter in order to recursive information
    int iOpenBrackets( 0 );

    //ds check if new call was found
    bool bIsNewCallFound( false );

    //ds new position token
    const Token* pcTokenNew( 0 );

    //ds if the end token is valid
    if( 0 != pcEndToken )
    {
        //ds find the argument before the end token
        for( const Token* itToken = p_pcToken->next( ); itToken != pcEndToken; itToken = itToken->next( ) )
        {
            //ds check if an opening bracket is found
            if( "<" == itToken->str( ) )
            {
                ++iOpenBrackets;

                continue;
            }

            //ds check if a closing bracket is found
            if( ">" == itToken->str( ) )
            {
                --iOpenBrackets;

                continue;
            }

            //ds as long as there are open brackets record the information in between
            if( 0 != iOpenBrackets )
            {
                strTypeName += itToken->str( );
            }

            //ds look for a new call (this is no violation yet)
            if( "new" == itToken->str( ) )
            {
                //ds save the token
                pcTokenNew = itToken;

                bIsNewCallFound = true;
            }

            //ds escape if we reach a function or parameter end
            if( ")" == itToken->str( ) )
            {
                return;
            }

            //ds once the new call is found a following array opener [
            if( true == bIsNewCallFound && "[" == itToken->str( ) )
            {
                std::string strNewTypeName( "" );

                //ds get the complete type name after the new call
                for( const Token* pcInnerToken = pcTokenNew->next( ); 0 != pcInnerToken; pcInnerToken = pcInnerToken->next( ) )
                {
                    //ds break if we land at the array start
                    if( itToken == pcInnerToken )
                    {
                        break;
                    }

                    //ds add up all characters until the new operator is hit
                    strNewTypeName += pcInnerToken->str( );
                }

                //ds check if the token before the '[' matches the template given to the boost pointer
                if( strTypeName == strNewTypeName )
                {
                    //ds trigger error message
                    checkCompleteError( p_pcToken, "forbidden array initialization of class: boost::" + strPointerName + "< " + strTypeName + " > " + strVariableName + " - please use: boost::shared_array< " + strTypeName  + " >", Severity::style );
                }
                else
                {
                    //ds trigger error message 1 - this should not happen if the code compiles
                    checkCompleteError( p_pcToken, "invalid array initialization of class: boost::" + strPointerName + "< " + strTypeName + " > " + strVariableName + " with new < " + strNewTypeName + " > make sure the same type is used for the array allocation", Severity::style );

                    //ds trigger error message 2
                    checkCompleteError( p_pcToken, "forbidden array initialization of class: boost::" + strPointerName + "< " + strTypeName + " > " + strVariableName + " - please use: boost::shared_array< " + strTypeName  + " >", Severity::style );
                }
            }
        }
    }
}

bool CCheckStyle::_isBoostPointer( const std::string& p_strTypeName ) const
{
    //ds check all valid cases - the string has to start with the pointer
    if( 16 < p_strTypeName.length( ) && "boost::shared_ptr" == p_strTypeName.substr( 0, 17 ) )
    {
        return true;
    }
    else if( 9 < p_strTypeName.length( ) && "shared_ptr" == p_strTypeName.substr( 0, 10 ) )
    {
        return true;
    }
    else if( 16 < p_strTypeName.length( ) && "boost::scoped_ptr" == p_strTypeName.substr( 0, 17 ) )
    {
        return true;
    }
    else if( 9 < p_strTypeName.length( ) && "scoped_ptr" == p_strTypeName.substr( 0, 10 ) )
    {
        return true;
    }
    else
    {
        //ds nothing found
        return false;
    }
}

bool CCheckStyle::_isBoostArray( const std::string& p_strTypeName ) const
{
    //ds check all valid cases - the string has to start with the pointer
    if( 18 < p_strTypeName.length( ) && "boost::shared_array" == p_strTypeName.substr( 0, 19 ) )
    {
        return true;
    }
    else if( 11 < p_strTypeName.length( ) && "shared_array" == p_strTypeName.substr( 0, 12 ) )
    {
        return true;
    }
    else if( 18 < p_strTypeName.length( ) && "boost::scoped_array" == p_strTypeName.substr( 0, 19 ) )
    {
        return true;
    }
    else if( 11 < p_strTypeName.length( ) && "scoped_array" == p_strTypeName.substr( 0, 12 ) )
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

    //ds check if we got an unsigned type (always has to be the first token) - cppcheck cuts the const specifier out too but we don't have to check for that
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

const std::string CCheckStyle::_filterVariableTypeForWhiteList( const std::string& p_strType ) const
{
    //ds final string to return - filter already some characters out
    std::string strTypeFiltered( _filterVariableTypeSimple( p_strType ) );

    //ds check for a template inside of the type
    const unsigned int uStartTemplate( static_cast< unsigned int >( p_strType.find( '<' ) ) );

    //ds get the container type if present
    if( static_cast< unsigned int >( std::string::npos ) != uStartTemplate )
    {
        //ds first check if an iterator is present (since it overwrites all prefixes -> std::map< etc >::iterator is always iterator for the whitelist)
        if( 13 < p_strType.length( ) && "const_iterator" == p_strType.substr( p_strType.length( ) - 14, 14 ) )
        {
            //ds simplify the type
            strTypeFiltered = "const_iterator";
        }

        //ds regular iterator
        else if( 7 < p_strType.length( ) && "iterator" == p_strType.substr( p_strType.length( ) - 8, 8 ) )
        {
            //ds always iterator
            strTypeFiltered = "iterator";
        }

        //ds for all other cases we cut off the whole template part (since it does not matter for the prefix)
        else
        {
            //ds get the type before the template: (e.g. std::pair< SomeThing, SomeOtherThing > -> std::pair)
            strTypeFiltered = p_strType.substr( 0, uStartTemplate );
        }
    }

    return strTypeFiltered;
}

const std::string CCheckStyle::_filterVariableTypeSimple( const std::string& p_strType ) const
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

const Token* CCheckStyle::_getLink( const Token* p_pcTokenStart ) const
{
    //ds check input
    if( 0 == p_pcTokenStart )
    {
        //ds invalid call (TODO throw)
        std::cout << "<CCheckStyle>[_getLink] error: received null pointer Token" << std::endl;

        return 0;
    }

    //ds characters to check
    const std::string strCharacterStart( p_pcTokenStart->str( ) );
    std::string strCharacterEnd( "" );

    //ds get the correct character for the input
    if( "(" == strCharacterStart ){ strCharacterEnd = ")"; }
    else if( "<" == strCharacterStart ){ strCharacterEnd = ">"; }
    else if( "{" == strCharacterStart ){ strCharacterEnd = "}"; }
    else if( "[" == strCharacterStart ){ strCharacterEnd = "]"; }

    //ds bracket counter in order to recursive information
    int iOpenBrackets( 0 );

    //ds start looping
    for( const Token* pcToken = p_pcTokenStart; 0 != pcToken; pcToken = pcToken->next( ) )
    {
        //ds check if an opening bracket is found
        if( strCharacterStart == pcToken->str( ) )
        {
            ++iOpenBrackets;
        }

        //ds check if a closing bracket is found
        else if( strCharacterEnd == pcToken->str( ) )
        {
            --iOpenBrackets;
        }

        //ds if we reach 0 total we scanned the whole body
        if( 0 == iOpenBrackets )
        {
            //ds return the current token (final link)
            return pcToken;
        }
    }

    //ds return null pointer if nothing was found
    return 0;
}

const Token* CCheckStyle::_getLinkInverse( const Token* p_pcTokenEnd ) const
{
    //ds check input
    if( 0 == p_pcTokenEnd )
    {
        //ds invalid call (TODO throw)
        std::cout << "<CCheckStyle>[_getLinkInverse] error: received null pointer Token" << std::endl;

        return 0;
    }

    //ds characters to check
    std::string strCharacterStart( "" );
    const std::string strCharacterEnd( p_pcTokenEnd->str( ) );

    //ds get the correct character for the input
    if( ")" == strCharacterEnd ){ strCharacterStart = "("; }
    else if( ">" == strCharacterEnd ){ strCharacterStart = "<"; }
    else if( "}" == strCharacterEnd ){ strCharacterStart = "{"; }
    else if( "]" == strCharacterEnd ){ strCharacterStart = "["; }

    //ds bracket counter in order to recursive information
    int iOpenBrackets( 0 );

    //ds start looping
    for( const Token* pcToken = p_pcTokenEnd; 0 != pcToken; pcToken = pcToken->previous( ) )
    {
        //ds check if an opening bracket is found
        if( strCharacterStart == pcToken->str( ) )
        {
            ++iOpenBrackets;
        }

        //ds check if a closing bracket is found
        else if( strCharacterEnd == pcToken->str( ) )
        {
            --iOpenBrackets;
        }

        //ds if we reach 0 total we scanned the whole body
        if( 0 == iOpenBrackets )
        {
            //ds return the current token (final link)
            return pcToken;
        }
    }

    //ds return null pointer if nothing was found
    return 0;
}

bool CCheckStyle::_isChecked( const Function* p_cFunction ) const
{
    //ds check input
    if( 0 == p_cFunction )
    {
        //ds invalid call (TODO throw)
        std::cout << "<CCheckStyle>[_isChecked] error: received null pointer Function" << std::endl;

        return false;
    }

    //ds success bool
    bool bHasBeenFound( false );

    for( std::vector< Function >::const_iterator itFunction = m_vecParsedFunctionList.begin( ); itFunction != m_vecParsedFunctionList.end( ); ++itFunction )
    {
        //ds check the name and type
        if( itFunction->name( )        == p_cFunction->name( )       &&
            itFunction->type           == p_cFunction->type          &&
            itFunction->minArgCount( ) == itFunction->minArgCount( ) )
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
        std::cout << "<CCheckStyle>[_isChecked] error: received null pointer Variable" << std::endl;

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
