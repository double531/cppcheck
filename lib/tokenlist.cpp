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

//---------------------------------------------------------------------------
#include "tokenlist.h"
#include "mathlib.h"
#include "path.h"
#include "preprocessor.h"
#include "settings.h"
#include "errorlogger.h"

#include <cstring>
#include <sstream>
#include <cctype>
#include <stack>


TokenList::TokenList(const Settings* settings) :
    _front(0),
    _back(0),
    _settings(settings),
    m_pcCustomTokenFront( 0 ),
    m_pcCustomTokenBack( 0 )
{
}

TokenList::~TokenList()
{
    deallocateTokens();
}

//---------------------------------------------------------------------------

// Deallocate lists..
void TokenList::deallocateTokens()
{
    deleteTokens(_front);
    _front = 0;
    _back = 0;

    //ds clean up all tokens
    deleteTokens( m_pcCustomTokenFront );
    m_pcCustomTokenFront = 0;
    m_pcCustomTokenBack = 0;

    _files.clear();
}

void TokenList::deleteTokens(Token *tok)
{
    while (tok) {
        Token *next = tok->next();
        delete tok;
        tok = next;
    }
}

//---------------------------------------------------------------------------
// add a token.
//---------------------------------------------------------------------------

void TokenList::addtoken(const char str[], const unsigned int lineno, const unsigned int fileno, bool split)
{
    if (str[0] == 0)
        return;

    // If token contains # characters, split it up
    if (split && std::strstr(str, "##")) {
        std::string temp;
        for (unsigned int i = 0; str[i]; ++i) {
            if (std::strncmp(&str[i], "##", 2) == 0) {
                addtoken(temp.c_str(), lineno, fileno, false);
                temp.clear();
                addtoken("##", lineno, fileno, false);
                ++i;
            } else
                temp += str[i];
        }
        addtoken(temp.c_str(), lineno, fileno, false);
        return;
    }

    // Replace hexadecimal value with decimal
    std::ostringstream str2;
    if (MathLib::isHex(str) || MathLib::isOct(str) || MathLib::isBin(str)) {
        str2 << MathLib::toLongNumber(str);
    } else if (std::strncmp(str, "_Bool", 5) == 0) {
        str2 << "bool";
    } else {
        str2 << str;
    }

    if (_back) {
        _back->insertToken(str2.str());
    } else {
        _front = new Token(&_back);
        _back = _front;
        _back->str(str2.str());
    }

    _back->linenr(lineno);
    _back->fileIndex(fileno);
}

void TokenList::addtoken(const Token * tok, const unsigned int lineno, const unsigned int fileno)
{
    if (tok == 0)
        return;

    if (_back) {
        _back->insertToken(tok->str());
    } else {
        _front = new Token(&_back);
        _back = _front;
        _back->str(tok->str());
    }

    _back->linenr(lineno);
    _back->fileIndex(fileno);
    _back->isUnsigned(tok->isUnsigned());
    _back->isSigned(tok->isSigned());
    _back->isLong(tok->isLong());
    _back->isUnused(tok->isUnused());

    //ds import the token type
    _back->type( tok->type( ) );

    //ds always import the scope
    _back->scope( tok->scope( ) );

    //ds import special attributes (only one possible at a time)
    if( 0 != tok->function( ) )
    {
        //ds set the function pointer
        _back->function( tok->function( ) );
    }
    else if( 0 != tok->variable( ) )
    {
        //ds set all variable attributes
        _back->variable( tok->variable( ) );
        _back->varId( tok->varId( ) );
    }
}

//ds custom token adding
void TokenList::addCustomToken( const std::string& p_strToken, const unsigned int& p_uLineNumber, const unsigned int& p_uFileIndex, const Token::Type& p_cType )
{
    //ds add the token to our list (check if back is set)
    if( 0 != m_pcCustomTokenBack )
    {
        //ds simply add the token
        m_pcCustomTokenBack->insertToken( p_strToken );

        //ds add the token type
        m_pcCustomTokenBack->type( p_cType );
    }
    else
    {
        //ds if back is not set yet create a new token
        m_pcCustomTokenFront = new Token( &m_pcCustomTokenBack );

        //ds link back to it
        m_pcCustomTokenBack = m_pcCustomTokenFront;

        //ds add the token name
        m_pcCustomTokenBack->str( p_strToken );

        //ds add the token type
        m_pcCustomTokenBack->type( p_cType );
    }

    //ds add line and fileno
    m_pcCustomTokenBack->linenr( p_uLineNumber );
    m_pcCustomTokenBack->fileIndex( p_uFileIndex );
}
//---------------------------------------------------------------------------
// InsertTokens - Copy and insert tokens
//---------------------------------------------------------------------------

void TokenList::insertTokens(Token *dest, const Token *src, unsigned int n)
{
    std::stack<Token *> link;

    while (n > 0) {
        dest->insertToken(src->str());
        dest = dest->next();

        // Set links
        if (Token::Match(dest, "(|[|{"))
            link.push(dest);
        else if (!link.empty() && Token::Match(dest, ")|]|}")) {
            Token::createMutualLinks(dest, link.top());
            link.pop();
        }

        dest->fileIndex(src->fileIndex());
        dest->linenr(src->linenr());
        dest->varId(src->varId());
        dest->type(src->type());
        dest->isUnsigned(src->isUnsigned());
        dest->isSigned(src->isSigned());
        dest->isPointerCompare(src->isPointerCompare());
        dest->isLong(src->isLong());
        dest->isUnused(src->isUnused());
        src  = src->next();
        --n;
    }
}

//---------------------------------------------------------------------------
// Tokenize - tokenizes a given file.
//---------------------------------------------------------------------------

bool TokenList::createTokens(std::istream &code, const std::string& file0, const std::string& p_strRawCode)
{
    _files.push_back(file0);

    // line number in parsed code
    unsigned int lineno = 1;

    // The current token being parsed
    std::string CurrentToken;

    // lineNumbers holds line numbers for files in fileIndexes
    // every time an include file is completely parsed, last item in the vector
    // is removed and lineno is set to point to that value.
    std::stack<unsigned int> lineNumbers;

    // fileIndexes holds index for _files vector about currently parsed files
    // every time an include file is completely parsed, last item in the vector
    // is removed and FileIndex is set to point to that value.
    std::stack<unsigned int> fileIndexes;

    // FileIndex. What file in the _files vector is read now?
    unsigned int FileIndex = 0;

    bool expandedMacro = false;

    // Read one byte at a time from code and create tokens
    for (char ch = (char)code.get(); code.good(); ch = (char)code.get()) {
        if (ch == Preprocessor::macroChar) {
            while (code.peek() == Preprocessor::macroChar)
                code.get();
            ch = ' ';
            expandedMacro = true;
        } else if (ch == '\n') {
            expandedMacro = false;
        }

        // char/string..
        // multiline strings are not handled. The preprocessor should handle that for us.
        else if (ch == '\'' || ch == '\"') {
            std::string line;

            // read char
            bool special = false;
            char c = ch;
            do {
                // Append token..
                line += c;

                // Special sequence '\.'
                if (special)
                    special = false;
                else
                    special = (c == '\\');

                // Get next character
                c = (char)code.get();
            } while (code.good() && (special || c != ch));
            line += ch;

            // Handle #file "file.h"
            if (CurrentToken == "#file") {
                // Extract the filename
                line = line.substr(1, line.length() - 2);

                // Has this file been tokenized already?
                ++lineno;
                bool foundOurfile = false;
                fileIndexes.push(FileIndex);
                for (unsigned int i = 0; i < _files.size(); ++i) {
                    if (Path::sameFileName(_files[i], line)) {
                        // Use this index
                        foundOurfile = true;
                        FileIndex = i;
                    }
                }

                if (!foundOurfile) {
                    // The "_files" vector remembers what files have been tokenized..
                    _files.push_back(Path::simplifyPath(line.c_str()));
                    FileIndex = static_cast<unsigned int>(_files.size() - 1);
                }

                lineNumbers.push(lineno);
                lineno = 0;
            } else {
                // Add previous token
                addtoken(CurrentToken.c_str(), lineno, FileIndex);
                if (!CurrentToken.empty())
                    _back->setExpandedMacro(expandedMacro);

                // Add content of the string
                addtoken(line.c_str(), lineno, FileIndex);
                if (!line.empty())
                    _back->setExpandedMacro(expandedMacro);
            }

            CurrentToken.clear();

            continue;
        }

        if (ch == '.' &&
            CurrentToken.length() > 0 &&
            std::isdigit(CurrentToken[0])) {
            // Don't separate doubles "5.4"
        } else if (std::strchr("+-", ch) &&
                   CurrentToken.length() > 0 &&
                   std::isdigit(CurrentToken[0]) &&
                   (CurrentToken[CurrentToken.length()-1] == 'e' ||
                    CurrentToken[CurrentToken.length()-1] == 'E') &&
                   !MathLib::isHex(CurrentToken)) {
            // Don't separate doubles "4.2e+10"
        } else if (CurrentToken.empty() && ch == '.' && std::isdigit(code.peek())) {
            // tokenize .125 into 0.125
            CurrentToken = "0";
        } else if (std::strchr("+-*/%&|^?!=<>[](){};:,.~\n ", ch)) {
            if (CurrentToken == "#file") {
                // Handle this where strings are handled
                continue;
            } else if (CurrentToken == "#line") {
                // Read to end of line
                std::string line;

                std::getline(code, line);

                // Update the current line number
                unsigned int row;
                if (!(std::stringstream(line) >> row))
                    ++lineno;
                else
                    lineno = row;
                CurrentToken.clear();
                continue;
            } else if (CurrentToken == "#endfile") {
                if (lineNumbers.empty() || fileIndexes.empty()) { // error
                    deallocateTokens();
                    return false;
                }

                lineno = lineNumbers.top();
                lineNumbers.pop();
                FileIndex = fileIndexes.top();
                fileIndexes.pop();
                CurrentToken.clear();
                continue;
            }

            addtoken(CurrentToken.c_str(), lineno, FileIndex, true);
            if (!CurrentToken.empty())
                _back->setExpandedMacro(expandedMacro);

            CurrentToken.clear();

            if (ch == '\n') {
                if (_settings->terminated())
                    return false;

                ++lineno;
                continue;
            } else if (ch == ' ') {
                continue;
            }

            CurrentToken += ch;
            // Add "++", "--", ">>" or ... token
            if (std::strchr("+-<>=:&|", ch) && (code.peek() == ch))
                CurrentToken += (char)code.get();
            addtoken(CurrentToken.c_str(), lineno, FileIndex);
            _back->setExpandedMacro(expandedMacro);
            CurrentToken.clear();
            continue;
        }

        CurrentToken += ch;
    }
    addtoken(CurrentToken.c_str(), lineno, FileIndex, true);
    if (!CurrentToken.empty())
        _back->setExpandedMacro(expandedMacro);
    _front->assignProgressValues();

    //ds create additional tokens if raw code is defined
    if( false == p_strRawCode.empty( ) )
    {
        createTokensRaw( p_strRawCode );
    }

    //ds set relative file paths just now if the raw code is not set else it gets set in the createTokensRaw function
    else
    {
        for(unsigned int i = 1; i < _files.size(); i++)
            _files[i] = Path::getRelativePath(_files[i], _settings->_basePaths);
    }

    return true;
}

bool TokenList::createTokensRaw( const std::string& p_strRawCode )
{
    //ds current file index we are working on
    unsigned int uCurrentFileIndex( 0 );

    //ds set to get file dependent line numbers
    std::map< unsigned int, unsigned int > mapLineNumbers;

    //ds stack of file indexes to parse (this means, for each new file we add an index and work on that until done, then remove it)
    std::stack< unsigned int > stRemainingFileIndexes;

    //ds maximum stack size, is necessary to keep the comment line numbers exact over n header files
    unsigned int uEndFileCounter( 0 );

    //ds start parsing the raw code if available
    if( false == p_strRawCode.empty( ) )
    {
        //ds loop over the raw code (with index because we use find functionality)
        for( unsigned int u = 0; u < p_strRawCode.length( ); ++u )
        {
            //ds check for #file or #endfile start
            if( '#' == p_strRawCode[u] )
            {
                //ds search a space
                const unsigned int uSeparator( static_cast< unsigned int >( p_strRawCode.find( ' ', u ) ) );

                //ds if it worked
                if( static_cast< unsigned int >( std::string::npos ) != uSeparator )
                {
                    //ds get the file specifier
                    const std::string strFileSpecifier( p_strRawCode.substr( u, uSeparator-u ) );

                    //ds search for a newline
                    const unsigned int uEndFilePath( static_cast< unsigned int >( p_strRawCode.find( '\n', uSeparator ) ) );

                    //ds if it worked
                    if( static_cast< unsigned int >( std::string::npos ) != uEndFilePath )
                    {
                        //ds get the filepath
                        const std::string strFilePath( p_strRawCode.substr( uSeparator+1, uEndFilePath-( uSeparator+1 ) ) );

                        //ds update the index counter to the new line to avoid redundant cases
                        u = uEndFilePath;

                        //ds determine the case
                        if( "#file" == strFileSpecifier )
                        {
                            //ds success flag
                            bool bWasFilePathFound( false );

                            //ds check if this file has been tokenized
                            for( unsigned int v = 0; v < _files.size( ); ++v )
                            {
                                if( Path::sameFileName( _files[v], strFilePath ) )
                                {
                                    //ds set variables
                                    uCurrentFileIndex = v;
                                    bWasFilePathFound = true;

                                    //ds add it to the stack
                                    stRemainingFileIndexes.push( v );
                                }
                            }

                            //ds if the filepath was not found
                            if( false == bWasFilePathFound )
                            {
                                //ds TODO throw, error - right now we just skip this file
                            }
                            else
                            {
                                //ds set the line numbers for the current file depending on the number of files before the current
                                mapLineNumbers[uCurrentFileIndex] = uEndFileCounter + static_cast< unsigned int >( _files.size( ) );
                            }
                        }
                        else if( "#endfile" == strFileSpecifier && 0 != stRemainingFileIndexes.size( ) )
                        {
                            //ds remove the current file index from the stack (it has to match)
                            if( uCurrentFileIndex == stRemainingFileIndexes.top( ) )
                            {
                                stRemainingFileIndexes.pop( );

                                //ds treated an endfile
                                ++uEndFileCounter;
                            }

                            //ds check if any indexes are left now
                            if( 0 != stRemainingFileIndexes.size( ) )
                            {
                                //ds update the current file index to the lower stack
                                uCurrentFileIndex = stRemainingFileIndexes.top( );
                            }
                        }
                    }
                }
            }

            //ds check for a possible comment beginning
            else if( '/' == p_strRawCode[u] && u+1 < p_strRawCode.length( ) )
            {
                //ds check next characters - first case one-line comment
                if( '/' == p_strRawCode[u+1] )
                {
                    //ds in the one-line case we just have to seek the endline /n
                    const unsigned int uEndLine( static_cast< unsigned int >( p_strRawCode.find( '\n', u ) ) );

                    //ds if we find one get the string
                    if( static_cast< unsigned int >( std::string::npos ) != uEndLine )
                    {
                        const std::string strCommentSingle( p_strRawCode.substr( u, uEndLine-u ) );

                        //ds add the token
                        addCustomToken( strCommentSingle, mapLineNumbers[uCurrentFileIndex], uCurrentFileIndex, Token::eComment );

                        //ds update current line number
                        ++mapLineNumbers[uCurrentFileIndex];

                        //ds update the index
                        u = uEndLine;
                    }
                }

                //ds second case multi-line comment
                else if( '*' == p_strRawCode[u+1] )
                {
                    //ds in the multi-line case we just have to seek the final "*/" sequence
                    const unsigned int uEndComment( static_cast< unsigned int >( p_strRawCode.find( "*/", u ) ) );

                    //ds if we found the end
                    if( static_cast< unsigned int >( std::string::npos ) != uEndComment )
                    {
                        //ds number of lines in comment (for accurate line number information in error)
                        unsigned int uLinesCommentMulti( 0 );

                        //ds get the lines of the multiline comment by count the newline characters in the sequence
                        for( unsigned int v = u; v < uEndComment; ++v )
                        {
                            //ds if a new line is found
                            if( '\n' == p_strRawCode[v] )
                            {
                                //ds increment the counter
                                ++uLinesCommentMulti;
                            }
                        }

                        //ds get the complete comment
                        const std::string strCommentMulti( p_strRawCode.substr( u, uEndComment-u ) );

                        //ds add the token
                        addCustomToken( strCommentMulti, mapLineNumbers[uCurrentFileIndex], uCurrentFileIndex, Token::eComment );

                        //ds update current line number
                        mapLineNumbers[uCurrentFileIndex] += uLinesCommentMulti ;

                        //ds update the index
                        u = uEndComment;
                    }
                }
            }

            //ds check for an indent error type 1 (backwards checking)
            else if( '{' == p_strRawCode[u] )
            {
                //ds during the search for the beginning of the {} statement we count the newlines we encounter
                unsigned int uNewLineCounter( 0 );

                //ds now we have to loop backwards until we engage a character which is not a whitespace: ' ', '\n' or '\t'
                for( int i = u-1; i >= 0; --i )
                {
                    //ds check for a statement start
                    if(  ' ' != p_strRawCode[i] &&
                        '\n' != p_strRawCode[i] &&
                        '\t' != p_strRawCode[i] )
                    {
                        //ds we reached the actual beginning of the {} statement, escape
                        break;
                    }

                    //ds if we found a newline operator
                    if( '\n' == p_strRawCode[i] )
                    {
                        //ds increment our counter
                        ++uNewLineCounter;
                    }
                }

                //ds before the { we want exactly 1 new line, not more not less
                if( 1 != uNewLineCounter )
                {
                    //ds add an invalid indent token
                    addCustomToken( "{", mapLineNumbers[uCurrentFileIndex], uCurrentFileIndex, Token::eIndent );
                }
            }

            //ds check for an indent error type 2 (backwards checking)
            else if( '}' == p_strRawCode[u] )
            {
                //ds during the search for the beginning of the {} statement we count the newlines we encounter
                unsigned int uNewLineCounter( 0 );

                //ds now we have to loop backwards until we engage a character which is not a whitespace: ' ', '\n' or '\t'
                for( int i = u-1; i >= 0; --i )
                {
                    //ds check for a statement start
                    if(  ' ' != p_strRawCode[i] &&
                        '\n' != p_strRawCode[i] &&
                        '\t' != p_strRawCode[i] )
                    {
                        //ds we reached the actual beginning of the {} statement, escape
                        break;
                    }

                    //ds if we found a newline operator
                    if( '\n' == p_strRawCode[i] )
                    {
                        //ds increment our counter
                        ++uNewLineCounter;
                    }
                }

                //ds before the closing } we want 1 new line minimum
                if( 0 == uNewLineCounter )
                {
                    //ds add an invalid indent token
                    addCustomToken( "}", mapLineNumbers[uCurrentFileIndex], uCurrentFileIndex, Token::eIndent );
                }
            }

            //ds check if we get a new line
            else if( '\n' == p_strRawCode[u] )
            {
                //ds increment current line counter
                ++mapLineNumbers[uCurrentFileIndex];
            }
        }
    }

    //ds set relative file paths now
    for( unsigned int i = 1; i < _files.size( ); i++ )
    {
        _files[i] = Path::getRelativePath( _files[i], _settings->_basePaths );
    }

    //ds default is sucess
    return true;
}

//---------------------------------------------------------------------------

void TokenList::createAst() const
{
    // operators that must be ordered according to C-precedence
    const char * const operators[] = {
        " :: ",
        " ++ -- . ",
        "> ++ -- + - ! ~ * & sizeof ",  // prefix unary operators, from right to left
        " * / % ",
        " + - ",
        " << >> ",
        " < <= > >= ",
        " == != ",
        " & ",
        " ^ ",
        " | ",
        " && ",
        " || ",
        " = ? : ",
        " throw ",
        " , "
        " [ "
    };

    // No tokens => bail out
    if (!_front)
        return;

    for (unsigned int i = 0; i < sizeof(operators) / sizeof(*operators); ++i) {
        // TODO: extract operators to std::set - that should be faster
        if (*operators[i] == '>') {  // Unary operators, parse from right to left
            const std::string op(1+operators[i]);
            Token *tok = _front;
            while (tok->next())
                tok = tok->next();
            for (; tok; tok = tok->previous()) {
                if ((!tok->previous() || tok->previous()->isOp()) &&
                    op.find(" "+tok->str()+" ")!=std::string::npos) {
                    tok->astOperand1(tok->next());
                }
            }
        } else {  // parse from left to right
            const std::string op(operators[i]);
            for (Token *tok = _front; tok; tok = tok->next()) {
                if (tok->astOperand1()==NULL && op.find(" "+tok->str()+" ")!=std::string::npos) {
                    if (tok->type() != Token::eIncDecOp) {
                        tok->astOperand1(tok->previous());
                        tok->astOperand2(tok->next());
                    } else if (tok->previous() && !tok->previous()->isOp()) {
                        tok->astOperand1(tok->previous());
                    }
                }
            }
        }
    }

    // function calls..
    for (Token *tok = _front; tok; tok = tok->next()) {
        if (Token::Match(tok, "%var% ("))
            tok->astFunctionCall();
    }

    // parentheses..
    for (Token *tok = _front; tok; tok = tok->next()) {
        if (Token::Match(tok, "(|)|]")) {
            tok->astHandleParentheses();
        }
    }
}

const std::string& TokenList::file(const Token *tok) const
{
    return _files.at(tok->fileIndex());
}

std::string TokenList::fileLine(const Token *tok) const
{
    return ErrorLogger::ErrorMessage::FileLocation(tok, this).stringify();
}
