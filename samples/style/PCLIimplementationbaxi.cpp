#ifdef _WINDOWS
#if !defined( _X64 ) && !defined( UNDER_CE )
#include "PCLIimplementationbaxi.h"
#include "PCLIbaxierrors.h"

//ds execeptions
#include "PCLIexceptions.h"
#include "PGSUexceptions.h"

//ds specific includes
#include "PCLIframework1.h"
#include "PCLIticket.h"
#include "LOGGER.h"
#include "PCLIcardcountertable.h"
#include "PGSUparseriso78132.h"
#include "PCLIfileutilities.h"
#include "PCLIstringutilities.h"
#include "PCLIxmlutilities.h"

//me System includes
#include "Objbase.h" // for CoLoadLibrary

//dh this must always be the last include in every implementation file
#include "PCLIlastinclude.h"

namespace pep2
{

//ds global version variable >.>
const std::string CPCLIImplementationBAXI::version = "BAXI V. 1.60";

CPCLIImplementationBAXI::CPCLIImplementationBAXI( )
: m_bIsWrapperLoaded( false ),
_initializeWrapper( NULL ),
_exitWrapper( NULL ),
_openBAXI( NULL ),
_sendData( NULL ),
_processTransaction( NULL ),
_processAdministration( NULL ),
_transferCardData( NULL ),
_closeBAXI( NULL ),
_getBaxiwrapperVersion( NULL ),
m_lEnableVAT( eBAXI_NULL ),
m_lAmountVAT( 0 ),
m_lEnableCashback( eBAXI_TransactionPayment ),
m_lAmountCashback( 0 ),
m_eExpectedEncoding( ePPIITerminalCodePage_Latin1 )
{
    //ds log contruction
    LogInfo( _s( L"[Constructor] Implementation BAXI allocated." ) );

    //ds get fresh parameters instance
    m_pBAXIBasicParameters = boost::shared_ptr< CBAXIBasicParameters >( new CBAXIBasicParameters( ) );

    //ds initialize wrapper and remember result
    m_bIsWrapperLoaded = _loadWrapper( );

    //gv initialize parameters for baxiwrapper version
    char chBaxiwrapperVersion[257];
    memset( chBaxiwrapperVersion, 0, sizeof( chBaxiwrapperVersion) );
    unsigned int uPepperCountries = 0;
    unsigned int uPepperMajorRevision = 0;
    unsigned int uPepperMinorRevision = 0;
    unsigned int uPepperSubversionRevision = 0;
    unsigned int uPepperApiMajorRevision = 0;
    unsigned int uPepperApiMinorRevision = 0;
    unsigned int uPepperOsArchitecture = 0;
    unsigned int uPepperReleaseType = 0;
    unsigned int uPepperConfigurationType = 0;


    if( false == m_bIsWrapperLoaded )
    {
        LogError( _s( L">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" ) );
        LogError( _s( L"[Constructor] could not load baxiwrapper.dll." ) );
        LogError( _s( L">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" ) );
    }
    else
    {
        if ( true == _getBaxiwrapperVersion( chBaxiwrapperVersion, 
                                             & uPepperCountries,
                                             & uPepperMajorRevision,
                                             & uPepperMinorRevision,
                                             & uPepperSubversionRevision,
                                             & uPepperApiMajorRevision,
                                             & uPepperApiMinorRevision,
                                             & uPepperOsArchitecture,
                                             & uPepperReleaseType,
                                             & uPepperConfigurationType ) )
        {
            std::string strBaxiwrapperVersion = chBaxiwrapperVersion;
            
            LogInfo( _s( L">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" ) );
            LogInfo( _s( L"            baxiwrapper.dll version " ) +  TString::from_native_string( strBaxiwrapperVersion ) + _s( L" loaded") );
            LogInfo( _s( L">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" ) );
        }
        else
        {
            LogWarning( _s( L">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" ) );
            LogWarning( _s( L"     baxiwrapper.dll version loaded, but no version string available." ) );
            LogWarning( _s( L">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" ) );
        }
    }

//******************************************************************************** OLD PERSISTENCE ********************************************************************************

    //ds try to set shift sum
    try
    {
        //ds check if not set in persistence
        if( "" == CPCLIUtilities::getClassicPersistentProperty( "shiftSum" ) )
        {
            //ds initialize persistence with intial value
            CPCLIUtilities::setClassicPersistentProperty( "shiftSum", "0.0" );
        }

        //ds and set it to our instance
        shiftSum = ( TString::from_native_string( CPCLIUtilities::getClassicPersistentProperty( "shiftSum" ) ) ).to_numerical< double >( );

		LogInfo( TString::sprintf( _s( L"[Constructor] Shift Sum = %f" ) ) % shiftSum );
    }
    catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
    {
        //ds log warning
        LogWarning( _s( L"[Constructor] could not convert shiftSum to double." ) );
    }

//*********************************************************************************************************************************************************************************

}

CPCLIImplementationBAXI::~CPCLIImplementationBAXI( )
{
    //ds log destruction
    LogInfo( _s( L"[Destructor] Implementation BAXI deallocated." ) );

    //ds unload wrapper if possible
    if( NULL != _exitWrapper && true == m_bIsWrapperLoaded )
    {
        _exitWrapper( );
        LogInfo( _s( L"[Destructor] unloaded baxiwrapper.dll." ) );
    }
    else
    {
        LogInfo( _s( L"[Destructor] baxiwrapper.dll not unloaded." ) );
    }
}

long CPCLIImplementationBAXI::specificationDependantValidation( short method, boost::shared_ptr< CPCLIMessage > in_outMsg ) 
{
    //ds try block because we need several conversions for advanced logging
    try
    {
		LogInfo( TString::sprintf( _s( L"[Validation] method: %i" ) ) % method );

        switch( method )
        {
            case( EFTCONFIGDRIVER ):
            {  
                break;
            }
            case( EFTTRX ):
            {
				LogInfo( TString::sprintf( _s( L"[Validation] in_outMsg->trxType: %i" ) ) % static_cast< unsigned int>( in_outMsg->trxType ) );

                if  ( CURR_EURO          != in_outMsg->currency &&
                      CURR_SWEDEN_KRONA  != in_outMsg->currency &&
                      CURR_NORWAY_KRONE  != in_outMsg->currency &&
                      CURR_DENMARK_KRONE != in_outMsg->currency    )
                {
                    LogError( _s( L"[Validation] invalid currency." ) );
                    in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );
                    return eErrorCode_PARAMETER_VALIDATION;
                }
                
                //ds only track 0 allowed
                if( TRACK_NOT_AVAILABLE != in_outMsg->trackPresence )
                {
                    LogError( _s( L"[Validation] invalid track presence." ) );

                    //ds invalid track presence
                    in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );
                    return eErrorCode_PARAMETER_VALIDATION;
                }

                switch( in_outMsg->trxType ) 
                {
                    //ds fall through intended
                    case( TRXTYPE_RECOVERY ):
                    case( TRXTYPE_GOODS_PAYM ):
                    case( TRXTYPE_VOID_GOODS_PAYM ):
                    case( TRXTYPE_CASH ):
                    case( TRXTYPE_VOID_CASH_ADV ):
                    case( TRXTYPE_CREDIT ):
                    case( TRXTYPE_VOID_CREDIT ):
                    {
                        break;
                    }

                    case( TRXTYPE_REFERRAL ):
                        {
                            //gv for "Force Offline" it is also allowed to leave the Auth Nbr empty ==> therefore remove dummy nbr set in translateTrx()
                            if ( BAXI_DummyRefNbr == in_outMsg->trxRefNbrIn )
                            {
                                in_outMsg->trxRefNbrIn = "";
                            }
                            break;
                        }

                    case( TRXTYPE_BALANCE ):
                    {
                        //ds if amount is not zero
                        if( 0 != in_outMsg->getAmount( ) )
                        {
							LogError( TString::sprintf( _s( L"[Validation] AMOUNT: %f. Must be zero!" ) ) % in_outMsg->getAmount() );
                            //ds bad balance amount
                            in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );
                            return eErrorCode_PARAMETER_VALIDATION;
                        }
                        break;
                    }

                    //ds everything else is not implemented
                    default:
                    {
						LogError( TString::sprintf( _s( L"[Validation] transaction type: %i" ) ) % static_cast< unsigned int >( in_outMsg->trxType ) );

                        //ds transaction type not supported
                        in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );
                        return eErrorCode_PARAMETER_VALIDATION;
                    }
                }
            }

            //ds fall through intended
            case( EFTOPEN ):
            case( EFTCLOSE ):
            case( EFTENDOFDAY ):
            case( EFTVERSION ):
            {
                //ds okay
                break;
            }

            case ( EFTUTILITY ):
                {
                    if( "c" != in_outMsg->utilityOpCode &&
                        "d" != in_outMsg->utilityOpCode &&
                        "e" != in_outMsg->utilityOpCode &&
                        "s" != in_outMsg->utilityOpCode &&
                        "+" != in_outMsg->utilityOpCode &&
                        "z" != in_outMsg->utilityOpCode )
                    {
                        if (in_outMsg->allowUtilityJ==true &&
                            "j" == in_outMsg->utilityOpCode)
                        {
                            return eErrorCode_OK;
                        }
                        else
                        {
                            LogInfo( _s( L"the opcode " ) + TString::from_native_string( in_outMsg->utilityOpCode ) + _s( L" is not supported" ) );
                            return eErrorCode_PARAMETER_VALIDATION;
                            in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );
                        }
                    }
                    break;
                }

            //ds everything else is not supported
            default:
            {
				LogError( TString::sprintf( _s( L"[Validation] eft method: %i not supported." ) ) % static_cast< int >( method ) );

                //ds method not supported
                in_outMsg->setDisplayCode( eErrorCode_METHOD_NOT_SUPPORTED );
                return eErrorCode_METHOD_NOT_SUPPORTED;
            }
        }

        //ds if we are here no inalid input was found
        in_outMsg->setDisplayCode( eErrorCode_OK );
        return eErrorCode_OK;
    }
    catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
    {
        //ds conversion could not be done
        LogError( _s( L"[Validation] could not convert numerical to string." ) );

        //ds something must be wrong with the input
        in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );
        return eErrorCode_PARAMETER_VALIDATION;
    }
}

long CPCLIImplementationBAXI::stopProtocol( boost::shared_ptr< CPCLIMessage > in_outMsg )
{
    //ds always ok
    return eErrorCode_OK;
}

long CPCLIImplementationBAXI::startProtocol( boost::shared_ptr< CPCLIMessage > in_outMsg )
{
    //ds always ok
    return eErrorCode_OK;
}

void CPCLIImplementationBAXI::addTicketHeaderFooter( std::string printFile, boost::shared_ptr< CPCLIMessage > in_outMsg ) 
{
    //ds base class
    IPCLIImplementation::addTicketHeaderFooter( printFile, in_outMsg );
}

long CPCLIImplementationBAXI::initDevice( boost::shared_ptr< CPCLIMessage > in_outMsg )
{
    //ds initialize with base class
    long lReturnValue = IPCLIImplementation::initDevice( in_outMsg );
    if( eErrorCode_OK != lReturnValue )
    {
        LogError( _s( L"[initDevice] base class call failed." ) );

        //ds call failed
        in_outMsg->setDisplayCode( lReturnValue );
        return lReturnValue;
    }

    //ds log configuration
    try
    {
        LogInfo( TChar::space( ) + TChar::newline( ) + _s( L"---------------------------------------------< CONFIGURATION >---------------------------------------------" ) );
        LogInfo( TString::sprintf( _s( L"            TraceLevel: %i" ) ) % ( m_cBAXIConfigurationParameters.m_siTraceLevel ) );

		//bk long causes compile error in linux when used with sprintf as for linux long is not int. so cast it to int before
        LogInfo( TString::sprintf( _s( L"              BaudRate: %i" ) ) % ( static_cast< int >( m_cBAXIConfigurationParameters.m_lBaudRate ) ) );

        LogInfo( TString::sprintf( _s( L"              CommPort: %i" ) ) % ( m_cBAXIConfigurationParameters.m_siCommPort ) );
        LogInfo( TString::sprintf( _s( L"          PrinterWidth: %i" ) ) % ( m_cBAXIConfigurationParameters.m_siPrinterWidth ) );
        LogInfo( TString::sprintf( _s( L"          DisplayWidth: %i" ) ) % ( m_cBAXIConfigurationParameters.m_siDisplayWidth ) );
        LogInfo( TString::sprintf( _s( L"           MsgRouterOn: %i" ) ) % ( m_cBAXIConfigurationParameters.m_siMsgRouterOn ) );
        LogInfo( TString::sprintf( _s( L"IndicateEotTransaction: %i" ) ) % ( m_cBAXIConfigurationParameters.m_siIndicateEotTransaction ) );
        LogInfo( TString::sprintf( _s( L"         CutterSupport: %i" ) ) % ( m_cBAXIConfigurationParameters.m_siCutterSupport ) );
        LogInfo( TString::sprintf( _s( L"       PowerCycleCheck: %i" ) ) % ( m_cBAXIConfigurationParameters.m_siPowerCycleCheck ) );
        LogInfo( TString::sprintf( _s( L"        TidSupervision: %i" ) ) % ( m_cBAXIConfigurationParameters.m_siTidSupervision ) );
        LogInfo( TString::sprintf( _s( L"          EventVersion: %i" ) ) % ( m_cBAXIConfigurationParameters.m_siEventVersion ) );
        LogInfo( TString::sprintf( _s( L"   AutoGetCostumerInfo: %i" ) ) % ( m_cBAXIConfigurationParameters.m_siAutoGetCustomerInfo ) );
        LogInfo( _s( L"         HostIpAddress: " ) + TString::from_native_string( m_cBAXIConfigurationParameters.m_chHostIpAddress ) );

		//bk long causes compile error in linux when used with sprintf as for linux long is not int. so cast it to int before
        LogInfo( TString::sprintf( _s( L"              HostPort: %i" ) ) % ( static_cast< int >( m_cBAXIConfigurationParameters.m_lHostPort ) ) );

        LogInfo( _s( L"           LogFilePath: " ) + TString::from_native_string( m_cBAXIConfigurationParameters.m_chLogFilePath ) );
        LogInfo( _s( L"         LogFilePrefix: " ) + TString::from_native_string( m_cBAXIConfigurationParameters.m_chLogFilePrefix ) );
        LogInfo( _s( L"-----------------------------------------------------------------------------------------------------------" ) + TChar::newline() + TChar::space() );
    }
    catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
    {
        LogError( _s( L"[initDevice] could not convert numerical to string." ) );

        //ds essential for logging
        in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );
        return eErrorCode_PARAMETER_VALIDATION;
    }

    //ds open BAXI -> initialize BAXI
    if( eErrorCode_OK == send( EFTCONFIGDRIVER, in_outMsg ) )
    {
        //ds receive result
        lReturnValue = receive( in_outMsg );

        in_outMsg->setDisplayCode( lReturnValue );
        return lReturnValue;
    }
    else
    {
        LogError( _s( L"[initDevice] could not open BAXI.DLL or no hardware found." ) );

        //ds communication error between BAXI & terminal
        in_outMsg->setDisplayCode( eErrorCode_COMMUNICATION_NO_EFT );
        return eErrorCode_COMMUNICATION_NO_EFT;
    }
}

long CPCLIImplementationBAXI::setConnectedState( boost::shared_ptr< CPCLIMessage > in_outMsg )
{
    //ds always ok
    return eErrorCode_OK;
}

long CPCLIImplementationBAXI::setDisconnectedState( boost::shared_ptr< CPCLIMessage > in_outMsg )
{
    //ds always ok
    return eErrorCode_OK;
}

long CPCLIImplementationBAXI::send( short method, boost::shared_ptr< CPCLIMessage > in_outMsg )
{
    LogInfo( TChar::space( ) + TChar::newline( ) + _s( L"---------------------------------------------< COMMUNICATION >---------------------------------------------" ) );

    //ds we got many conversions from numerics to strings in here
    try
    {
      
        LogInfo( TString::sprintf( _s( L"[send] calling method: %i" ) ) % static_cast< int >( method ) );

        //ds well used parameter
        const TString strOperatorNumber( TString::from_numerical< short >( in_outMsg->operatorNbr ) );

        switch( method )
        {
        case( EFTCONFIGDRIVER ):
            {
                //ds initialize and open BAXI (CRASHES IF BAXI IS ALREADY OPENED)
                _logSendCall( _s( L"Open" ), _s( L" " ) );

                if( true == _openBAXI( &m_cBAXIConfigurationParameters ) )
                {
                    _logSendSuccess( _s( L"Open" ) );
                    return eErrorCode_OK;
                }
                else
                {
                    _logSendFailure( _s( L"Open" ) );
                    return eErrorCode_NOT_OK;
                }
            }

            case( EFTTRX ):
            {
                //ds well used transaction parameters (sadly longs)
                const TString strCardInformation( TString::from_native_string( in_outMsg->cardInformation ) );
                const long lAmount = static_cast< long >( in_outMsg->getAmount( ) );

                LogInfo( TString::sprintf( _s( L"[send] transaction type: %u" ) ) % static_cast< unsigned int >( in_outMsg->trxType ) );
                LogInfo( TString::sprintf( _s( L"[send] amount: %u" ) ) % static_cast< unsigned int >( lAmount ) );
                LogInfo( _s( L"[send] currency: " ) + TString::from_native_string( in_outMsg->currency ) );   
                LogInfo( TString::sprintf( _s( L"[send] track presence: %u" )) % static_cast< unsigned int >( in_outMsg->trackPresence ) );

                /*ds log track2 if entered
                if( TRACK_ISO2 == in_outMsg->trackPresence )
                {
                    LogInfo( _s( L"[send] track 2 data: " ) + TString::from_native_string( in_outMsg->cardInformation ) );
                }*/

                switch ( in_outMsg->trxType )
                {
                    /*
                    TransferAmount_V2 
                    PARAMETER: string OperID, long type1, long amount1, long type2, long 
                    amount2, long type3, long amount3, string data, string 
                    articleDetails 

                    TransferAmount_V4:
                    PARAMETER: string OperID, long type1, long amount1, long type2, long 
                    amount2, long type3, long amount3, string data, string 
                    articleDetails, string paymentConditionCode, string authCode

                    <TYPE 1> H30 = EFT Authorisation (KJOP) => Purchase amount
                             H31 = Return of Goods (RETU)
                             H32 = Reversal (ANNU) => Annulate last amount
                             H33 = Purchase with Cashback (KONT)
                             H36 = Balance Inquiry (DISP)
                             H38 = Deposit (INN)
                             H39 = Cash Withdrawal (UT)
                             H40 = Force Offline
                    <AMNT 1> Total amount. 
                    <TYPE 2> H30 = not in use
                    <AMNT 2> Only used if  <TYPE 1> = Purchase with Cashback (H33) – Total purchase amount. 
                    <TYPE 3> H30 = not in use
                             H31 = not in use
                             H32 = VAT (Value added tax) amount.  The total tax amount supplied by the ECR. 
                    <AMNT 3> VAT amount
                    <DATA>   Variable field length, max 40 digits alfanumeric. The data is to be sent to the HOST.  
                             The data characters must be in range H20 to H7F. This field is optional. 
                    <ART #>  Variable field, alphanumeric data. Each record is separated with H1e, ASCII RS, 
                             A maximum of 3 records can be sent in one message, each separated with RS.
                             The field is used to identify dedicated articles or reference to articles. This field is 
                             optional and will vary depending on the article/service.  These use cases are described 
                             in separate documents per article/service.
                    <PCC>    Payment Condition Code. Optional field.
                             Variable field length, max 3 alphanumeric char.
                    <AUTH. CODE>
                            Authorisation Code field. Variable field length, max 15 digits alfanumeric. 
                            Optional field. 
                            The field is used for Force Offline transactions.
                    */
                
                    case( TRXTYPE_GOODS_PAYM ):
                    {
						//dh copy to local representation
						//dh DO NOT USE CODE LIKE xxx.to_native_string().c_str() !!!!!
						std::string strBuffer( m_strBufferLoyaltyInfo.to_native_string() );

                        //ds process transaction with chosen params
                        _logSendCall( _s( L"TransferAmount_V4" ), _s( L"\"" ) + strOperatorNumber + _s( L", " ) +  TString::from_numerical< int >( m_lEnableCashback ) + _s( L", " ) +  TString::from_numerical< int >( lAmount ) + _s( L", 0x30" ) + TString::from_numerical< int >( m_lAmountCashback ) + _s( L", " ) + TString::from_numerical< int >( m_lEnableVAT ) + _s( L", " ) + TString::from_numerical< int >( m_lAmountVAT ) + _s( L", \"" ) + m_strBufferLoyaltyInfo + _s( L"\", \"\", \"\", \"\"" ) );
                        if( true == _processTransaction( &CBAXITransactionParameters( ( strOperatorNumber.to_native_string( ) ).c_str( ), m_lEnableCashback, lAmount, eBAXI_NULL, m_lAmountCashback, m_lEnableVAT, m_lAmountVAT, strBuffer.c_str( ), "", "", "", false ) ) )
                        {
                            _logSendSuccess( _s( L"TransferAmount_V4" ) );
                            return eErrorCode_OK;
                        }
                        else
                        {
                            _logSendFailure( _s( L"TransferAmount_V4" ) );

                            //gv in case we get a specific error code, set the display text accordingly
                            if( 0 != m_pBAXIBasicParameters->m_lLastError )
                            {
                                in_outMsg->displayText = ( _getErrorText( m_pBAXIBasicParameters->m_lLastError ) ).to_native_string( );
                                in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
                            }
                            return eErrorCode_NOT_OK;
                        }
                    }

                    //ds fall through intended
                    case( TRXTYPE_VOID_GOODS_PAYM ):
                    case( TRXTYPE_VOID_CASH_ADV ):
                    case( TRXTYPE_VOID_CREDIT ):
                    {
                        //ds reverse last transaction
                        _logSendCall( _s( L"TransferAmount_V4" ), _s( L"\"" ) + strOperatorNumber + _s( L"\", 0x32, " ) + TString::from_numerical< int >( lAmount ) + _s( L", 0x30, 0, 0x30, 0, \"\", \"\", \"\", \"\"" ) );
                        if( true == _processTransaction( &CBAXITransactionParameters( ( strOperatorNumber.to_native_string( ) ).c_str( ) , eBAXI_TransactionVoidPayment, lAmount, eBAXI_NULL, 0, eBAXI_NULL, 0, "", "", "", "", false ) ) )
                        {
                            _logSendSuccess( _s( L"TransferAmount_V4" ) );
                            return eErrorCode_OK;
                        }
                        else
                        {
                            _logSendFailure( _s( L"TransferAmount_V4" ) );

                            //gv in case we get a specific error code, set the display text accordingly
                            if( 0 != m_pBAXIBasicParameters->m_lLastError )
                            {
                                in_outMsg->displayText = ( _getErrorText( m_pBAXIBasicParameters->m_lLastError ) ).to_native_string( );
                                in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
                            }
                            return eErrorCode_NOT_OK;
                        }
                    }

                    case( TRXTYPE_CASH ):
                    {
                        //ds cash advance payment
                        _logSendCall( _s( L"TransferAmount_V4" ), _s( L"\"" ) + strOperatorNumber + _s( L"\", 0x39, " ) + TString::from_numerical< int >( lAmount ) + _s( L", 0x30, 0, 0x30, 0, \"\", \"\", \"\", \"\"" ) );
                        if( true == _processTransaction( &CBAXITransactionParameters( ( strOperatorNumber.to_native_string( ) ).c_str( ), eBAXI_TransactionCashAdvance, lAmount, eBAXI_NULL, 0, eBAXI_NULL, 0, "", "", "", "", false ) ) )
                        {
                            _logSendSuccess( _s( L"TransferAmount_V4" ) );
                            return eErrorCode_OK;
                        }
                        else
                        {
                            _logSendFailure( _s( L"TransferAmount_V4" ) );

                            //gv in case we get a specific error code, set the display text accordingly
                            if( 0 != m_pBAXIBasicParameters->m_lLastError )
                            {
                                in_outMsg->displayText = ( _getErrorText( m_pBAXIBasicParameters->m_lLastError ) ).to_native_string( );
                                in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
                            }
                            return eErrorCode_NOT_OK;
                        }
                    }

                    /*
                    //gv added Referral (= force Offline)
                    see Email from Torben Kristiansen aof 02.01.2012: 
                    "TransferAmount_V4(“0000”, 0x40, 1000, 0x30, 0, 0x30, 0, “”, “”, “”, “A1234”);
                    So in your GUI interface it must be possible for the Merchant:
                    Key in the authorisation code.
                    Or the Merchant just continue and do not key in the authorisation code.
                    It Merchant own risk to decide if he want to make a call to the acquire to get the authorisation code."
                    */
                    case( TRXTYPE_REFERRAL ):
                    {
                        //gv the trxRefNbrIn contains the Referral Authorisation Code
                        const TString strAuthNbr ( TString::from_native_string( in_outMsg->trxRefNbrIn ) );

                        //gv Referral (= force Offline)
                        _logSendCall( _s( L"TransferAmount_V4" ), _s( L"\"" ) + strOperatorNumber + _s( L"\", 0x40, " ) 
                            + TString::from_numerical< int >( lAmount ) + _s( L", 0x30, 0, 0x30, 0, \"\", \"\", \"\", \"" ) + strAuthNbr + _s( L"\"" ) );

                        if( true == _processTransaction( &CBAXITransactionParameters( ( strOperatorNumber.to_native_string( ) ).c_str( ), eBAXI_TransactionForceOffline, lAmount, eBAXI_NULL, 0, eBAXI_NULL, 0, "", "", "", ( strAuthNbr.to_native_string( ) ).c_str( ), false ) ) )
                        {
                            _logSendSuccess( _s( L"TransferAmount_V4" ) );
                            return eErrorCode_OK;
                        }
                        else
                        {
                            _logSendFailure( _s( L"TransferAmount_V4" ) );

                            //gv in case we get a specific error code, set the display text accordingly
                            if( 0 != m_pBAXIBasicParameters->m_lLastError )
                            {
                                in_outMsg->displayText = ( _getErrorText( m_pBAXIBasicParameters->m_lLastError ) ).to_native_string( );
                                in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
                            }
                            return eErrorCode_NOT_OK;
                        }
                    }

                    case( TRXTYPE_CREDIT ):
                    {
                        //ds regular credit
                        _logSendCall( _s( L"TransferAmount_V4" ), _s( L"\"" ) + strOperatorNumber + _s( L"\", 0x31, " ) + TString::from_numerical< int >( lAmount ) + _s( L", 0x30, 0, 0x30, 0, \"\", \"\", \"\", \"\"" ) );
                        if( true == _processTransaction( &CBAXITransactionParameters( ( strOperatorNumber.to_native_string( ) ).c_str( ), eBAXI_TransactionCredit, lAmount, eBAXI_NULL, 0, eBAXI_NULL, 0, "", "", "", "", false ) ) )
                        {
                            _logSendSuccess( _s( L"TransferAmount_V4" ) );
                            return eErrorCode_OK;
                        }
                        else
                        {
                            _logSendFailure( _s( L"TransferAmount_V4" ) );

                            //gv in case we get a specific error code, set the display text accordingly
                            if( 0 != m_pBAXIBasicParameters->m_lLastError )
                            {
                                in_outMsg->displayText = ( _getErrorText( m_pBAXIBasicParameters->m_lLastError ) ).to_native_string( );
                                in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
                            }
                            return eErrorCode_NOT_OK;
                        }

                        /*ds track 2 -> GIFTCARD
                        if( TRACK_ISO2 == in_outMsg->trackPresence )
                        {
                            //ds do payment on customers card
                            _logSendCall( _s( L"TransferAmount_V4" ), _s( L"\"" ) + strOperatorNumber + _s( L"\", 0x30, " ) + TString::from_numerical< int >( lAmount ) + _s( L", 0x30, 0, 0x30, 0, \"\", \"\", \"\", \"\"" ) );
                            if( true == _processTransaction( &CBAXITransactionParameters( ( strOperatorNumber.to_native_string( ) ).c_str( ), eBAXI_TransactionPayment, lAmount, eBAXI_NULL, 0, eBAXI_NULL, 0, "", "", "", "", false ) ) )
                            {
                                _logSendSuccess( _s( L"TransferAmount_V4" ) );
                            }
                            else
                            {
                                _logSendFailure( _s( L"TransferAmount_V4" ) );
                                return eErrorCode_NOT_OK;
                            }

                            //ds let the terminal breath
                            ::Sleep( 5000 );

                            //ds do credit on gift card
                            _logSendCall( _s( L"TransferAmount_V4" ), _s( L"\"" ) + strOperatorNumber + _s( L"\", 0x38, " ) + TString::from_numerical< int >( lAmount ) + _s( L", 0x30, 0, 0x30, 0, \"\", \"\", \"\", \"\"" ) );
                            if( true == _processTransaction( &CBAXITransactionParameters( ( strOperatorNumber.to_native_string( ) ).c_str( ), eBAXI_TransactionCreditDeposit, lAmount, eBAXI_NULL, 0, eBAXI_NULL, 0, "", "", "", "", true ) ) )
                            {
                                _logSendSuccess( _s( L"TransferAmount_V4" ) );
                            }
                            else
                            {
                                _logSendFailure( _s( L"TransferAmount_V4" ) );

                                    //gv in case we get a specific error code, set the display text accordingly
                                if( 0 != m_pBAXIBasicParameters->m_lLastError )
                                {
                                    in_outMsg->displayText = ( _getErrorText( m_pBAXIBasicParameters->m_lLastError ) ).to_native_string( );
                                    in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
                                }
                                return eErrorCode_NOT_OK;
                            }

                            //ds send track 2 to specify the card
                            _logSendCall( _s( L"TransferCardData" ), _s( L"0x32, \"" ) + strCardInformation + _s( L"\" )" ) );
                            if( true == _transferCardData( &CBAXITransferCardDataParameters( static_cast< long >( eBAXI_TransactionTrack2 ), ( strCardInformation.to_native_string( ) ).c_str( ) ) ) )
                            {
                                _logSendSuccess( _s( L"TransferCardData" ) );
                                return eErrorCode_OK;
                            }
                            else
                            {
                                _logSendFailure( _s( L"TransferCardData" ) );

                                //gv in case we get a specific error code, set the display text accordingly
                                if( 0 != m_pBAXIBasicParameters->m_lLastError )
                                {
                                    in_outMsg->displayText = ( _getErrorText( m_pBAXIBasicParameters->m_lLastError ) ).to_native_string( );
                                    in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
                                }
                                return eErrorCode_NOT_OK;
                            }
                        }*/
                    }

                    case( TRXTYPE_BALANCE ):
                    {
                        //ds balance
                        _logSendCall( _s( L"TransferAmount_V4" ), _s( L"\"" ) + strOperatorNumber + _s( L"\", 0x36, " ) + TString::from_numerical< int >( lAmount ) + _s( L", 0x30, 0, 0x30, 0, \"\", \"\", \"\", \"\"" ) );
                        if( true == _processTransaction( &CBAXITransactionParameters( ( strOperatorNumber.to_native_string( ) ).c_str( ), eBAXI_TransactionBalance, 0, eBAXI_NULL, 0, eBAXI_NULL, 0, "", "", "", "", false ) ) )
                        {
                            _logSendSuccess( _s( L"TransferAmount_V4" ) );
                            return eErrorCode_OK;
                        }
                        else
                        {
                            _logSendFailure( _s( L"TransferAmount_V4" ) );

                            //gv in case we get a specific error code, set the display text accordingly
                            if( 0 != m_pBAXIBasicParameters->m_lLastError )
                            {
                                in_outMsg->displayText = ( _getErrorText( m_pBAXIBasicParameters->m_lLastError ) ).to_native_string( );
                                in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
                            }
                            return eErrorCode_NOT_OK;
                        }
                    }

                    //ds fall through intended
                    case( TRXTYPE_TIP ):
                    case( TRXTYPE_VOID_TIP ):
                    case( TRXTYPE_PAYMENT_WITH_TIP ):
                    {
                        //ds not implemented yet
                        return eErrorCode_OK;
                    }

                    //ds for everything else is nothing to send (always okay)
                    default:
                    {
                        return eErrorCode_OK;
                    }
                }
            }

            case( EFTENDOFDAY ):
            {
                //ds do a reconciliation
                _logSendCall( _s( L"Administration" ), _s( L"0x3130, \"" ) + strOperatorNumber + _s( L"\"" ) );
                if( true == _processAdministration( &CBAXIAdministrationParameters( eBAXI_AdministrationReconciliation, ( strOperatorNumber.to_native_string( ) ).c_str( ) ) ) )
                {
                    _logSendSuccess( _s( L"Administration" ) );
                    return eErrorCode_OK;
                }
                else
                {
                    _logSendFailure( _s( L"Administration" ) );
                    return eErrorCode_NOT_OK;
                }
            }

            case( EFTUTILITY ):
            {
                LogInfo( _s( L"[send] utility: " ) + TString::from_native_string( in_outMsg->utilityOpCode ) );

                //ds Z
                if( "z" == in_outMsg->utilityOpCode )
                {
                    //ds close BAXI (CRASHES IF BAXI IS NOT OPEN)
                    _logSendCall( _s( L"Close" ), _s( L" " ) );
                    if( true == _closeBAXI( ) )
                    {
                        _logSendSuccess( _s( L"Close" ) );
                        return eErrorCode_OK;
                    }
                    else
                    {
                        _logSendFailure( _s( L"Close" ) );

                        //gv in case we get a specific error code, set the display text accordingly
                        if( 0 != m_pBAXIBasicParameters->m_lLastError )
                        {
                            in_outMsg->displayText = ( _getErrorText( m_pBAXIBasicParameters->m_lLastError ) ).to_native_string( );
                            in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
                        }
                        return eErrorCode_NOT_OK;
                    }
                }

                //ds XReport
                if( "+" == in_outMsg->utilityOpCode )
                {
                        _logSendCall( _s( L"Administration" ), _s( L"0x3136, \"" ) + strOperatorNumber + _s( L"\"" ) );

                        //ds build a temporary end of day report
                        if( true == _processAdministration( &CBAXIAdministrationParameters( eBAXI_AdministrationXReport, ( strOperatorNumber.to_native_string( ) ).c_str( ) ) ) )
                        {
                            _logSendSuccess( _s( L"Administration" ) );
                            return eErrorCode_OK;
                        }
                        else
                        {
                            _logSendFailure( _s( L"Administration" ) );

                            //gv in case we get a specific error code, set the display text accordingly
                            if( 0 != m_pBAXIBasicParameters->m_lLastError )
                            {
                                in_outMsg->displayText = ( _getErrorText( m_pBAXIBasicParameters->m_lLastError ) ).to_native_string( );
                                in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
                            }
                            return eErrorCode_NOT_OK;
                        }
                }
            }

            //ds for everything else is also nothing to send
            default:
            {
                //ds okay
                LogInfo( _s( L"[send] nothing was sent." ) );
                return eErrorCode_OK;
            }
        }
    }
    catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
    {
        //ds log as error, essential for logging
        LogError( _s( L"[send] could not convert numerical to string." ) );

        return eErrorCode_NOT_OK;
    }

    //ds if we are still here, send was not successful
    LogError( _s( L"[send] unknown method/utility." ) );
    LogInfo( _s( L"-----------------------------------------------------------------------------------------------------------" ) + TChar::newline( ) + TChar::space( ) );

    return eErrorCode_NOT_OK;
}

long CPCLIImplementationBAXI::receive( boost::shared_ptr< CPCLIMessage > in_outMsg )
{
    LogInfo( _s( L"- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -" ) );

    //ds needed to escape receive loop with non communication error
    in_outMsg->eftError = false;

    //ds we got many conversions from numerics to strings in here
    try
    {
        LogInfo( TString::sprintf( _s( L"[receive] calling method: %i" ) ) % static_cast< int >( in_outMsg->eftMethod ) );

        switch( in_outMsg->eftMethod )
        {
            case ( EFTCONFIGDRIVER ):
            {
                LogInfo( _s( L">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" ) );
                LogInfo( _s( L"[receive] Version of BAXI.DLL: " ) + TString::from_native_string( m_pBAXIBasicParameters->m_chVersion ) );
                LogInfo( _s( L"[receive] Version of terminal software: " ) + TString::from_native_string( m_pBAXIBasicParameters->m_chTerm_sw_version ) );
                LogInfo( _s( L">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" ) );
                
                //gv in case we get a specific error code, set the display text accordingly
                if( ( 0 != m_pBAXIBasicParameters->m_lLocalModeResult ) && ( 0 != m_pBAXIBasicParameters->m_lLastError ) )
                {
                    in_outMsg->displayText = ( _getErrorText( m_pBAXIBasicParameters->m_lLastError ) ).to_native_string( );
                    in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
                }

                break;
            }
            case( EFTTRX ):
            {
                //gv convert encoding and format the ticket data (this is a general routine for all Pepper methods)
                LogInfo( _s( L">>> enter the conversion routine for the receipt data <<<" ) );
                in_outMsg->receiptText = _formatReceiveReceipt( m_pBAXIBasicParameters->m_chPrintText, in_outMsg->receiptFormat );

                //ds log all received values
                LogInfo( TString::sprintf( _s( L"[receive] LocalModeResult: %i" ) ) % static_cast< int >( m_pBAXIBasicParameters->m_lLocalModeResult ) );
                const TString strIssuer( TString::from_numerical< int > ( m_pBAXIBasicParameters->m_lLocalModeIssuer ) );
                LogInfo( _s( L"[receive] LocalModeIssuer: " ) + strIssuer );
                LogInfo( _s( L"[receive] CardData: " ) + TString::from_native_string( m_pBAXIBasicParameters->m_chCardData ) );
                LogInfo( _s( L"[receive] TimeStamp: " ) + TString::from_native_string( m_pBAXIBasicParameters->m_chTimestamp ) );
                LogInfo( TString::sprintf( _s( L"[receive] VerificationMethod: %i" ) ) % static_cast< int >( m_pBAXIBasicParameters->m_lVerificationMethod ) );
                LogInfo( _s( L"[receive] SessionNumber: " ) + TString::from_native_string( m_pBAXIBasicParameters->m_chSessionNumber ) );
                LogInfo( _s( L"[receive] StanAuth: " ) + TString::from_native_string( m_pBAXIBasicParameters->m_chStanAuth ) );
                LogInfo( _s( L"[receive] LocalModeResultData: " ) + TString::from_native_string( m_pBAXIBasicParameters->m_chLocalModeResultData ) );
                LogInfo( _s( L"[receive] TerminalID: " ) + TString::from_native_string( m_pBAXIBasicParameters->m_chTerminalID ) );
                LogInfo( _s( L"[receive] ResultCode: " ) + TString::from_native_string( m_pBAXIBasicParameters->m_chResponseCode ) );

                //ds if response is valid
                if( 0 == m_pBAXIBasicParameters->m_lLocalModeResult )
                {
                    in_outMsg->displayText = ( _s( L"TRANSACTION APPROVED (" ) + TString::from_numerical< int >( m_pBAXIBasicParameters->m_lLocalModeResult ) + _s( L")" ) ).to_native_string( );
                    in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );

                    //gv set Terminal-ID
                    in_outMsg->terminalID = m_pBAXIBasicParameters->m_chTerminalID;

                    //ds fill in parameters
                    const TString strTimeStamp( TString::from_native_string( m_pBAXIBasicParameters->m_chTimestamp ) );

                    //ds make sure we got a valid timestamp length for substr
                    if( 14 == strTimeStamp.length( ) )
                    {
                        in_outMsg->trxTime        = ( strTimeStamp.substr( 8, 6 ) ).to_native_string( );
                        in_outMsg->trxDate        = ( strTimeStamp.substr( 6, 2 ) + strTimeStamp.substr( 4, 2 ) + strTimeStamp.substr( 0, 4 ) ).to_native_string( );
                    }

                    in_outMsg->cardType = static_cast< unsigned short >( getCardType( in_outMsg, strIssuer.to_native_string( ) ) );

                    in_outMsg->cardNbr        = m_pBAXIBasicParameters->m_chCardData;
                    in_outMsg->bookkeepPeriod = m_pBAXIBasicParameters->m_chSessionNumber;
                    in_outMsg->trxRefNbrOut   = m_pBAXIBasicParameters->m_chStanAuth;
                    in_outMsg->authNbr        = m_pBAXIBasicParameters->m_chStanAuth;

                    //gv so far only build cardholder receipt 
                    buildClientReceipt( in_outMsg, "" );

                    //gv set ReceiptSignature flag
                    switch ( m_pBAXIBasicParameters->m_lVerificationMethod ) 
                    {
                        //gv  0: Transaction is PIN based   (default) 
                    case ( 0 ):
                        {
                            in_outMsg->receiptSignature = ePPIIReceiptSignature_Pin;
                            break;
                        }
                        //gv 1: Transaction is signature based 
                    case ( 1 ):
                        //gv  2: No CVM. With or without amount confirmation by cardholder.
                    case ( 2 ):
                        {
                            in_outMsg->receiptSignature = ePPIIReceiptSignature_ClientSignature; 
                            //gv only build a merchant receipt if a signature is required 
                            buildMerchantReceipt( in_outMsg, "" );
                            break;
                        }
                    default:
                        {
                            //gv 3: Transaction is Loyalty Transaction 
                            in_outMsg->receiptSignature = ePPIIReceiptSignature_Nothing;
                            break;
                        }
                    }
                    //gv also set the ResponseCode from host or - if offline authorised - from terminal
                    if ( 0x00 != m_pBAXIBasicParameters->m_chResponseCode[0] )
                    {
                        if( true == GLOBAL_CONFIG->get< ePGSIConfigParameter_Operation_DeprecatedXmlOutput >() )
                        {
                            in_outMsg->xmlAdditionalParameters = CPCLIXmlUtilities::addTag2PepXmlParams(in_outMsg->xmlAdditionalParameters, "ResultCode", m_pBAXIBasicParameters->m_chResponseCode ); 
                        }
                        else
                        {
                            in_outMsg->xmlAdditionalParameters = CPCLIXmlUtilities::addTag2PepXmlParams(in_outMsg->xmlAdditionalParameters, "ResultCodeString", m_pBAXIBasicParameters->m_chResponseCode ); 
                        }
                    }
                    else
                    {
                        if( true == GLOBAL_CONFIG->get< ePGSIConfigParameter_Operation_DeprecatedXmlOutput >() )
                        {
                            in_outMsg->xmlAdditionalParameters = CPCLIXmlUtilities::addTag2PepXmlParams(in_outMsg->xmlAdditionalParameters, "ResultCode", CPCLIStringUtilities::toStr( m_pBAXIBasicParameters->m_lLocalModeResult ) ); 
                        }
                        else
                        {
                            in_outMsg->xmlAdditionalParameters = CPCLIXmlUtilities::addTag2PepXmlParams(in_outMsg->xmlAdditionalParameters, "ResultCodeString", CPCLIStringUtilities::toStr( m_pBAXIBasicParameters->m_lLocalModeResult ) ); 
                        }
                    }
                    LogInfo( _s( L"[receive] transaction parameters set." ) );
                    break;
                }
                else
                {
                    LogError( _s( L"[receive] transaction rejected." ) );
                    LogInfo( _s( L"[receive] LocalModeRejectionReason: " ) + TString::from_native_string( m_pBAXIBasicParameters->m_chLocalModeRejectionReason ) );
                    LogInfo( _s( L"[receive] LocalModeRejectionSource: " ) + TString::from_native_string( m_pBAXIBasicParameters->m_chLocalModeRejectionSource ) );
                    LogInfo( TString::sprintf( _s( L"[receive] LastError: %i" ) ) % static_cast< int >( m_pBAXIBasicParameters->m_lLastError ) );

                    if( 0 != m_pBAXIBasicParameters->m_lLastError )
                    {
                        in_outMsg->displayText = ( _getErrorText( m_pBAXIBasicParameters->m_lLastError ) ).to_native_string( );
                    }
                    else
                    {
                        in_outMsg->displayText = ( _s( L"TRANSACTION REJECTED (" ) + TString::from_numerical< int >( m_pBAXIBasicParameters->m_lLocalModeResult ) + _s( L")" ) ).to_native_string( );
                    }
                    in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
                    in_outMsg->eftError = true;

                    //gv output the terminal's result code
                    if( true == GLOBAL_CONFIG->get< ePGSIConfigParameter_Operation_DeprecatedXmlOutput >() )
                    {
                        in_outMsg->xmlAdditionalParameters = CPCLIXmlUtilities::addTag2PepXmlParams(in_outMsg->xmlAdditionalParameters, "ResultCode", CPCLIStringUtilities::toStr( m_pBAXIBasicParameters->m_lLocalModeResult ) ); 
                    }
                    else
                    {
                       in_outMsg->xmlAdditionalParameters = CPCLIXmlUtilities::addTag2PepXmlParams(in_outMsg->xmlAdditionalParameters, "ResultCodeString", CPCLIStringUtilities::toStr( m_pBAXIBasicParameters->m_lLocalModeResult ) ); 
                    }

                    //gv usually Baxi generates an error ticket
                    buildMerchantReceipt( in_outMsg, "" );

                    break;
                }
            }

            case( EFTENDOFDAY ):
            {
            
                //gv convert encoding and format the ticket data (this is a general routine for all Pepper methods)
                LogInfo( _s( L">>> enter the conversion routine for the receipt data <<<" ) );
                in_outMsg->receiptText = _formatReceiveReceipt( m_pBAXIBasicParameters->m_chPrintText, in_outMsg->receiptFormat );

                //ds if response is valid
                if( 1 == m_pBAXIBasicParameters->m_lLocalModeResult )
                {
                    //ds set end of day text
                    CPCLIFileUtilities::writeFileEntry( in_outMsg->receiptText, in_outMsg->eodPrintFile );
                    LogInfo( _s( L"[receive] end of day text set." ) );

                    in_outMsg->displayText = ( _s( L"END OF DAY OK (" ) + TString::from_numerical< int >( m_pBAXIBasicParameters->m_lLocalModeResult ) + _s( L")" ) ).to_native_string( );
                    in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
                    break;
                }
                else
                {
                    LogError( _s( L"[receive] end of day request rejected." ) );
                    LogInfo( _s( L"[receive] LocalModeRejectionReason: " ) + TString::from_native_string( m_pBAXIBasicParameters->m_chLocalModeRejectionReason ) );
                    LogInfo( _s( L"[receive] LocalModeRejectionSource: " ) + TString::from_native_string( m_pBAXIBasicParameters->m_chLocalModeRejectionSource ) );

                    if( 0 != m_pBAXIBasicParameters->m_lLastError )
                    {
                        in_outMsg->displayText = ( _getErrorText( m_pBAXIBasicParameters->m_lLastError ) ).to_native_string( );
                    }
                    else
                    {
                        in_outMsg->displayText = ( _s( L"END OF DAY NOT OK (" ) + TString::from_numerical< int >( m_pBAXIBasicParameters->m_lLocalModeResult ) + _s( L")" ) ).to_native_string( );
                    }
                    in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
                    in_outMsg->eftError = true;
                    break;
                }
            }

            case( EFTUTILITY ):
            {
                //ds XReport
                if( "+" == in_outMsg->utilityOpCode )
                {
                    //gv convert encoding and format the ticket data (this is a general routine for all Pepper methods)
                    LogInfo( _s( L">>> enter the conversion routine for the receipt data <<<" ) );
                    in_outMsg->receiptText = _formatReceiveReceipt( m_pBAXIBasicParameters->m_chPrintText, in_outMsg->receiptFormat );

                    //ds if response is valid
                    if( 1 == m_pBAXIBasicParameters->m_lLocalModeResult )
                    {
                        //ds set XReport text
                        CPCLIFileUtilities::writeFileEntry( in_outMsg->receiptText, m_strReportFileName.to_native_string( ) );
                        LogInfo( _s( L"[receive] X-Report text set." ) );

                        in_outMsg->displayText = ( _s( L"X-Report OK (" ) + TString::from_numerical< int >( m_pBAXIBasicParameters->m_lLocalModeResult ) + _s( L")" ) ).to_native_string( );
                        in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
                    }
                    else
                    {
                        LogError( _s( L"[receive] X-Report request rejected." ) );
                        LogInfo( _s( L"[receive] LocalModeRejectionReason: " ) + TString::from_native_string( m_pBAXIBasicParameters->m_chLocalModeRejectionReason ) );
                        LogInfo( _s( L"[receive] LocalModeRejectionSource: " ) + TString::from_native_string( m_pBAXIBasicParameters->m_chLocalModeRejectionSource ) );

                        if( 0 != m_pBAXIBasicParameters->m_lLastError )
                        {
                            in_outMsg->displayText = ( _getErrorText( m_pBAXIBasicParameters->m_lLastError ) ).to_native_string( );
                        }
                        else
                        {
                            in_outMsg->displayText = ( _s( L"X-Report NOT OK (" ) + TString::from_numerical< int >( m_pBAXIBasicParameters->m_lLocalModeResult ) + _s( L")" ) ).to_native_string( );
                        }
                        in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
                        in_outMsg->eftError = true;
                    }
                }
                else
                {
                    LogInfo( _s( L"[receive] there is nothing to be parsed." ) );
                }
                break;
            }

            default:
            {
                LogInfo( _s( L"[receive] there is nothing to be parsed." ) );

                //ds default is always ok
                break;
            }
        }
    }
    catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
    {
        LogError( _s( L"[receive] could not convert numerical to string." ) );

        //ds essential for logging
        in_outMsg->eftError = true;
    }

    LogInfo( _s( L"-----------------------------------------------------------------------------------------------------------" ) + TChar::newline( ) + TChar::space( ) );
    return eErrorCode_OK;
}

long CPCLIImplementationBAXI::prepareTrxMsg( boost::shared_ptr< CPCLIMessage > in_outMsg )
{
    //ds skip this for recovery
    if ( true == in_outMsg->isRecoveryTrx )
    {
        //ds set number of streams to send to zero
        in_outMsg->nofStreamToSend = 0;

        //ds set display text
        in_outMsg->setDisplayCode( eErrorCode_RECOVERY_OK );

        //ds recovery always ok here
        return eErrorCode_RECOVERY_OK;
    }

    //ds rude try block because we got some numerical to string conversions
    try
    {
        LogInfo( TString::sprintf( _s( L"[prepareTrxMsg] in_outMsg->trackPresence: %u" ) ) % ( in_outMsg->trackPresence ) );
        LogInfo( _s( L"[prepareTrxMsg] in_outMsg->cardInformation: " ) + TString::from_native_string( in_outMsg->cardInformation ) );
    }
    catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
    {
        LogError( _s( L"[prepareTrxMsg] could not convert numerical to string." ) );

        //ds invalid numerical format
        in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );
        return eErrorCode_PARAMETER_VALIDATION;
    }

    //ds trigger 1 send event
    in_outMsg->nofStreamToSend = 1;

    in_outMsg->setDisplayCode( eErrorCode_OK );

    //ds everything okay if still here
    return eErrorCode_OK;
}

long CPCLIImplementationBAXI::prepareEndOfDayMsg( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds delete old file
    CPCLIFileUtilities::deleteFile( in_outMsg->eodPrintFile );

    //ds trigger send event
    in_outMsg->nofStreamToSend = 1;

    in_outMsg->setDisplayCode( eErrorCode_OK );

    return eErrorCode_OK;
}

long CPCLIImplementationBAXI::getVersion( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds get pepper version
    in_outMsg->version = IPCLIImplementation::getPEPVersion( );

    //ds add implementation version
    in_outMsg->version += version;

    //ds always ok
    in_outMsg->setDisplayCode( eErrorCode_OK );
    return eErrorCode_OK;
}

int CPCLIImplementationBAXI::getUserInputTimeout( )
{
    return USER_TIMEOUT_TEMPLATE;
}

std::string CPCLIImplementationBAXI::getTimeoutTicketMsg( )
{
    return TIMEOUT_TICKET_MSG_TEMPLATE;
}


std::string CPCLIImplementationBAXI::getNegCCEODTicketMsg( )
{
    return NEG_CC_EOD_MSG_TEMPLATE;
}

std::string CPCLIImplementationBAXI::getNegDCEODTicketMsg( )
{
    return NEG_DC_EOD_MSG_TEMPLATE;
}

long CPCLIImplementationBAXI::startIdleProcess( boost::shared_ptr< CPCLIMessage >  in_outMsg, bool newFlag )
{
    //ds always ok
    return eErrorCode_OK;
}

long CPCLIImplementationBAXI::stopIdleProcess( bool stopFlag )
{
    //ds always ok
    return eErrorCode_OK;
}

short CPCLIImplementationBAXI::getCardType ( boost::shared_ptr< CPCLIMessage > in_outMsg,
                                                       std::string key1,
                                                       std::string key2,
                                                       std::string key3,
                                                       std::string key4,
                                                       std::string key5 )
{
    //ds default to unknown
    short siCardType = CRD_UNKNOWN;

    //ds set cardtype and cardname in in_outMsg
    siCardType = IPCLIImplementation::getCardType( in_outMsg, key1 );

    //ds log result informative
    try
    {
        LogInfo( TString::sprintf( _s( L"[getCardType] cardType: %i" ) ) % siCardType );
    }
    catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
    {
        //ds conversion could not be done
        LogError( _s( L"[getCardType] could not convert cardType to string." ) );
    }

    return siCardType;
}

long CPCLIImplementationBAXI::translateEftConfigDriver( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds set return value to state
    long lReturnValue = IPCLIImplementation::translateEftConfigDriver( in_outMsg );

	//dh check for result
	if ( eErrorCode_OK != lReturnValue )
	{
		//dh leave
		return lReturnValue;
	}

    if( false == m_bIsWrapperLoaded )
    {
        LogError( _s( L"[translateEftConfigDriver] baxiwrapper.dll not loaded." ) );

        //ds import failure
        in_outMsg->displayText = "COULD NOT LOAD BAXIWRAPPER.DLL";
        in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
        return eErrorCode_NOT_OK;
    }

    //ds setup parameters
    m_cBAXIConfigurationParameters.m_siTraceLevel             = 4;
    m_cBAXIConfigurationParameters.m_siCommPort               = static_cast< short >( in_outMsg->comPort );
    m_cBAXIConfigurationParameters.m_siPrinterWidth           = static_cast< short >( in_outMsg->receiptFormat );
    m_cBAXIConfigurationParameters.m_siDisplayWidth           = 40; //in_outMsg->displayWidth == 0?;
    m_cBAXIConfigurationParameters.m_siMsgRouterOn            = 0;
    m_cBAXIConfigurationParameters.m_siIndicateEotTransaction = 1;
    m_cBAXIConfigurationParameters.m_siCutterSupport          = 1;
    m_cBAXIConfigurationParameters.m_siPowerCycleCheck        = 1;
    m_cBAXIConfigurationParameters.m_siTidSupervision         = 0;
    m_cBAXIConfigurationParameters.m_siEventVersion           = 2;

    //gv OTRS #0100695 according to nets this parameter should be set to 0
    m_cBAXIConfigurationParameters.m_siAutoGetCustomerInfo    = 0;
    

    //ds set to active (dll MUST be always available)
    in_outMsg->eftAvailable = true;

    //ds set version and receipt format
    in_outMsg->version              = IPCLIImplementation::getPEPVersion( ) + version; 
    in_outMsg->desiredReceiptFormat = in_outMsg->receiptFormat; 

	//dh ---------------------------------------------------------------------------------------------------

	if ( true == getpConfigDriverInputOptions()->isSet( ePPIIOption_PreSettlementReportFileName ) )
	{
		//dh get value
		TString strFilename( getpConfigDriverInputOptions()->get< ePPIIOption_PreSettlementReportFileName >().getString() );

		//dh retrieve complete path
		m_strReportFileName = TString::from_native_string( CPCLIFileUtilities::getPathFileName(ePCLIPathDescriptor_Print, strFilename.to_native_string() ) );
    }
    else              
    {
        m_strReportFileName = TString::from_native_string( CPCLIFileUtilities::getPathFileName(ePCLIPathDescriptor_Print, "Report.txt" ) );
    }

	//dh log this
	LogInfo( _s( L"[getpConfigDriverInputOptions] set report file path and name to: " ) + m_strReportFileName );


	//gv ---------------------------------------------------------------------------------------------------

    //gv get extended Vendor Name (mandatory for certification at NETS)
    
     /* gv RULES:Vendor Name, this will be a 3 letter description 
           supplied from BBS to each vendor. Fixed length, 3 
           letters (A-Z).  To receive a description contact 
           baxi@bbs.no
       */
    if ( true == getpConfigDriverInputOptions()->isSet( ePPIIOption_CashRegisterVendorString ) )
	{
        std::string strVendorName( getpConfigDriverInputOptions()->get< ePPIIOption_CashRegisterVendorString >( ).to_native_string( ) );
 
        if ( 3 < strVendorName.size( ) )
        {
            LogWarning( _s( L"[getpConfigDriverInputOptions] CashRegisterVendorString is too long and will be shortened to 3 characters" ) ); 
            
            //gv truncate to 3 characters
            strVendorName = strVendorName.substr( 0, 3 );
        }
        else if ( 3 > strVendorName.size( ) )
        {
            LogError( _s( L"[getpConfigDriverInputOptions] CashRegisterVendorString is too short" ) ); 
            in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION ); 
            return eErrorCode_PARAMETER_VALIDATION;
        }

        if ( std::string::npos != strVendorName.find_first_not_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZ" ) )
        {
            LogInfo( _s( L"[getpConfigDriverInputOptions] CashRegisterVendorString must contain upper case letters only " ) ); 
            in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION ); 
            return eErrorCode_PARAMETER_VALIDATION;
        }

        LogInfo( _s( L"[getpConfigDriverInputOptions] set CashRegisterVendorString to: " ) + TString::from_native_string( strVendorName ) ); 

        //gv start to build up the VendorInfoExtended
        m_strVendorInfoExtended = strVendorName; 
        m_strVendorInfoExtended += ";"; 
    }
    else
    {
        LogInfo( _s( L"[getpConfigDriverInputOptions] CashRegisterVendorString is missing" ) );
        in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION ); 
        return eErrorCode_PARAMETER_VALIDATION;
    }

    //gv get Application Name (mandatory for certification at NETS)
    if ( true == getpConfigDriverInputOptions()->isSet( ePPIIOption_CashRegisterApplicationName ) )
	{
        std::string strApplicationName( getpConfigDriverInputOptions()->get< ePPIIOption_CashRegisterApplicationName >( ).to_native_string( ) );
        
        //gv append to VendorInfoExtended
        LogInfo( _s( L"[getpConfigDriverInputOptions] set ApplicationName to: " ) + TString::from_native_string( strApplicationName ) ); 
        m_strVendorInfoExtended += strApplicationName; 
        m_strVendorInfoExtended += ";"; 
    }
    else
    {
        LogInfo( _s( L"[getpConfigDriverInputOptions] CashRegisterApplicationName is missing" ) );
        in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION ); 
        return eErrorCode_PARAMETER_VALIDATION;
    }

    //gv get Application Version (mandatory for certification at NETS)
     if ( true == getpConfigDriverInputOptions()->isSet( ePPIIOption_CashRegisterApplicationVersionString ) )
     {
        std::string strVendorVersion( getpConfigDriverInputOptions()->get< ePPIIOption_CashRegisterApplicationVersionString >( ).to_native_string( ) );
        
        LogInfo( _s( L"[getpConfigDriverInputOptions] set VendorVersion to: " ) + TString::from_native_string( strVendorVersion ) ); 
        
        //gv append to VendorInfoExtended
        m_strVendorInfoExtended += strVendorVersion; 
        m_strVendorInfoExtended += ";"; 
    }
    else
    {
        LogInfo( _s( L"[getpConfigDriverInputOptions] CashRegisterApplicationVersionString is missing" ) );
        in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION ); 
        return eErrorCode_PARAMETER_VALIDATION;
    }

    //gv get Till ID (mandatory for certification at NETS)
    if ( true == getpConfigDriverInputOptions()->isSet( ePPIIOption_CashRegisterIdentificationString ) )
     {
        std::string strTillId( getpConfigDriverInputOptions()->get< ePPIIOption_CashRegisterIdentificationString >( ).to_native_string( ) );
      

        LogInfo( _s( L"[getpConfigDriverInputOptions] set TillId to: " ) + TString::from_native_string( strTillId ) ); 
        //gv append to VendorInfoExtended
        m_strVendorInfoExtended += strTillId; 
        m_strVendorInfoExtended += ";"; 
    }
    else
    {
        LogInfo( _s( L"[getpConfigDriverInputOptions] CashRegisterIdentificationString is missing" ) );
        in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION ); 
        return eErrorCode_PARAMETER_VALIDATION;
    }

    //gv check if total length of VendorInfoExtended does not exceed 32 characters
    if( 32 < m_strVendorInfoExtended.size( ) )
    {
        LogInfo( _s( L"[getpConfigDriverInputOptions] the combination of <CashRegisterApplicationName>, <CashRegisterApplicationVersionString> and <CashRegisterIdentificationString> exceeds the maximum length of 25 characters!" ) );
        in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION ); 
        return eErrorCode_PARAMETER_VALIDATION;
    }

    //gv requested by Torben Kristiansen, the Vendor Info must be put in order to be read by Nets
    //   format: "3-letters-Vendor-Name;Application-Name;Application-Version;ECR-Number;"
    sprintf( m_cBAXIConfigurationParameters.m_chVendorInfoExtended, "%s", m_strVendorInfoExtended.c_str( ) );

	//dh ---------------------------------------------------------------------------------------------------

	if ( true == getpConfigDriverInputOptions()->isSet( ePPIIOption_TerminalCodePageValue ) )
	{
		//dh get value
		m_eExpectedEncoding = getpConfigDriverInputOptions()->get< ePPIIOption_TerminalCodePageValue >();
	}

	//dh a string for the encoding
	TString strEncoding( L"INVALID" );

	//dh get string value
	switch ( m_eExpectedEncoding )
	{
	case ePPIITerminalCodePage_Latin1: strEncoding = _s( L"Latin1" ); break;
	case ePPIITerminalCodePage_Latin2: strEncoding = _s( L"Latin2" ); break;
	case ePPIITerminalCodePage_Cp1252: strEncoding = _s( L"Cp1252" ); break;
	case ePPIITerminalCodePage_Cp850:  strEncoding = _s( L"Cp850"  ); break;
	case ePPIITerminalCodePage_Utf8:   strEncoding = _s( L"Utf8"   ); break;
	default: /* cannot happen */ PASSERT( 0 );
	}

	//dh log this
	LogInfo( _s( L"[getpConfigDriverInputOptions] set Input Encoding to " ) + strEncoding );

	//dh ---------------------------------------------------------------------------------------------------

	//dh check host name
	if ( true == getpConfigDriverInputOptions()->isSet( ePPIIOption_HostName ) )
	{
		//dh get host name
		std::string strIpAddress( getpConfigDriverInputOptions()->get< ePPIIOption_HostName >().to_native_string() );

        //ds set the ip
        strncpy( m_cBAXIConfigurationParameters.m_chHostIpAddress, strIpAddress.c_str( ), BAXIFunctionParameters_MAX_SIZE_CHARARRAY - 1 );

		//dh log this
		LogInfo( _s( L"[getpConfigDriverInputOptions] set HostName to: " ) + TString::from_native_string( strIpAddress ) );
	}
    else
    {
        //ds not set
        LogError( _s( L"[getpConfigDriverInputOptions] Parameter HostName is not set." ) );

        //dh leave here
		return eErrorCode_MISSING_PARAMETER;
    }

	//dh ---------------------------------------------------------------------------------------------------


	//dh check host's port number
	if ( true == getpConfigDriverInputOptions()->isSet( ePPIIOption_HostPortNumber ) )
	{
		//dh get port
		m_cBAXIConfigurationParameters.m_lHostPort = static_cast< long >( getpConfigDriverInputOptions()->get< ePPIIOption_HostPortNumber >() );

		//dh log this
		LogInfo( TString::sprintf( _s( L"[getpConfigDriverInputOptions] set HostPortNumber to: %i" ) ) % static_cast< int >( m_cBAXIConfigurationParameters.m_lHostPort ) );
	}
    else
    {
        //ds not set
        LogError( _s( L"[getpConfigDriverInputOptions] Parameter HostPortNumber is not set." ) );

        //dh leave here
		return eErrorCode_MISSING_PARAMETER;
    }

	//gv ---------------------------------------------------------------------------------------------------

    std::string strName = "baxi-";
    std::string strPath = ( GLOBAL_CONFIG->get< ePGSIConfigParameter_Logging_Directory >() ).getString( ).to_native_string( );

    //ds set the BAXI log file
    strncpy( m_cBAXIConfigurationParameters.m_chLogFilePrefix, strName.c_str( ), BAXIFunctionParameters_MAX_SIZE_CHARARRAY - 1 );

    //dh log this
    LogInfo( _s( L"[getpConfigDriverInputOptions] set BAXI LogFilePrefix to: " ) + TString::from_native_string( strName ) );

    //ds set the path
    strncpy( m_cBAXIConfigurationParameters.m_chLogFilePath, strPath.c_str( ), BAXIFunctionParameters_MAX_SIZE_CHARARRAY - 1 ); 

    //dh log this
    LogInfo( _s( L"[getpConfigDriverInputOptions] set BAXI LogFilePath to: " ) + TString::from_native_string( strPath ) );

	//gv ---------------------------------------------------------------------------------------------------

	//dh set baudrate
	m_cBAXIConfigurationParameters.m_lBaudRate = static_cast< long >( getpConfigDriverInputOptions()->get< ePPIIOption_BaudRateValue >() );

	//dh log this
	LogInfo( TString::sprintf( _s( L"[getpConfigDriverInputOptions] set baud rate to: %u" ) ) % static_cast< unsigned int >( m_cBAXIConfigurationParameters.m_lBaudRate ) );

	//dh ---------------------------------------------------------------------------------------------------

    //read Hold Time from XML-Additional-Parameter <TimeoutMS>
	if ( true == getpConfigDriverInputOptions()->isSet( ePPIIOption_TimeoutInMilliseconds ) )
    {
		//dh get it
		m_cBAXIConfigurationParameters.m_uTimeOutMilliSeconds = getpConfigDriverInputOptions()->get< ePPIIOption_TimeoutInMilliseconds >();
	}
	else
	{
		m_cBAXIConfigurationParameters.m_uTimeOutMilliSeconds = BAXI_Default_Timeout;                
	}

    //ds log that
	LogInfo( TString::sprintf( _s( L"[getpConfigDriverInputOptions] set time-out to: %u" ) ) % ( m_cBAXIConfigurationParameters.m_uTimeOutMilliSeconds ) );

	//dh ---------------------------------------------------------------------------------------------------

    //ds okay
    in_outMsg->setDisplayCode( lReturnValue );
    return lReturnValue;
}

long CPCLIImplementationBAXI::translateEftOpen( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds base class
    return IPCLIImplementation::translateEftOpen( in_outMsg ); 
}

long CPCLIImplementationBAXI::translateEftTrx( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds call base class
    long ret = IPCLIImplementation::translateEftTrx( in_outMsg );

	//dh check return code
	if ( eErrorCode_OK != ret )
	{
		//dh on error leave
		return ret;
	}

	//dh -----------------------------------------------------------------------------

	//dh get loyalty info
    m_strBufferLoyaltyInfo = getpTransactionInputOptions()->get< ePPIIOption_LoyaltyInformationText >();

	//dh -----------------------------------------------------------------------------

    m_lEnableVAT = eBAXI_NULL;
    m_lAmountVAT = 0;

	//dh check if vat is set
	if ( true == getpTransactionInputOptions()->isSet( ePPIIOption_VatNumberString ) )
	{
		//dh get it
		TString strVat( getpTransactionInputOptions()->get< ePPIIOption_VatNumberString >() );

		try
		{
			//dh convert this to numeric
			m_lEnableVAT = eBAXI_TransactionVAT;	
			m_lAmountVAT = strVat.to_numerical< int >();
		}
		catch ( const baselayer::XCOREDTStringConversionFailedException& )
		{
			//dh forget it
			m_lEnableVAT = eBAXI_NULL;
			m_lAmountVAT = 0;

			//dh log this
			LogError( _s( L"The given VAT number is not numerical. The value is ignored" ) );
		}
	}

	//dh -----------------------------------------------------------------------------

    //ds default payment parameters
    m_lEnableCashback = eBAXI_TransactionPayment;
    m_lAmountCashback = 0;

	//dh check if vat is set
	if ( true == getpTransactionInputOptions()->isSet( ePPIIOption_CashBackAmount ) )
	{
        m_lEnableCashback = eBAXI_TransactionPaymentCashback;
        m_lAmountCashback = static_cast< long >( getpTransactionInputOptions()->get< ePPIIOption_CashBackAmount >() );
	}

	//dh -----------------------------------------------------------------------------

    //gv also allow Referrals (=force Offline)if trxRefNbrIn is empty
    if( ( TRXTYPE_REFERRAL == in_outMsg->trxType ) && ( EFTTRX == in_outMsg->eftMethod )  && ( "" == in_outMsg->trxRefNbrIn ) )
    {
        //gv if trxRefNbrIn is empty, fill in dummy string that will be removed later on
        in_outMsg->trxRefNbrIn = BAXI_DummyRefNbr;
    }

	//dh leave with result
	return ret;
}

long CPCLIImplementationBAXI::translateEftUtility( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
	//dh call base class
	long ret = IPCLIImplementation::translateEftUtility( in_outMsg );

	//dh leave on error
	if ( eErrorCode_OK != ret )
	{
		//dh leave on error
		return ret;
	}

	return ret;
}

long CPCLIImplementationBAXI::translateEftEndOfDay( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds base class
    return IPCLIImplementation::translateEftEndOfDay( in_outMsg );
}

long CPCLIImplementationBAXI::translateEftInit( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds base class
    return IPCLIImplementation::translateEftInit( in_outMsg );
}

long CPCLIImplementationBAXI::openEFTDevice( boost::shared_ptr< CPCLIMessage > in_outMsg )
{
    //ds always ok
    in_outMsg->setDisplayCode( eErrorCode_OK );

    return eErrorCode_OK;
}

long CPCLIImplementationBAXI::closeEFTDevice( boost::shared_ptr< CPCLIMessage > in_outMsg )
{
    //ds always ok
    in_outMsg->setDisplayCode( eErrorCode_OK );

    return eErrorCode_OK;
}

void CPCLIImplementationBAXI::buildClientReceipt( boost::shared_ptr< CPCLIMessage > in_outMsg, const char *receiveData )
{
    //gv set the ticket data 
    ticket->setTicket( in_outMsg->receiptText );
    
    //gv define 2 position markers
    size_t found_l = std::string::npos;
    size_t found_r = std::string::npos;
    
    //gv++ PROBABLY WORKS ONLY FOR DANISH TICKETS: 
    //gv search the receipt text for a ticket separator marker ("----Afriv her----")
    found_l = in_outMsg->receiptText.find( "-Afriv her-" );

    //gv if a separator is found, split the ticket data in two
    if ( std::string::npos != found_l ) 
    {
        found_r = found_l;

        //gv set to left marker to the ENDL preceding the current position
        found_l = ( in_outMsg->receiptText.substr( 0, found_l ) ).find_last_of( ENDL );

        //gv set ticket to be the first part receipt text
        ticket->setTicket( in_outMsg->receiptText.substr( 0, found_l ) );

        //gv set to right marker to the ENDL following the current position
        found_r = in_outMsg->receiptText.find( ENDL, found_r );

        //gv set the receipt text to the 2nd part of the data in order to be able to have a proper merchant ticket
        in_outMsg->receiptText = in_outMsg->receiptText.substr( found_r + 1 );
    }
    
    //gv write to cardholder print file
    ticket->flushToFile( in_outMsg->trxPrintFile );

    //gv push HEX dump of ticket to Log
    LogInfo( _s( L"- - - - - -CLIENT TICKET - HEX DUMP - - - - - - - - -" ) );
    LogInfo(  TByteStream( ticket->getTicket( ) ).toHexDumpString( eCOREDTHexdumpFormat_Separated ) );
    LogInfo( _s( L"- - - - - - - - - - - - - - - - - - - - - - - - - - -" ) );

    //gv push ticket to Log
    LogInfo( _s( L"- - - - - - - - - CLIENT TICKET - - - - - - - - - - -" ) );
    LogInfo( TString::from_native_string( ticket->getTicket() ) );
    LogInfo( _s( L"- - - - - - - - - - - - - - - - - - - - - - - - - - -" ) );

}

void CPCLIImplementationBAXI::buildMerchantReceipt( boost::shared_ptr< CPCLIMessage > in_outMsg, const char *receiveData )
{
    
   //gv set the ticket data 
    ticket->setTicket( in_outMsg->receiptText );

    //gv write to merchant print file
    ticket->flushToFile( in_outMsg->ccTrxPrintFile );

    //gv push ticket to Log
    if( 0 == m_pBAXIBasicParameters->m_lLocalModeResult )
    {
        //gv push HEX dump of ticket to Log
        LogInfo( _s( L"- - - - - MERCHANT TICKET - HEX DUMP - - - - - - - -" ) );
        LogInfo( TByteStream( ticket->getTicket( ) ).toHexDumpString( eCOREDTHexdumpFormat_Separated ) );
        LogInfo( _s( L"- - - - - - - - - - - - - - - - - - - - - - - - - - -" ) );
        //gv push merchant ticket to log
        LogInfo( _s( L"- - - - - - - - - MERCHANT TICKET - - - - - - - - - -" ) );
    }
    else
    {
        //gv push error ticket to log
        LogInfo( _s( L"- - - - - - - - - - ERROR TICKET - - - - - - - - - - -" ) );
    }
    LogInfo( TString::from_native_string( ticket->getTicket() ) );
    LogInfo( _s( L"- - - - - - - - - - - - - - - - - - - - - - - - - - -" ) );

    //gv reset the receipt text
    in_outMsg->receiptText = "";
}

long CPCLIImplementationBAXI::prepareStatusMsg( boost::shared_ptr< CPCLIMessage > in_outMsg )
{
    //ds not supported
    in_outMsg->setDisplayCode( eErrorCode_METHOD_NOT_SUPPORTED );
    return eErrorCode_METHOD_NOT_SUPPORTED;
}

long CPCLIImplementationBAXI::executeUtility( boost::shared_ptr< CPCLIMessage > in_outMsg )
{
    //ds call base class
    long lReturnValue = IPCLIImplementation::executeUtility( in_outMsg );

    if( eErrorCode_OK != lReturnValue )
    {
        //ds return bad base class call result
        in_outMsg->setDisplayCode( lReturnValue );
        return lReturnValue;
    }

    //ds send utility
    if( eErrorCode_OK == send( EFTUTILITY, in_outMsg ) )
    {
        //ds send successful
        lReturnValue = receive( in_outMsg );

        in_outMsg->setDisplayCode( lReturnValue );
        return lReturnValue;
    }
    else
    {
        //ds communication error between BAXI & terminal
        in_outMsg->setDisplayCode( eErrorCode_NOT_OK );
        return eErrorCode_NOT_OK;
    }

}

long CPCLIImplementationBAXI::executeInit( boost::shared_ptr< CPCLIMessage > in_outMsg )
{
    //ds not supported
    in_outMsg->setDisplayCode( eErrorCode_METHOD_NOT_SUPPORTED );
    return eErrorCode_METHOD_NOT_SUPPORTED;
}

bool CPCLIImplementationBAXI::isEFTAvailable(boost::shared_ptr< CPCLIMessage > in_outMsg, unsigned char * lowlayer_state, bool withoutTimer, bool waitBefore)
{
    //ds always available in this case
    return true;
}

void CPCLIImplementationBAXI::debugHighLevel()
{
}

void CPCLIImplementationBAXI::initStateMachine()
{
}

void CPCLIImplementationBAXI::addHeader()
{
}

void CPCLIImplementationBAXI::addTail()
{
}

void CPCLIImplementationBAXI::debugLowLevel()
{
}

void CPCLIImplementationBAXI::_logSendCall( const TString& p_strFunctionName, const TString& p_strFunctionParameters ) const
{
    LogInfo( _s( L"- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -" ) );
    LogInfo( _s( L"[send][" ) + p_strFunctionName + _s( L"] RAW call to BAXI: " ) + p_strFunctionName + _s( L"(" ) + p_strFunctionParameters + _s( L")" ) );
}

void CPCLIImplementationBAXI::_logSendSuccess( const TString& p_strFunctionName ) const
{
    //ds only log event text if available
    if( true == m_pBAXIBasicParameters->m_bIsEventTextSet )
    {
        LogInfo( _s( L"[send][" ) + p_strFunctionName + _s( L"] call succeed" ) );
        for( unsigned int u = 0; u < m_pBAXIBasicParameters->getEventTextBufferIndex( ); ++u )
        {
            LogInfo( _s( L"[send][" ) + p_strFunctionName + _s( L"] Event: " ) + TString::from_native_string( m_pBAXIBasicParameters->getEventTextBuffer( u ) ) );
        }
    }
    else
    {
        LogInfo( _s( L"[send][" ) + p_strFunctionName + _s( L"] call succeed - no Events received." ) );
    }

    //ds only log display text if available
    if( true == m_pBAXIBasicParameters->m_bIsDisplayTextSet )
    {
        for( unsigned int u = 0; u < m_pBAXIBasicParameters->getDisplayTextBufferIndex( ); ++u )
        {
            LogInfo( _s( L"[send][" ) + p_strFunctionName + _s( L"] DisplayText: " ) + TString::from_native_string( m_pBAXIBasicParameters->getDisplayTextBuffer( u ) ) );
        }
    }

    //ds only log received send data if available
    if( true == m_pBAXIBasicParameters->m_bIsCallSendData )
    {
        LogInfo( _s( L"[send][" ) + p_strFunctionName + _s( L"] received SendData: " ) + TString::from_native_string( m_pBAXIBasicParameters->m_chReceivedSendData ) );
    }
}

void CPCLIImplementationBAXI::_logSendFailure( const TString& p_strFunctionName ) const
{
    LogError( _s( L"[send][" ) + p_strFunctionName + _s( L"] call failed: " ) + TString::from_native_string( m_pBAXIBasicParameters->getError( ) ) );

    //ds only log error code if set
    if( 0 != m_pBAXIBasicParameters->m_lLastError )
    {
        try
        {
            LogError( _s( L"[send][" ) + p_strFunctionName + _s( L"] LastError: " ) + TString::from_numerical< int >( m_pBAXIBasicParameters->m_lLastError ) );
        }
        catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
        {
            //ds weird error
            LogError( _s( L"[send][" ) + p_strFunctionName + _s( L"] LastError: could not be parsed." ) );
        }
    }

    //ds only log event text if available
    if( true == m_pBAXIBasicParameters->m_bIsEventTextSet )
    {
        for( unsigned int u = 0; u < m_pBAXIBasicParameters->getEventTextBufferIndex( ); ++u )
        {
            LogError( _s( L"[send][" ) + p_strFunctionName + _s( L"] Event: " ) + TString::from_native_string( m_pBAXIBasicParameters->getEventTextBuffer( u ) ) );
        }
    }
    else
    {
        LogError( _s( L"[send][" ) + p_strFunctionName + _s( L"] no Events received." ) );
    }

    //ds only log display text if available
    if( true == m_pBAXIBasicParameters->m_bIsDisplayTextSet )
    {
        for( unsigned int u = 0; u < m_pBAXIBasicParameters->getDisplayTextBufferIndex( ); ++u )
        {
            LogError( _s( L"[send][" ) + p_strFunctionName + _s( L"] DisplayText: " ) + TString::from_native_string( m_pBAXIBasicParameters->getDisplayTextBuffer( u ) ) );
        }
    }

    //ds only log received send data if available
    if( true == m_pBAXIBasicParameters->m_bIsCallSendData )
    {
        LogError( _s( L"[send][" ) + p_strFunctionName + _s( L"] received SendData: " ) + TString::from_native_string( m_pBAXIBasicParameters->m_chReceivedSendData ) );
    }

    LogInfo( _s( L"-----------------------------------------------------------------------------------------------" ) + TChar::newline( ) + TChar::space( ) );
}

const std::string CPCLIImplementationBAXI::_formatReceiveReceipt( const char* p_charText, unsigned short p_uReceiptFormat ) const
{
 
    //gv first copy the char array into a standard string
    std::string p_strText = p_charText;

    LogInfo( _s( L"- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -" ) );
    LogInfo( _s( L"[receive] ticket text (raw):" ) );
    TString strBar;
    for( unsigned int u = 0; u < p_uReceiptFormat; ++u )
    {
        strBar += TChar::minus( );
    }
    LogInfo( strBar );
    LogInfo( _s( L"ticket HEX dump:  " ) + TByteStream( p_strText ).toHexDumpString( eCOREDTHexdumpFormat_Separated ) );
    LogInfo( strBar );
    LogInfo( TString::from_native_string( p_strText ) );
    LogInfo( strBar );

    //gv format receipt
    p_strText = CPCLIStringUtilities::removeCtrlCharsExt( p_strText );
    p_strText = CPCLIStringUtilities::justifyTicket( p_strText,  p_uReceiptFormat );

    TString p_strTicketDataUc;

    //gv convert text from expected encoding "UTF-8" to Unicode
	switch ( m_eExpectedEncoding )
	{
		case ePPIITerminalCodePage_Latin1:
		{
			//dh convert
			p_strTicketDataUc = TString::from_string( p_strText, eCOREDTCodepage_iso8859_1 );

			//dh leave here
			break;
		}
		case ePPIITerminalCodePage_Latin2:
		{
			//dh convert
			p_strTicketDataUc = TString::from_string( p_strText, eCOREDTCodepage_ibm_912_P100_1995 );

			//dh leave here
			break;
		}
		case ePPIITerminalCodePage_Cp1252:
		{
			//dh convert
			p_strTicketDataUc = TString::from_string( p_strText, eCOREDTCodepage_ibm_5348_P100_1997 );

			//dh leave here
			break;
		}
		case ePPIITerminalCodePage_Cp850:
		{
			//dh convert
			p_strTicketDataUc = TString::from_string( p_strText, eCOREDTCodepage_ibm_850_P100_1995 );

			//dh leave here
			break;
		}
		case ePPIITerminalCodePage_Utf8:
		{
			//dh convert
			p_strTicketDataUc = TString::from_string( p_strText, eCOREDTCodepage_utf8 );

			//dh leave here
			break;
		}
		default:
		{
			//dh must not happen
			PASSERT( 0 );
		}
	}

    //gv convert back from Unicode to native codepage...
    p_strText = p_strTicketDataUc.to_native_string( );

    //gv ...and return it
    return p_strText;
}

const bool CPCLIImplementationBAXI::_loadWrapper( )
{
    //ds load wrapper dll
    HINSTANCE hWrapper = ::CoLoadLibrary( L"baxiwrapper.dll", true );

    //ds if dll not loaded
    if( NULL == hWrapper )
    {
        LogError( _s( L"[_loadWrapper] could not load baxiwrapper.dll in PWD." ) );

        //ds dll not loaded
        return false;
    }

    //ds map all functions to the dll
    _initializeWrapper     = ( pInitializeWrapper )		WIN_GET_PROC_ADDRESS( hWrapper, "initializeWrapper" );
    _exitWrapper           = ( pExitWrapper )			WIN_GET_PROC_ADDRESS( hWrapper, "exitWrapper" );
    _openBAXI              = ( pOpenBAXI )				WIN_GET_PROC_ADDRESS( hWrapper, "openBAXI" );
    _sendData              = ( pSendData )				WIN_GET_PROC_ADDRESS( hWrapper, "sendData" );
    _processTransaction    = ( pProcessTransaction )	WIN_GET_PROC_ADDRESS( hWrapper, "processTransaction" );
    _processAdministration = ( pProcessAdministration ) WIN_GET_PROC_ADDRESS( hWrapper, "processAdministration" );
    _transferCardData      = ( pTransferCardData )		WIN_GET_PROC_ADDRESS( hWrapper, "transferCardData" );
    _closeBAXI             = ( pCloseBAXI )				WIN_GET_PROC_ADDRESS( hWrapper, "closeBAXI" );
    //gv get Version number
    _getBaxiwrapperVersion = ( pGetBaxiwrapperVersion ) WIN_GET_PROC_ADDRESS( hWrapper, "getBaxiwrapperVersion" );

    //ds check if all were set
    if( NULL == _initializeWrapper     ||
        NULL == _exitWrapper           ||
        NULL == _openBAXI              ||
        NULL == _sendData              ||
        NULL == _processTransaction    ||
        NULL == _processAdministration ||
        NULL == _transferCardData      ||
        NULL == _closeBAXI             ||
        NULL == _getBaxiwrapperVersion     )
    {
        LogError( _s( L"[_loadBAXI] could not map a function to the dll." ) );

        //ds dll function pointer not set
        return false;
    }

    //ds initialize the wrapper
    return _initializeWrapper( m_pBAXIBasicParameters.get( ) );
}


} // namespace pep2

#endif // #ifndef _X64
#endif // #ifdef _WINDOWS
