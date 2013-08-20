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

#ifndef CHECKSTYLE_H
#define CHECKSTYLE_H

#include "config.h"
#include "check.h"
#include "settings.h"
#include "symboldatabase.h"



class Token;
class Function;
class Variable;

/// @addtogroup Checks
/// @{

//ds style checks
class CPPCHECKLIB CCheckStyle:public Check
{

//ds de/allocation
public:

    //ds constructor for registration
    CCheckStyle( );

    //ds constructor for test runs
    CCheckStyle( const Tokenizer* p_Tokenizer, const Settings* p_Settings, ErrorLogger* p_ErrorLogger );


//ds accessors
public:

    //ds regular checks
    void runChecks( const Tokenizer* p_Tokenizer, const Settings* p_Settings, ErrorLogger* p_ErrorLogger )
    {
        //ds create check instance
        CCheckStyle cChecker( p_Tokenizer, p_Settings, p_ErrorLogger );

        //ds execute checks
        //cChecker.dumpTokens( );
        cChecker.checkNames( );
    }

    //ds simple checks
    void runSimplifiedChecks( const Tokenizer* p_Tokenizer, const Settings* p_Settings, ErrorLogger* p_ErrorLogger )
    {
        //ds create check instance
        CCheckStyle cChecker( p_Tokenizer, p_Settings, p_ErrorLogger );
    }

//ds virtual inheritance
private:

    //ds framework method to display error messages
    void getErrorMessages( ErrorLogger* p_ErrorLogger, const Settings* p_Settings ) const
    {
        CCheckStyle c( 0, p_Settings, p_ErrorLogger );

        c.checkNamesError( 0, "", Severity::style );
    }

    //ds provide a description about the checks
    std::string classInfo( ) const
    {
        return "pepper coding guidelines checks\n";
    }

//ds methods
private:

    //ds displays all parsed tokens
    void dumpTokens( );

    //ds check names (only one function for higher efficiency since we only have to loop once through all tokens)
    void checkNames( );
    void checkNamesError( const Token* p_Token, const std::string p_strErrorInformation, const Severity::SeverityType p_cSeverity );

//ds helpers
private:

    //ds prefix checking for variables and functions
    void checkPrefix( const Token* p_pcToken, const Function* p_pcFunction, const std::string p_strFunctionScopePrefix = "function" );
    void checkPrefix( const Token* p_pcToken, const Variable* p_pcVariable, const std::string p_strVariableScopePrefix = "variable" );

    //ds check assertions
    void checkAssertion( const Token* p_pcToken );

    //ds retrieves the complete variable type consisting of multiple tokens (e.g std::string)
    const std::string _getVariableType( const Variable* p_pcVariable ) const;

    //ds returns a filtered version of the variable type (without *'s and &'s)
    const std::string _filterVariableType( const std::string p_strType ) const;

    //ds check vectors (overloaded for easy readability)
    bool _isChecked( const Function* p_cFunction ) const;
    bool _isChecked( const Variable* p_cVariable ) const;

//ds attributes
private:

    //ds whitelist for names (set in constructor)
    std::map< std::string, std::string > m_mapWhitelist;

    //ds vector containing all found functions
    std::vector< Function > m_vecParsedFunctionList;

    //ds vector containing all variables
    std::vector< Variable > m_vecParsedVariableList;
};

/// @}

#endif //CHECKSTYLE_H
