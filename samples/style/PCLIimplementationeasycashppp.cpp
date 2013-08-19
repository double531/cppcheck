#include "PCLIimplementationeasycashppp.h"

//ds execeptions
#include "PCLIexceptions.h"
#include "PGSUexceptions.h"

//ds specific includes
#include <OSABLYosapi.h>
#include "PCLIframework1.h"
#include "PCLIticket.h"
#include "PCLIcardcountertable.h"
#include "PCLCcommunicationtcpsimple.h"
#include "PCLIstringutilities.h"
#include "PGSUparseriso78132.h"
#include "LOGGER.h"
#include "PCLIdatetimeutilities.h"
#include "PCLIfileutilities.h"
#include "PCLIxmlutilities.h"

//dh this must always be the last include in every implementation file
#include "PCLIlastinclude.h"


namespace pep2
{


//ds global version variable :S
const std::string CPCLIImplementationEasycashPPP::version = "easycash PrePaidPlus V. 1.0";


CPCLIImplementationEasycashPPP::CPCLIImplementationEasycashPPP( ) : m_strTerminalID( _s( L"INVALID" ) ), 
                                                                                        m_strBranchID( _s( L"INVALID" ) ),
                                                                                        m_uReceiveTimeOut( 90000 ),
                                                                                        m_bIsResponseValid( false ),
                                                                                        m_bAreAllResponseTagsSet( false ),
                                                                                        m_bIsTransactionAlreadyReceived( false )
{
    //ds check if transaction persistence is not stored
    if( 0 == CPCLIPersistenceEasycashPPPTransaction::getuPersistentCount( ) )
    {
        //ds create new instance in db and link it to our working pointer
        m_pPersistenceTransaction = CPCLIPersistenceEasycashPPPTransaction::TPtr( new CPCLIPersistenceEasycashPPPTransaction );
    }
    else
    {
        //ds link our pointer to the retrieved object of persistence
        m_pPersistenceTransaction = CPCLIPersistenceEasycashPPPTransaction::persistentRetrieve( );
    }

    //ds if pointer was not set
    if( NULL == m_pPersistenceTransaction )
    {
        //ds log only here (no throws in constructor, we will throw later)
        LogError( _s( L"[constructor] persistence transaction pointer not set." ) );
    }

//******************************************************************************** OLD PERSISTENCE ********************************************************************************

    //ds try to set shift sum
    try
    {	
        //ds check if not set in persistence
        if( "" == CPCLIUtilities::getClassicPersistentProperty( "shiftSum" ) )
        {
            //ds initialize with intial value
            CPCLIUtilities::setClassicPersistentProperty( "shiftSum", "0.0" );
        }

        //ds and set it instance
        shiftSum = ( TString::from_native_string( CPCLIUtilities::getClassicPersistentProperty( "shiftSum" ) ) ).to_numerical< double >( );
    }
    catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
    {
        LogWarning( _s( L"[constructor] could not convert PersistentProperty shiftSum to double." ) );
    }

//*********************************************************************************************************************************************************************************

}

CPCLIImplementationEasycashPPP::~CPCLIImplementationEasycashPPP( )
{
    //ds no dynamic memory unhandled allocated (smart ptrs ftw)
}

long CPCLIImplementationEasycashPPP::specificationDependantValidation( short method, boost::shared_ptr< CPCLIMessage > in_outMsg ) 
{
    //ds logging
	LogInfo( TString::sprintf( _s( L"[validation] method: %i" ) ) % method );

    switch( method )
    {
        case( EFTCONFIGDRIVER ):
        {
			//dh check language
			switch ( in_outMsg->geteLanguageCode() )
			{
				case ePPIILanguage_English:
				case ePPIILanguage_German:
				case ePPIILanguage_French:
				{
					//dh these are ok
					break;
				}

				default:
				{
					LogError( _s( L"[validation] in_outMsg->languageCode: Only the codes \"0\" (English), \"1\" (German) and \"2\" (French) are supported." ) );
					in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

					//ds invalid languageCode
					return eErrorCode_PARAMETER_VALIDATION;
				}
			}

            break;
        }
        case( EFTUTILITY ):
        {
            //ds check if unkown utility parameter was entered
            if( "a" != in_outMsg->utilityOpCode &&
                "b" != in_outMsg->utilityOpCode &&
                "c" != in_outMsg->utilityOpCode &&
                "d" != in_outMsg->utilityOpCode &&
                "e" != in_outMsg->utilityOpCode &&
                "s" != in_outMsg->utilityOpCode &&
                "z" != in_outMsg->utilityOpCode &&
                !( "j" == in_outMsg->utilityOpCode && in_outMsg->allowUtilityJ == true ) )
            {
                LogWarning( _s( L"[validation] in_outMsg->utilityOpCode: " ) + TString::from_native_string( in_outMsg->utilityOpCode ) + _s( L" not supported." ) );
                in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

                //ds invalid utility
                return eErrorCode_PARAMETER_VALIDATION;
            }


            //ds clear journal
            in_outMsg->journalText ="";
            break;
        }
        case( EFTTRX ):
        {
			LogInfo( TString::sprintf( _s( L"[validation] in_outMsg->trxType: %u" ) ) % ( static_cast< unsigned int >( in_outMsg->trxType ) ) );

            //ds first check if recovery desired
            if( TRXTYPE_RECOVERY == in_outMsg->trxType )
            {
                //ds nothing more to do for recovery method here
                break;
            }

            //ds just 4 options are allowed to pass here, report all other desired methods as error   
            if( TRXTYPE_GOODS_PAYM      != in_outMsg->trxType &&   //if not a goods payment
                TRXTYPE_VOID_GOODS_PAYM != in_outMsg->trxType &&   //if not a void (storno)
                TRXTYPE_CREDIT          != in_outMsg->trxType &&   //if not a credit
                TRXTYPE_VOID_CREDIT     != in_outMsg->trxType &&   //if not a void credit
                TRXTYPE_BALANCE         != in_outMsg->trxType )    //if not a balance request
            {
				LogWarning( TString::sprintf( _s( L"[validation] in_outMsg->trxType: %u not supported." ) ) % static_cast< unsigned int >( in_outMsg->trxType ) );
                in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

                //ds invalid transaction type
                return eErrorCode_PARAMETER_VALIDATION;
            }
            
            //ds track presences: only 1 & 2
            if( TRACK_MANUAL  != in_outMsg->trackPresence &&   //if not manual
                TRACK_ISO2    != in_outMsg->trackPresence )    //if not magnet
            {
				LogWarning( TString::sprintf( _s( L"[validation] in_outMsg->trackPresence: %u not supported." ) ) % ( in_outMsg->trackPresence ) );
                in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

                //ds invalid track presence
                return eErrorCode_PARAMETER_VALIDATION;
            }

            //ds 123456789012 check if amount is in the defined range (12 digits)
            if( 999999999999.0 < in_outMsg->getAmount( ) || 0.0 > in_outMsg->getAmount( ) )
            {
				LogWarning( TString::sprintf( _s( L"[validation] in_outMsg->amount: %f must be equal or shorter than 12 digits." ) )  % ( in_outMsg->getAmount() ) );
                in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

                //ds bad amount
                return eErrorCode_PARAMETER_VALIDATION;
            }
    
            //ds only EURO as currency allowed
            if( "EUR" != in_outMsg->currency )
            {
                LogWarning( _s( L"[validation] in_outMsg->currency: " ) + TString::from_native_string( in_outMsg->currency ) + _s( L" not supported." ) );
                in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

                //ds invalid currency
                return eErrorCode_PARAMETER_VALIDATION;
            }

            //ds check if balance is chosen
            if( TRXTYPE_BALANCE == in_outMsg->trxType )
            {
                //ds escape if amount is not 0
                if( 0 != in_outMsg->getAmount( ) )
                {
					LogWarning( TString::sprintf( _s( L"[validation] in_outMsg->amount: %f must be zero for BALANCE." ) ) % ( in_outMsg->getAmount() ) );
                    in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

                    //ds wrong amount for balance
                    return eErrorCode_PARAMETER_VALIDATION;
                }
                break;
            }

            //ds check if void goods payment or void credit is chosen
            if( TRXTYPE_VOID_GOODS_PAYM == in_outMsg->trxType || TRXTYPE_VOID_CREDIT == in_outMsg->trxType )
            {
                //ds escape if reference is not set
                if( "" == in_outMsg->trxRefNbrIn )
                {
                    LogWarning( _s( L"[validation] reference number is not set." ) );
                    in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

                    //ds no reference
                    return eErrorCode_PARAMETER_VALIDATION;
                }
				//bk trxRefNbrIn = 0 is a valid case, therefore no 0 check is made (see TRAC Bug 2615)
                break;

                //gv check the length of the construction site number
                if( 12 > m_strConstructionSiteNumber.length( ) )
                {
                    LogWarning( _s( L"[validation] Invalid length of construction site number! Length must be max. 12 characters." ) );
                    in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );
                    return eErrorCode_PARAMETER_VALIDATION;
                }
                if( 18 > m_strReferenceNumber.length( ) )
                {
                    LogWarning( _s( L"[validation] Invalid length of reference number! Length must be max. 18 characters." ) );
                    in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );
                    return eErrorCode_PARAMETER_VALIDATION;
                }
            }
            break;
        }
        case( EFTSTATUS ):
        {
            LogInfo( _s( L"[validation] eftStatus is not supported." ) );
            in_outMsg->setDisplayCode( eErrorCode_METHOD_NOT_SUPPORTED );
    
            //ds eftStatus not supported in easycash PPP protocol
            return eErrorCode_METHOD_NOT_SUPPORTED;
        }
        case( EFTINIT ):
        {
            LogInfo( _s( L"[validation] eftInit is not supported." ) );
            in_outMsg->setDisplayCode( eErrorCode_METHOD_NOT_SUPPORTED );
    
            //ds eftInit not supported in easycash PPP protocol
            return eErrorCode_METHOD_NOT_SUPPORTED;
        }
        default:
        {
            //ds just log untreated methods as info
			LogInfo( TString::sprintf( _s( L"[validation] method: %i no validation required." ) ) % ( in_outMsg->eftMethod ) );
            break;
        }
    }

    //ds if we are still here all input is valid
    return eErrorCode_OK;
}

long CPCLIImplementationEasycashPPP::stopProtocol( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds not implemented
    return eErrorCode_OK;
}

long CPCLIImplementationEasycashPPP::startProtocol( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds not implemented
    return eErrorCode_OK;
}

void CPCLIImplementationEasycashPPP::addTicketHeaderFooter( std::string printFile, boost::shared_ptr< CPCLIMessage > in_outMsg ) 
{
    //ds add the footer
    IPCLIImplementation::addTicketHeaderFooter( printFile, in_outMsg );
}

long CPCLIImplementationEasycashPPP::initDevice( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds check if nothing was entered
    if( 0 == in_outMsg->ipAddress.length( ) )
    {
        LogError( _s( L"[initDevice] no IP address set." ) );
        in_outMsg->setDisplayCode( eErrorCode_MISSING_PARAMETER );

        //ds misssing ip information
        return eErrorCode_MISSING_PARAMETER;
    }

    //ds check if port defined -> IP communication
    const unsigned int uPortSeparator = ( TString::from_native_string( in_outMsg->ipAddress ) ).find( TChar::colon( ) );

    //ds if port was found
    if ( TString::npos( ) != uPortSeparator )
    {
        //ds save port number
        const TString strIpPort( TString::from_native_string( in_outMsg->ipAddress.substr( uPortSeparator + 1 ) ) );

        //ds check if empty
        if( 0 == strIpPort.length( ) )
        {
            LogError( _s( L"[initDevice] no IP port set." ) );
            in_outMsg->setDisplayCode( eErrorCode_MISSING_PARAMETER );

            //ds misssing ip:port
            return eErrorCode_MISSING_PARAMETER;
        }

        LogInfo( _s( L"[initDevice] starting communication with: " ) + TString::from_native_string( in_outMsg->ipAddress )   );

        //ds save correct IP number
        in_outMsg->ipAddress = in_outMsg->ipAddress.substr( 0, uPortSeparator );

        //ds start communication handle with TcpSimple
        comm = boost::shared_ptr< IPCLCCommunication >( new CPCLCCommunicationTcpSimple( in_outMsg->ipAddress, strIpPort.to_native_string( ) ) );

        //ds initialize the IN/OUT message
        return IPCLIImplementation::initDevice( in_outMsg );
    }
    else
    {
        LogError( ( _s( L"[initDevice] no IP port separator found." ) ) );
        in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

        //ds no IP port found
        return eErrorCode_PARAMETER_VALIDATION;
    }
}


long CPCLIImplementationEasycashPPP::setConnectedState( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds not implemented
    return eErrorCode_OK;
}

long CPCLIImplementationEasycashPPP::setDisconnectedState( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds not implemented
    return eErrorCode_OK;
}

long CPCLIImplementationEasycashPPP::send( short method, boost::shared_ptr< CPCLIMessage > in_outMsg )
{
	LogInfo( TString::sprintf( _s( L"[send] method: %i" ) ) % method );

    //ds if efttrx and not recovery
    if( EFTTRX == method && TRXTYPE_RECOVERY != in_outMsg->trxType )
    {
        LogInfo( TString::sprintf( _s( L"[send] in_outMsg->trxType: %u" ) ) % ( static_cast< unsigned int >( in_outMsg->trxType ) ) );

        if( false == isEFTAvailable( in_outMsg ) )
        {
            LogError( _s( L"[send] EFT not available." ) );
            in_outMsg->setDisplayCode( eErrorCode_COMMUNICATION_NO_EFT );

            //ds eft not available
            return eErrorCode_COMMUNICATION_NO_EFT;
        }
        
        //ds stream to send holder
        std::string strStreamToSend = "";
        
        //ds determine which message should be sent
        switch( in_outMsg->nofActualStreamToSend )
        {
            case( 0 ):
            {
                strStreamToSend = in_outMsg->streamToSend1;
                break;
            }
            case( 1 ):
            {
                strStreamToSend = in_outMsg->streamToSend2;
                break;
            }
            case( 2 ):
            {
                strStreamToSend = in_outMsg->streamToSend2;
                break;
            }
        }
       
        //ds new line for better readable
        LogInfo( _s( L"\n\n----------------------------REQUEST-----------------------------\n" ) );

        //ds add debug entry of stream to send
        LogInfo( TString::from_native_string( strStreamToSend ) );

        //ds new line for better readable
        LogInfo( _s( L"----------------------------------------------------------------\n\n" ) );
      
        //ds if communication is open
        if( NULL != comm ) 
        {
            //ds send the stream
            return comm->send( strStreamToSend );
        }
        else
        {
            LogError( _s( L"[send] communication object not available." ) );
            in_outMsg->setDisplayCode( eErrorCode_COMMUNICATION_NO_EFT );

            //ds no comm
            return eErrorCode_COMMUNICATION_NO_EFT;
        }
    }

    //ds recovery
    else if( TRXTYPE_RECOVERY == in_outMsg->trxType )
    {
        //ds set recovery to done
        in_outMsg->m_bNoRecoveryNeededFlag = true;
        in_outMsg->setDisplayCode( eErrorCode_RECOVERY_OK );
        in_outMsg->eftError = false;

        return eErrorCode_RECOVERY_OK;
    }

    //ds anything else is okay
    else
    {
        return eErrorCode_OK;
    }
}

long CPCLIImplementationEasycashPPP::receive( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds initialize all state parameters
    in_outMsg->eftError         = false;
	in_outMsg->displayText.clear( );
    in_outMsg->cardType         = CRD_UNKNOWN;
	in_outMsg->cardName.clear( );
	in_outMsg->cardNbr.clear( );
	in_outMsg->expDate.clear( );
	in_outMsg->trxRefNbrOut.clear( );
	in_outMsg->trxDate.clear( );
	in_outMsg->trxTime.clear( );
	in_outMsg->authNbr.clear( );;
	in_outMsg->terminalID.clear( );
	in_outMsg->bookkeepPeriod.clear( );
	in_outMsg->contractNbr.clear( );
	in_outMsg->receiptSignature = ePPIIReceiptSignature_Nothing;
	in_outMsg->referralText.clear( );
	in_outMsg->journalLevel.clear( );
    
    //ds MOCKED RECEIVE (for EndOfDay only)
    if( EFTENDOFDAY == in_outMsg->eftMethod )
    {
        return _receiveEndOfDay( in_outMsg ); 
    }

    //ds NORMAL RECEIVE
    else
    {
        return _receiveTransaction( in_outMsg );
    }
}

long CPCLIImplementationEasycashPPP::prepareTrxMsg( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{                                    
    //ds skip this for recovery
    if( in_outMsg->isRecoveryTrx )
    {
        //ds set number of streams to send to zero
        in_outMsg->nofStreamToSend = 0;

        in_outMsg->setDisplayCode( eErrorCode_RECOVERY_OK );

        //ds recovery always ok here
        return eErrorCode_RECOVERY_OK;
    }

    //ds if pointer was not set
    if( NULL == m_pPersistenceTransaction )
    {
        LogError( _s( L"[prepareTrxMsg] persistence transaction pointer not set." ) );

        //ds set error text
        in_outMsg->displayText = _getText( eEasycashPPP_eDisplayPersistenceFail, in_outMsg->geteLanguageCode() ).to_native_string( );
        in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );

        //ds pointer not set
        return eErrorCode_NOT_OK; 
    }

    //ds prepare data holders
    in_outMsg->streamToSend1 = "";  //ds stream to send message with
    in_outMsg->receiptText   = "";  //ds empty receipt


    //ds MASK METHODS
    //ds switch-hardcoded values (defined by costumer)
    TString strMessageTypeID;   //ds possible values: 200, 400
    TString strRequestType;     //ds possible values: 00, 20 
    TString strEntryMode;       //ds possible values: 01, 02, 03

    //ds get current track presence value
    strEntryMode = TString::from_numerical( in_outMsg->trackPresence );

    //ds lengthen it to the according format (0x)
    strEntryMode.lengthen( 2, eCOREDTAlignMode_Right, TChar::zero( ) );

    //ds reference number (already here instanciated to set it in the next switch)
    TString strReferenceNumber;

    //ds switch through all possible trx methods and set values as specified from costumer
    switch( in_outMsg->trxType )
    {
        //ds fallthrough is intended ;)
        case( TRXTYPE_BALANCE ):
        case( TRXTYPE_GOODS_PAYM ):
        {
            strMessageTypeID = _s( L"200" );
            strRequestType   = _s( L"00" );
            break;
        }
        case( TRXTYPE_VOID_GOODS_PAYM ):
        {
            strMessageTypeID   = _s( L"400" );
            strRequestType     = _s( L"00" );

            //ds set reference number
            strReferenceNumber = TString::from_native_string( in_outMsg->trxRefNbrIn );

            //ds check it
            if( false == _isANumber( strReferenceNumber ) )
            {
                LogError( _s( L"[prepareTrxMsg] reference number is not set/a number." ) );
                in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

                //ds persistence parameter invalid
                return eErrorCode_PARAMETER_VALIDATION;
            }
            else
            {
                try
                {
                    //ds also set persistence
                    m_pPersistenceTransaction->setReferenceNumber( strReferenceNumber.to_numerical< unsigned int >( ) );
                }
                catch( const XPCLIStringConversionFailedException& /*ex*/ )
                {
                    LogWarning( _s( L"[prepareTrxMsg] could not set persistence reference number." ) );
                }
                catch( const XPCLIPersistenceException& /*ex*/ )
                {
                    LogWarning( _s( L"[prepareTrxMsg] could not set persistence reference number." ) );
                }
            }
            break;
        }
        case( TRXTYPE_CREDIT ):
        {
            strMessageTypeID = _s( L"200" );
            strRequestType   = _s( L"20" );
            break;
        }
        case( TRXTYPE_VOID_CREDIT ):
        {
            strMessageTypeID = _s( L"400" );
            strRequestType   = _s( L"20" );

            //ds set reference number
            strReferenceNumber = TString::from_native_string( in_outMsg->trxRefNbrIn );

            //ds check it
            if( false == _isANumber( strReferenceNumber ) )
            {
                LogError( _s( L"[prepareTrxMsg] reference number is not set/a number." ) );
                in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

                //ds persistence parameter invalid
                return eErrorCode_PARAMETER_VALIDATION;
            }
            else
            {
                try
                {
                    //ds also set persistence
                    m_pPersistenceTransaction->setReferenceNumber( strReferenceNumber.to_numerical< unsigned int >( ) );
                }
                catch( const XPCLIStringConversionFailedException& /*ex*/ )
                {
                    LogWarning( _s( L"[prepareTrxMsg] could not set persistence reference number." ) );
                }
                catch( const XPCLIPersistenceException& /*ex*/ )
                {
                    LogWarning( _s( L"[prepareTrxMsg] could not set persistence reference number." ) );
                }
            }
            break;
        }
        default:
        {
			LogError( TString::sprintf( _s( L"[prepareTrxMsg] trxType: %u not supported." ) ) % ( static_cast< unsigned int >( in_outMsg->trxType ) ) );
            in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

            //ds bad transaction type
            return eErrorCode_PARAMETER_VALIDATION;
        }
    }
    

    //ds GET & SET ADDITIONAL PARAMETERS
    //ds get persistence values
    TString strTraceNumber;
    TString strSequenceNumber;

    try
    {
        //ds set the numbers    
        strTraceNumber    = TString::from_numerical< unsigned int >( m_pPersistenceTransaction->getTraceNumber( ) );
        strSequenceNumber = TString::from_numerical< unsigned int >( m_pPersistenceTransaction->getSequenceNumber( ) );

        //ds increment the trace number
        m_pPersistenceTransaction->incrementTraceNumber( );
    }
    catch( const XPCLIPersistenceException& /*ex*/ )
    {
        LogError( _s( L"[prepareTrxMsg] could not work with persistence." ) );

        //ds set error text
        in_outMsg->displayText = _getText( eEasycashPPP_eDisplayPersistenceFail, in_outMsg->geteLanguageCode() ).to_native_string( );
        in_outMsg->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );

        //ds persistence invalid
        return eErrorCode_NOT_OK;
    } 

    //ds card information
    TString strCardNumber;
    TString strCardExpiryDate;

    //ds check input card information
    if( "" == in_outMsg->cardInformation )
    {
        LogError( _s( L"[prepareTrxMsg] missing CardInformation." ) );
        in_outMsg->setDisplayCode( eErrorCode_MISSING_PARAMETER );

        //ds empty card information
        return eErrorCode_MISSING_PARAMETER;
    }

    //ds switch desired track presence
    switch( in_outMsg->trackPresence )
    {

        case( TRACK_MANUAL ):
        {
            //ds search separator
            const unsigned int iSeparatorIndex = ( TString::from_native_string( in_outMsg->cardInformation ) ).find( _c( L'=' ) );

            //ds check for error
            if( TString::npos( ) == iSeparatorIndex )
            {
                LogError( _s( L"[prepareTrxMsg] missing '=' separator in track manual." ) );
                in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

                //ds invalid track
                return eErrorCode_PARAMETER_VALIDATION;
            }

            //ds set card number
            strCardNumber = TString::from_native_string( in_outMsg->cardInformation.substr( 0, iSeparatorIndex ) );

            //ds check if valid
            if( false == _isANumber( strCardNumber ) )
            {
                LogError( _s( L"[prepareTrxMsg] CardNumber: " ) + strCardNumber + _s( L" is not a number." ) );
                in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

                //ds invalid track
                return eErrorCode_PARAMETER_VALIDATION;
            }

            //ds check if date is parseable
            if( ( iSeparatorIndex + 5 ) <=( in_outMsg->cardInformation ).length( ) )
            { 
                //ds get the single digit pairs for expiry date
                TString strMonth( TString::from_native_string( in_outMsg->cardInformation.substr( iSeparatorIndex + 1, 2 ) ) );
                TString strYear( TString::from_native_string( in_outMsg->cardInformation.substr( iSeparatorIndex + 3, 2 ) ) );

                try
                {
                    //ds check if any string is invalid
                    if( 12 < strMonth.to_numerical< unsigned int >( ) || 99 < strYear.to_numerical< unsigned int >( ) )
                    {
                        LogError( _s( L"[prepareTrxMsg] invalid date format in card information." ) );
                        in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

                        //ds invalid date format
                        return eErrorCode_PARAMETER_VALIDATION;
                    }
                }
                catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
                {
                        LogError( _s( L"[prepareTrxMsg] invalid date format in card information." ) );
                        in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

                        //ds invalid date format
                        return eErrorCode_PARAMETER_VALIDATION;
                } 

                //ds save and swap it (reason: pepper only accepts expDate in format MMYY but costumer wants YYMM)
                strCardExpiryDate = strYear + strMonth;
            }
            else
            {
                LogError( _s( L"[prepareTrxMsg] date not parseable out of card information." ) );
                in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

                //ds date string not found
                return eErrorCode_PARAMETER_VALIDATION;
            }
            break;
        }
        case( TRACK_ISO2 ):
        {
            const TString strCardInformation( TString::from_native_string( in_outMsg->cardInformation ) );

            try
            {
                //ds instanciate object (parses automatically the input)
                genservices::CPGSUParserIso78132 cParsedTrack2( strCardInformation );

                //ds set card number
                strCardNumber = cParsedTrack2.getstrPrimaryAccountNumber( );
                
                //ds set expiration date if set
                if( true == cParsedTrack2.gettmExpirationDate( ).isValid( ) && false == cParsedTrack2.gettmExpirationDate( ).isInfiniteFuture( ) )
                {
                    //ds get the year
                    TString strYear( TString::from_numerical< unsigned int >( cParsedTrack2.gettmExpirationDate( ).getuYear( ) ) );

                    //ds cut the prefix to 2 digits
                    strYear = strYear.substr( 2, 2 );

                    //ds get the month and format to fill 2 digits
                    const TString strMonth( TString::sprintf( _s( L"%02u" ) ) % cParsedTrack2.gettmExpirationDate( ).getuMonth( ) );

                    //ds combine
                    strCardExpiryDate = strYear + strMonth;
                }
                else if( true == cParsedTrack2.gettmExpirationDate( ).isInfiniteFuture( ) ) 
                {
                    //ds set to infinity for pepper
                    strCardExpiryDate = TString::from_native_string( Card_NoExpirationDate );
                }                                                    
            }
            catch( const genservices::XPGSUParserIso78132Exception& /*ex*/ )
            {
                LogError( _s( L"[prepareTrxMsg] invalid format of track 2 (must be ISO7813-2)." ) );
                in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

                //ds track 2 string invalid
                return eErrorCode_PARAMETER_VALIDATION;
            }
            break;
        }
        default:
        {
			LogError( TString::sprintf( _s( L"[prepareTrxMsg] trackPresence: %u not supported." ) ) % ( in_outMsg->trackPresence ) );
            in_outMsg->setDisplayCode( eErrorCode_PARAMETER_VALIDATION );

            //ds card information not valid
            return eErrorCode_PARAMETER_VALIDATION;
        }
    }
    
    //ds Ttime instances (for Time & Date information in request, to be the most accurate instanciate it just here)
    TTime cCurrentTime = TTime::getNowLocal( );
    TString strTime( TString::sprintf( _s( L"%02u%02u%02u" ) ) % cCurrentTime.getuHour( ) % cCurrentTime.getuMinute( ) % cCurrentTime.getuSecond( ) );
    TString strDate( TString::sprintf( _s( L"%02u%02u" ) ) % cCurrentTime.getuMonth( ) % cCurrentTime.getuDay( ) );


    //ds BUILD XML MESSAGE
    //ds set root name
    TXmlTree cTree( _s( L"AutSys" ) );

    //ds set document type (hacked)
    cTree.setDocumentType( _s( L"AutSys" ), TString( ), _s( L"Request.dtd" ) );

    //ds set head attributes
    ( *cTree ).setAttribute( _s( L"version"), _s( L"1" ) );
    ( *cTree ).setAttribute( _s( L"source"), _s( L"AutSys" ) );

    //ds set request specification
    TXmlTree::TNode& cRequestNode( cTree.addChild( _s( L"Request" ) ) );

    //ds add all tags in defined order (with constant values if desired by costumer)
    TXmlTree::TNode& cMessageType( cRequestNode.addChild( _s( L"MessageType" ) ) );
    ( *cMessageType ).setAttribute( _s( L"ID" ), strMessageTypeID );

    cRequestNode.addChild( TXmlNode( _s( L"CardNumber" )     , strCardNumber ) );
    cRequestNode.addChild( TXmlNode( _s( L"RequestType" )    , strRequestType ) );
    cRequestNode.addChild( TXmlNode( _s( L"Amount" )         , TString::from_numerical( in_outMsg->getAmount( ) ) ) );
    cRequestNode.addChild( TXmlNode( _s( L"TraceNumber" )    , strTraceNumber ) );
    cRequestNode.addChild( TXmlNode( _s( L"Time" )           , strTime ) );
    cRequestNode.addChild( TXmlNode( _s( L"Date" )           , strDate ) );
    cRequestNode.addChild( TXmlNode( _s( L"ExpiryDate" )     , strCardExpiryDate ) );   
    cRequestNode.addChild( TXmlNode( _s( L"TransactionDate" ), _s( L"0000" ) ) );
    cRequestNode.addChild( TXmlNode( _s( L"EntryMode" )      , strEntryMode ) );
    cRequestNode.addChild( TXmlNode( _s( L"Condition" )      , _s( L"00" ) ) );

    //ds add this line only if void payment or void credit is chosen (reference is needed)
    if( TRXTYPE_VOID_GOODS_PAYM == in_outMsg->trxType || TRXTYPE_VOID_CREDIT == in_outMsg->trxType )
    {
        cRequestNode.addChild( TXmlNode( _s( L"Reference" ), strReferenceNumber ) );
    }

    cRequestNode.addChild( TXmlNode( _s( L"Terminal-ID" )    , m_strTerminalID ) );
    cRequestNode.addChild( TXmlNode( _s( L"Branch-ID" )      , m_strBranchID ) );
    cRequestNode.addChild( TXmlNode( _s( L"CCTI" )           , _s( L"89" ) ) );
    cRequestNode.addChild( TXmlNode( _s( L"Currency" )       , _s( L"978" ) ) );
    cRequestNode.addChild( TXmlNode( _s( L"Sequence" )       , strSequenceNumber ) );



    //gv add ConstructionSiteNumber and ReferenceNumber for Hornbach
    if ( ( true == getpConfigDriverInputOptions()->isBehaviour( ePPIIBehaviour_EasyCashLoyalty ) ) && ( true == getpConfigDriverInputOptions()->get< ePPIIOption_EasyCashLoyalty_AdditionalParametersLuxembourgFlag >() ) )
    {
        cRequestNode.addChild( TXmlNode( _s( L"IDNr" )          , m_strReferenceNumber ) );
        cRequestNode.addChild( TXmlNode( _s( L"BStNr" )         , m_strConstructionSiteNumber ) );
    }

    //ds SEND MESSAGE
    //ds save the xml tree as std::string bytestream in streamToSend1
    in_outMsg->streamToSend1 = cTree.toByteStream( eCOREDTXmlCodepage_iso8859_1 ).toStdString( );

    //ds set the total number of streams to send to 1 (we have only StreamToSend1)
    in_outMsg->nofStreamToSend = 1;

    //ds set display code and return
    in_outMsg->setDisplayCode( eErrorCode_OK );

    //ds everything went fine if we are still here
    return eErrorCode_OK;
}

long CPCLIImplementationEasycashPPP::prepareEndOfDayMsg( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds delete old file
    CPCLIFileUtilities::deleteFile( in_outMsg->eodPrintFile );

    //ds set number of streams that must be sent to one and mock it in receive routine
    in_outMsg->nofStreamToSend = 1;
    in_outMsg->setDisplayCode( eErrorCode_OK );

    return eErrorCode_OK;
}

long CPCLIImplementationEasycashPPP::executeUtility( boost::shared_ptr< CPCLIMessage > in_outMsg )
{   
	//dh important! Call base implementation first
    IPCLIImplementation::executeUtility(in_outMsg);

    return eErrorCode_OK;	
}

long CPCLIImplementationEasycashPPP::executeInit( boost::shared_ptr< CPCLIMessage > in_outMsg )
{	
    //ds set method
    in_outMsg->eftMethod = EFTINIT;

    return eErrorCode_OK;
}

long CPCLIImplementationEasycashPPP::prepareOpenMsg( boost::shared_ptr< CPCLIMessage > in_outMsg )
{
    //ds set method
    in_outMsg->eftMethod = EFTOPEN;
 
    return eErrorCode_OK;
}

long CPCLIImplementationEasycashPPP::prepareCloseMsg( boost::shared_ptr< CPCLIMessage > in_outMsg )
{
    //ds set method
    in_outMsg->eftMethod= EFTCLOSE;
 
    return eErrorCode_OK;
}

long CPCLIImplementationEasycashPPP::prepareStatusMsg( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds set method
    in_outMsg->eftMethod= EFTSTATUS;

    return eErrorCode_OK;
}

long CPCLIImplementationEasycashPPP::getVersion( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds get pepper version string
    in_outMsg->version = IPCLIImplementation::getPEPVersion( );
    
    //ds add local version string
    in_outMsg->version += CPCLIImplementationEasycashPPP::version;
    
    //ds set display code
    in_outMsg->setDisplayCode( eErrorCode_OK );

    return eErrorCode_OK;
}

long CPCLIImplementationEasycashPPP::startIdleProcess( boost::shared_ptr< CPCLIMessage >  in_outMsg, bool newFlag )
{
    //ds not implemented
    return eErrorCode_OK;
}

long CPCLIImplementationEasycashPPP::stopIdleProcess( bool stopFlag )
{
    //ds not implemented
    return eErrorCode_OK;	
}


short CPCLIImplementationEasycashPPP::getCardType ( boost::shared_ptr< CPCLIMessage > in_outMsg,
                                                              std::string key1,
                                                              std::string key2,
                                                              std::string key3,
                                                              std::string key4,
                                                              std::string key5                             )
{
    //ds default to zero
    short ret = CRD_UNKNOWN;	

    //ds set cardtype and cardname in in_outMsg
    ret = IPCLIImplementation::getCardType( in_outMsg, key1 );

    //ds log result informative
	LogInfo( TString::sprintf( _s( L"[getCardType] cardType = %i" ) ) % ret );

    return ret;
}

long CPCLIImplementationEasycashPPP::translateEftConfigDriver( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds set version and receipt format
    in_outMsg->version              = IPCLIImplementation::getPEPVersion( ) + version; 
    in_outMsg->desiredReceiptFormat = in_outMsg->receiptFormat; 

    //ds set return value to state
    //gv this also parses the xmlAdditionalParameters
    long lReturnValue = IPCLIImplementation::translateEftConfigDriver( in_outMsg );

    if ( eErrorCode_OK != lReturnValue )
    {
        return lReturnValue;
    }

    //dh ------------------------------------------------------------------------------------

    //dh read terminal id
    if ( true == getpConfigDriverInputOptions()->isSet( ePPIIOption_TerminalIdentificationString ) )
    {
        //dh get value
        m_strTerminalID = getpConfigDriverInputOptions()->get< ePPIIOption_TerminalIdentificationString >(); 

        if ( ( m_strTerminalID.length( ) > 9 ) || ( false == _isANumber( m_strTerminalID ) ) )
        {
            //dh log this
            LogError( _s( L"[_getXMLAdditionalParameters] TerminalID: " ) + m_strTerminalID + _s( L" invalid." ) );

            //dh leave
            return eErrorCode_MISSING_PARAMETER;
        }
    }
    else
    {
        //dh log this
        LogError( _s( L"[_getXMLAdditionalParameters] TerminalID not set." ) );

        //dh leave
        return eErrorCode_MISSING_PARAMETER;
    }

    //ds log IDs (valid if here)
    LogInfo( _s( L"[_getXMLAdditionalParameters] TerminalID: " ) + m_strTerminalID ); 

    //dh ------------------------------------------------------------------------------------

    //dh read Branch id
    if ( true == getpConfigDriverInputOptions()->isSet( ePPIIOption_BranchIdentificationString ) )
    {
        //dh get value
        m_strBranchID = getpConfigDriverInputOptions()->get< ePPIIOption_BranchIdentificationString >(); 

        if ( ( m_strBranchID.length( ) > 16 ) || ( false == _isANumber( m_strBranchID ) ) )
        {
            //dh log this
            LogError( _s( L"[_getXMLAdditionalParameters] BranchID: " ) + m_strBranchID + _s( L" invalid." ) );

            //dh leave
            return eErrorCode_MISSING_PARAMETER;
        }
    }
    else
    {
        //dh log this
        LogError( _s( L"[_getXMLAdditionalParameters] BranchID not set." ) );

        //dh leave
        return eErrorCode_MISSING_PARAMETER;
    }

    //ds log IDs (valid if here)
    LogInfo( _s( L"[_getXMLAdditionalParameters] BranchID: " ) + m_strBranchID ); 

    //dh ------------------------------------------------------------------------------------

    //dh check for time out
    if ( true == getpConfigDriverInputOptions()->isSet( ePPIIOption_TimeoutInMilliseconds ) )
    {
        //dh get it
        m_uReceiveTimeOut = getpConfigDriverInputOptions()->get< ePPIIOption_TimeoutInMilliseconds >();
    }

    //dh ------------------------------------------------------------------------------------

    return lReturnValue;
}

long CPCLIImplementationEasycashPPP::translateEftOpen( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds do translation
    return IPCLIImplementation::translateEftOpen( in_outMsg );
}

long CPCLIImplementationEasycashPPP::translateEftTrx( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds new trx
    m_bIsTransactionAlreadyReceived = false;
    
    //ds translate InOutMessage and store result
    long ret = IPCLIImplementation::translateEftTrx( in_outMsg );

    //dh leave on error
    if ( eErrorCode_OK != ret )
    {
        return ret;
    }

    //gv get Hornbach parameters Referenznummer
    m_strReferenceNumber = getpTransactionInputOptions()->get< ePPIIOption_EasyCashLoyalty_ReferenceString >();

    //gv get Hornbach parameters Baustellennummer
    m_strConstructionSiteNumber = getpTransactionInputOptions()->get< ePPIIOption_EasyCashLoyalty_ConstructionSiteString >();
    

    //gv now get the case of the letters contained in the ConstructionSiteNumber
    EPPIICaseHandling eCaseHandling = getpTransactionInputOptions()->get< ePPIIOption_EasyCashLoyalty_ConstructionSiteCaseValue >();

    //dh adapt it
    if( ePPIICaseHandling_ToUpper == eCaseHandling )
    {
        m_strConstructionSiteNumber = TString::from_native_string( CPCLIStringUtilities::toUpper( m_strConstructionSiteNumber.to_native_string( ) ) );
    }
    else if( ePPIICaseHandling_ToLower == eCaseHandling )
    {
        m_strConstructionSiteNumber = TString::from_native_string( CPCLIStringUtilities::toLower( m_strConstructionSiteNumber.to_native_string( ) ) );
    }

    return ret; 
}

long CPCLIImplementationEasycashPPP::translateEftUtility( boost::shared_ptr< CPCLIMessage >  in_outMsg )
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

long CPCLIImplementationEasycashPPP::translateEftEndOfDay( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds translate end of day
    return IPCLIImplementation::translateEftEndOfDay(in_outMsg);
}

long CPCLIImplementationEasycashPPP::translateEftInit( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds translate eft init
    return IPCLIImplementation::translateEftInit( in_outMsg );
}

long CPCLIImplementationEasycashPPP::openEFTDevice( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds default ok
    long ret = eErrorCode_OK;

    //ds prepare open message and log it
    ret = this->prepareOpenMsg( in_outMsg );
    IPCLIImplementation::logOpen( ret, in_outMsg );

    return ret;
}

long CPCLIImplementationEasycashPPP::closeEFTDevice( boost::shared_ptr< CPCLIMessage >  in_outMsg )
{
    //ds translate close
    long ret = IPCLIImplementation::translateEftClose( in_outMsg );
 
    //ds prepare close message and log it
    ret = this->prepareCloseMsg( in_outMsg );
    IPCLIImplementation::logClose( ret, in_outMsg );

    return ret;
}

void CPCLIImplementationEasycashPPP::buildClientReceipt( boost::shared_ptr< CPCLIMessage > in_outMsg, const char *receiveData )
{
    //ds escape if ticket not available
    if( NULL == ticket )
    {
        LogError( _s( L"[buildClientReceipt] ticket pointer is empty." ) );

        THROW( XPCLINULLPointerException, ErrorCategory );
    }
 
    //gv empty ticket ( overloaded )
    ticket->setTicket( "" );
    ticket->setTicket( _s( L"" ) );

    //ds line buffer instance to write format blocks
    TString strLine;
    
    //ds search for header
    switch( in_outMsg->trxType ) 
    {
        case( TRXTYPE_GOODS_PAYM ):
        {
            strLine = _getText( eEasycashPPP_eTransactionPayment, in_outMsg->geteLanguageCode() );
            ticket->appendLine( strLine );
            break;
        }
        case( TRXTYPE_VOID_GOODS_PAYM ):
        {
            strLine = _getText( eEasycashPPP_eTransactionVoidPayment, in_outMsg->geteLanguageCode() );
            ticket->appendLine( strLine );
            break;
        }
        case( TRXTYPE_CREDIT ):
        {
            strLine = _getText( eEasycashPPP_eTransactionCredit, in_outMsg->geteLanguageCode() );
            ticket->appendLine( strLine );
            break;
        }
        case( TRXTYPE_VOID_CREDIT ):
        {
            strLine = _getText( eEasycashPPP_eTransactionVoidCredit, in_outMsg->geteLanguageCode() );
            ticket->appendLine( strLine );
            break;
        }
        case( TRXTYPE_BALANCE ):
        {
            strLine = _getText( eEasycashPPP_eTransactionBalance, in_outMsg->geteLanguageCode() );
            ticket->appendLine( strLine );
            break;
        }
        default:
        {
            LogInfo( _s( L"[buildClientReceipt] invalid transaction type for client receipt." ) );

            //ds return here (results in an empty ticket)
            return;
        }
    }

    //ds append card name (empty if empty)
    ticket->appendLine( TString::from_native_string( in_outMsg->cardName ) );

    //ds format track presence
    const TString strTrackPresence( TString::sprintf( _s( L"%02s" ) ) % TString::from_numerical < short >( in_outMsg->trackPresence ) );

    //ds string buffer for card number
    TString strCardNumber = TString::from_native_string( in_outMsg->cardNbr );

    //ds if suppression possible and desired
    if( 4 < in_outMsg->cardNbr.size( ) && true == in_outMsg->panTruncation )
    {
        //ds get string size
        const unsigned int uCardNumberSize = in_outMsg->cardNbr.size( );

        //ds empty string for fillers
        strCardNumber = TString( );

        //ds fill up with X until the last 4 digits    
        strCardNumber.lengthen( uCardNumberSize - 4, eCOREDTAlignMode_Right, _c( L'X' ) );

        //ds fill the last 4 digits with the real card number equivalents
        strCardNumber += TString::from_native_string( in_outMsg->cardNbr.substr( uCardNumberSize - 4, 4 ) );
    }

    //ds append card number and track presence 
    strLine = ( strCardNumber + CPCLITicket::BTS + strTrackPresence );
    ticket->appendLine( strLine, CPCLITicket::BLOCK );

    //ds expiration date (only set it if possible, else bad substrings)
    if( 4 == ( in_outMsg->expDate ).length( ) )
    { 
        //ds get single strings to stretch for full date format
        const TString strMonth( TString::from_native_string( ( in_outMsg->expDate ).substr( 0, 2 ) ) );
        const TString strYear( TString::from_native_string( "20" + ( in_outMsg->expDate ).substr( 2, 2 ) ) );

        strLine = ( _getText( eEasycashPPP_eTransactionExpiration, in_outMsg->geteLanguageCode() ) +  CPCLITicket::BTS + strMonth + TChar::slash( ) + strYear );
        ticket->appendLine( strLine, CPCLITicket::BLOCK );
    }

    //ds empty line
    ticket->appendLine( " ", CPCLITicket::LEFT );

    //ds current date + time
    strLine = TString::from_native_string ( ticket->getDateDDMMJJJJ( "." ) + CPCLITicket::BS + ticket->getTimeHHMMSS( ":" ) );
    ticket->appendLine( strLine, CPCLITicket::BLOCK );

    //ds IDs
    strLine = _s( L"Branch-ID:" ) + CPCLITicket::BTS + m_strBranchID;
    ticket->appendLine( strLine, CPCLITicket::BLOCK );
    strLine = _s( L"Terminal-ID:" )+ CPCLITicket::BTS + m_strTerminalID;
    ticket->appendLine( strLine, CPCLITicket::BLOCK );
    strLine = _s ( L"TraceNumber:" ) + CPCLITicket::BTS + TString::from_native_string( in_outMsg->traceNr );
    ticket->appendLine( strLine, CPCLITicket::BLOCK );
    strLine = _s( L"Response-ID:" ) + CPCLITicket::BTS + TString::from_native_string( in_outMsg->authNbr );
    ticket->appendLine( strLine, CPCLITicket::BLOCK );

    ticket->appendLine( " ", CPCLITicket::LEFT );

    //ds get amount digits
    const unsigned int uFirstAmount  = static_cast< int > ( in_outMsg->getAmount( ) / 100 );
    const unsigned int uSecondAmount = static_cast< int > ( in_outMsg->getAmount( ) - ( uFirstAmount * 100 ) );

    //ds create amount string
    const TString strAmount( TString::sprintf( _s( L"%u.%02u" ) ) % uFirstAmount % uSecondAmount );

    //ds plot it
    strLine = ( _getText( eEasycashPPP_eTransactionTotal, in_outMsg->geteLanguageCode() ) + TChar::colon( ) +  CPCLITicket::BTS  + strAmount );
    ticket->appendLine( strLine, CPCLITicket::BLOCK );

    //ds current balance
    try
    {
        //ds get balance amount digits
        const unsigned int uFirstAmountBalance  = static_cast< int > ( _getAmountFromString( TString::from_native_string( in_outMsg->displayText ), _c( L',' ) ) / 100 );
        const unsigned int uSecondAmountBalance = static_cast< int > ( _getAmountFromString( TString::from_native_string( in_outMsg->displayText ), _c( L',' ) ) - ( uFirstAmountBalance * 100 ) );

        //ds create amount string
        const TString strAmountBalance( TString::sprintf( _s( L"%u.%02u" ) ) % uFirstAmountBalance % uSecondAmountBalance );

        //ds current balance
        strLine = ( _getText( eEasycashPPP_eTransactionBalanceAmount, in_outMsg->geteLanguageCode() ) + TChar::colon( ) + CPCLITicket::BTS + strAmountBalance );
        ticket->appendLine( strLine, CPCLITicket::BLOCK );
    }
    catch( const XPCLIParsingFailedException& /*ex*/ )
    {
        //ds just log and dont plot this line
        LogError( _s( L"[buildClientReceipt] Parsing of <Message> Tag not possible." ) );   
    }
    catch( const XPCLIStringConversionFailedException& /*ex*/ )
    {
        //ds just log and dont plot this line
        LogError( _s( L"[buildClientReceipt] could not convert string to numerical." ) );
    }
    ticket->appendLine( " ", CPCLITicket::LEFT );

    //gv if extended behaviour for LUX is activated, build a Merchant ticket
    if ( ( true == getpConfigDriverInputOptions()->isBehaviour( ePPIIBehaviour_EasyCashLoyalty ) ) && ( true == getpConfigDriverInputOptions()->get< ePPIIOption_EasyCashLoyalty_AdditionalParametersLuxembourgFlag >() ) )
    {
        //gv add Construction Site number
        if ( 0 < m_strConstructionSiteNumber.length( ) )
        {
            if( ePPIILanguage_German == in_outMsg->geteLanguageCode() )
            {
                strLine = _s( L"BStNr:" ) + CPCLITicket::BTS + m_strConstructionSiteNumber;
            }
            else if( ePPIILanguage_French == in_outMsg->geteLanguageCode() )
            {
                strLine = _s( L"N" ) + TChar::deg( ) + _s( L"chantier:" ) + CPCLITicket::BTS + m_strConstructionSiteNumber;
            }
            else
            {
                strLine = _s( L"ConstrSiteNbr:" ) + CPCLITicket::BTS + m_strConstructionSiteNumber;
            }

            ticket->appendLine( strLine, CPCLITicket::BLOCK );
        }

        //gv add Reference number
        if ( 0 < m_strReferenceNumber.length( ) )
        {
            if( ePPIILanguage_German == in_outMsg->geteLanguageCode() )
            {
                strLine = _s( L"RefNr:" ) + CPCLITicket::BTS + m_strReferenceNumber;
            }
            else if( ePPIILanguage_French == in_outMsg->geteLanguageCode() )
            {
                strLine = _s( L"N" ) + TChar::deg( ) + _s( L"r" ) +TChar::eacute( ) + _s( L"f:" ) + CPCLITicket::BTS + m_strReferenceNumber;
            }
            else
            {
                strLine = _s( L"RefNbr:" ) + CPCLITicket::BTS + m_strReferenceNumber;
            }
            ticket->appendLine( strLine, CPCLITicket::BLOCK );
        }
        ticket->appendLine( " ", CPCLITicket::LEFT );
    }//if

    //gv convert the ticket to std::string and justify it 
    in_outMsg->receiptText = ( ticket->getTstrTicket( ) ).to_native_string( );
    in_outMsg->receiptText = CPCLIStringUtilities::justifyTicket( in_outMsg->receiptText, in_outMsg->receiptFormat );
    ticket->setTicket( in_outMsg->receiptText );

    LogInfo( _s ( L"write client receipt:" ) );
    LogInfo( _s ( L"=================================================" ) );
    if ( !ticket->flushToFile( in_outMsg->trxPrintFile ) )
    {
        LogError(_s(L"impossible to write client receipt file"));
    }
    else
    {
        this->addTicketHeaderFooter(in_outMsg->trxPrintFile, in_outMsg);
        //write ticket to Log file
        LogInfo( TString::from_native_string( CPCLIFileUtilities::readFile( CPCLIFileUtilities::getPathFileName( ePCLIPathDescriptor_Print, in_outMsg->trxPrintFile ) ) ) );
    }
    LogInfo( _s ( L"=================================================" ) );
}

void CPCLIImplementationEasycashPPP::buildMerchantReceipt( boost::shared_ptr< CPCLIMessage > in_outMsg, const char *receiveData )
{
    //ds escape if ticket not available
    if( NULL == ticket )
    {
        LogError( _s( L"[buildMerchantReceipt] ticket poiner is empty." ) );

        THROW( XPCLINULLPointerException, ErrorCategory );
    }

    //gv for LUX ProfiCard we also need a Merchant Receipt


    //gv append empty line
    ticket->appendLine( _s( L" " ), CPCLITicket::LEFT);
    //gv append the signature line

    if ( ePPIILanguage_German == in_outMsg->geteLanguageCode() )
    {
        ticket->appendLine( _s( L"Unterschrift" ) ); 
    }
    else //gv for EN and FR
    {
        ticket->appendLine( _s( L"Signature" ) ); 
    }

    //gv append 2 empty lines
    ticket->appendLine( _s( L" " ), CPCLITicket::LEFT);
    ticket->appendLine( _s( L" " ), CPCLITicket::LEFT);

    TString tStrLine = ticket->getSequence( _s( L"." ), in_outMsg->receiptFormat );
    ticket->appendLine( tStrLine, CPCLITicket::LEFT); 

    //gv convert the ticket to std::string and justify it 
    in_outMsg->receiptText = ( ticket->getTstrTicket( ) ).to_native_string( );
    in_outMsg->receiptText = CPCLIStringUtilities::justifyTicket( in_outMsg->receiptText, in_outMsg->receiptFormat );
    ticket->setTicket( in_outMsg->receiptText );

    LogInfo( _s ( L"write merchant receipt:" ) );
    LogInfo( _s ( L"=================================================" ) );
    if ( false == ticket->flushToFile( CPCLIFileUtilities::getPathFileName( ePCLIPathDescriptor_Print, in_outMsg->ccTrxPrintFile ) ) )
    {
        LogError(_s(L"impossible to write merchant receipt file"));
    }
    else
    {
        this->addTicketHeaderFooter( in_outMsg->ccTrxPrintFile, in_outMsg);
        //write ticket to Log file
        LogInfo( TString::from_native_string( CPCLIFileUtilities::readFile( CPCLIFileUtilities::getPathFileName( ePCLIPathDescriptor_Print, in_outMsg->ccTrxPrintFile ) ) ) );
    }
    LogInfo( _s ( L"=================================================" ) );
}

bool CPCLIImplementationEasycashPPP::isEFTAvailable( boost::shared_ptr< CPCLIMessage > in_outMsg, unsigned char * lowlayer_state, bool withoutTimer, bool waitBefore )
{
    //ds always available
    in_outMsg->eftAvailable = true;

    return true;	
}

const TString CPCLIImplementationEasycashPPP::_getText( const ETextCode& p_eTextCode, EPPIILanguage p_eLanguage ) const
{   
    //ds get language
    switch( p_eLanguage )
    {
        //ds english
        case( ePPIILanguage_English ):
        {
            //ds search the according text
            switch( p_eTextCode )
            {
                case( eEasycashPPP_eDisplayInvalidResponse ):
                case( eEasycashPPP_eDisplayInvalidResponseCode ):
                {
                    return _s( L"TRANSACTION INVALID" );
                }
                case( eEasycashPPP_eDisplayPersistenceFail ):
                {
                    return _s( L"TRANSACTION VALID " ) + TChar::pipe( ) +  _s( L"DAMAGED DATABASE" );
                }
                case( eEasycashPPP_eDisplayResponseInformation ):
                {
                    return _s( L"Cur. Card Balance: " );
                }
                case( eEasycashPPP_eDisplayNoResponseInformation ):
                {
                    return _s( L"MISSING INFORMATION" ); 
                }
                case( eEasycashPPP_eEndOfDayTitle ):
                {
                    return _s( L"Final Balance" );
                }
                case( eEasycashPPP_eEndOfDayTime ):
                {
                    return _s( L"Date" ) + TChar::slash( ) + _s( L"Time" );
                }
                case( eEasycashPPP_eEndOfDayPayment ):
                case( eEasycashPPP_eTransactionPayment ):
                {
                    return _s( L"Purchase" );
                }
                case( eEasycashPPP_eEndOfDayVoidPayment ):
                case( eEasycashPPP_eTransactionVoidPayment ):
                {
                    return _s( L"Reversal Purchase" );
                }
                case( eEasycashPPP_eEndOfDayCredit ):
                case( eEasycashPPP_eTransactionCredit ):
                {
                    return _s( L"Credit" );
                }
                case( eEasycashPPP_eEndOfDayVoidCredit ):
                case( eEasycashPPP_eTransactionVoidCredit ):
                {
                    return _s( L"Reversal Credit" );
                }
                case( eEasycashPPP_eEndOfDayTotal ):
                {
                    return _s( L"Total" );
                }
                case( eEasycashPPP_eTransactionBalance ):
                {
                    return _s( L"Balance" );
                }
                case( eEasycashPPP_eTransactionExpiration ):
                {
                    return _s( L"Expiration Date" );
                }
                case( eEasycashPPP_eTransactionTotal ):
                {
                    return _s( L"Total-EFT EUR" );
                }
                case( eEasycashPPP_eTransactionBalanceAmount ):
                {
                    return _s( L"Current Balance Amount EUR" );
                }
                default:
                {
                    LogError( _s( L"[_getText] invalid english text code" ) );

                    THROW( XPCLITextNotSetException, ErrorCategory );
                }
            }
            break;
        }

        //ds german
        case( ePPIILanguage_German ):
        {
            //ds search the according text
            switch( p_eTextCode )
            {
                case( eEasycashPPP_eDisplayInvalidResponse ):
                case( eEasycashPPP_eDisplayInvalidResponseCode ):
                {
                    return _s( L"TRANSAKTION UNG" ) + TChar::Uuml( ) + _s( L"LTIG" );
                }
                case( eEasycashPPP_eDisplayPersistenceFail ):
                {
                    return _s( L"TRANSAKTION OK " ) + TChar::pipe( ) + _s( L" DATENBANK BESCH" ) + TChar::Auml( ) + _s( L"DIGT" );
                }
                case( eEasycashPPP_eDisplayResponseInformation ):
                {
                    return _s( L"Akt. Guthaben: " );
                }
                case( eEasycashPPP_eDisplayNoResponseInformation ):
                {
                    return _s( L"FEHLENDE INFORMATIONEN" ); 
                }
                case( eEasycashPPP_eEndOfDayTitle ):
                {
                    return _s( L"Kassenschnitt" );
                }
                case( eEasycashPPP_eEndOfDayTime ):
                {
                    return _s( L"Datum" ) + TChar::slash( ) + _s( L"Uhrzeit" );
                }
                case( eEasycashPPP_eEndOfDayPayment ):
                case( eEasycashPPP_eTransactionPayment ):
                {
                    return _s( L"Bezahlung" );
                }
                case( eEasycashPPP_eEndOfDayVoidPayment ):
                case( eEasycashPPP_eTransactionVoidPayment ):
                {
                    return _s( L"Storno Bezahlung" );
                }
                case( eEasycashPPP_eEndOfDayCredit ):
                case( eEasycashPPP_eTransactionCredit ):
                {
                    if ( ( true == getpConfigDriverInputOptions()->isBehaviour( ePPIIBehaviour_EasyCashLoyalty ) ) && ( true == getpConfigDriverInputOptions()->get< ePPIIOption_EasyCashLoyalty_AdditionalParametersLuxembourgFlag >() ) )
                    {
                        return _s( L"Gutschrift" );
                    }
                    else
                    {
                        return _s( L"Aufladung" );
                    }
                }
                case( eEasycashPPP_eEndOfDayVoidCredit ):
                case( eEasycashPPP_eTransactionVoidCredit ):
                {
                    if ( ( true == getpConfigDriverInputOptions()->isBehaviour( ePPIIBehaviour_EasyCashLoyalty ) ) && ( true == getpConfigDriverInputOptions()->get< ePPIIOption_EasyCashLoyalty_AdditionalParametersLuxembourgFlag >() ) )
                    {
                        return _s( L"Storno Gutschrift" );
                    }
                    else
                    {
                        return _s( L"Storno Aufladung" );
                    }
                }
                case( eEasycashPPP_eEndOfDayTotal ):
                {
                    return _s( L"Total" );
                }
                case( eEasycashPPP_eTransactionBalance ):
                {
                    return _s( L"Saldoabfrage" );
                }
                case( eEasycashPPP_eTransactionExpiration ):
                {
                    return _s( L"Verfalldatum" );
                }
                case( eEasycashPPP_eTransactionTotal ):
                {
                    return _s( L"Total-EFT EUR" );
                }
                case( eEasycashPPP_eTransactionBalanceAmount ):
                {
                    return _s( L"Aktuelles Guthaben EUR" );
                } 
                default:
                {
                    LogError( _s( L"[_getText] invalid german text code" ) );

                    THROW( XPCLITextNotSetException, ErrorCategory );
                 }
            }
            break;
        }

        //gv French
        case( ePPIILanguage_French ):
        {
            //ds search the according text
            switch( p_eTextCode )
            {
                case( eEasycashPPP_eDisplayInvalidResponse ):
                case( eEasycashPPP_eDisplayInvalidResponseCode ):
                {
                    return _s( L"TRANSACTION NON VALIDE" );
                }
                case( eEasycashPPP_eDisplayPersistenceFail ):
                {
                    return _s( L"TRANSACTION OK " ) + TChar::pipe( ) + _s( L"BASE DE DONN" ) + TChar::eacute( ) + _s( L"ES ABIM" ) + TChar::eacute( ) + _s( L"E" ) ;
                }
                case( eEasycashPPP_eDisplayResponseInformation ):
                {
                    return _s( L"Solde courant: " );
                }
                case( eEasycashPPP_eDisplayNoResponseInformation ):
                {
                    return _s( L"INFORMATIONS MANQUANTES" ); 
                }
                case( eEasycashPPP_eEndOfDayTitle ):
                {
                    return _s( L"D" ) + TChar::eacute( ) + _s( L"compte de caisse" );
                }
                case( eEasycashPPP_eEndOfDayTime ):
                {
                    return _s( L"Date" ) + TChar::slash( ) + _s( L"heure" );
                }
                case( eEasycashPPP_eEndOfDayPayment ):
                case( eEasycashPPP_eTransactionPayment ):
                {
                    return _s( L"Paiement" );
                }
                case( eEasycashPPP_eEndOfDayVoidPayment ):
                case( eEasycashPPP_eTransactionVoidPayment ):
                {
                    return _s( L"Annulation paiement" );
                }
                case( eEasycashPPP_eEndOfDayCredit ):
                case( eEasycashPPP_eTransactionCredit ):
                {
                    if ( ( true == getpConfigDriverInputOptions()->isBehaviour( ePPIIBehaviour_EasyCashLoyalty ) ) && ( true == getpConfigDriverInputOptions()->get< ePPIIOption_EasyCashLoyalty_AdditionalParametersLuxembourgFlag >() ) )
                    {
                        return _s( L"Cr" ) + TChar::eacute( ) + _s( L"dit" );
                    }
                    else
                    {
                        return _s( L"Chargement" );
                    }
                }
                case( eEasycashPPP_eEndOfDayVoidCredit ):
                case( eEasycashPPP_eTransactionVoidCredit ):
                {
                    if ( ( true == getpConfigDriverInputOptions()->isBehaviour( ePPIIBehaviour_EasyCashLoyalty ) ) && ( true == getpConfigDriverInputOptions()->get< ePPIIOption_EasyCashLoyalty_AdditionalParametersLuxembourgFlag >() ) )
                    {
                        return _s( L"Annulation Cr" ) + TChar::eacute( ) + _s( L"dit" );
                    }
                    else
                    {
                        return _s( L"Annulation Chargement" );
                    }
                }
                case( eEasycashPPP_eEndOfDayTotal ):
                {
                    return _s( L"Total" );
                }
                case( eEasycashPPP_eTransactionBalance ):
                {
                    return _s( L"Interrogation du solde" );
                }
                case( eEasycashPPP_eTransactionExpiration ):
                {
                    return _s( L"Date de p" ) + TChar::eacute( ) + _s( L"remption" );
                }
                case( eEasycashPPP_eTransactionTotal ):
                {
                    return _s( L"Total-EFT EUR" );
                }
                case( eEasycashPPP_eTransactionBalanceAmount ):
                {
                    return _s( L"Solde courant EUR" );
                } 
                default:
                {
                    LogError( _s( L"[_getText] invalid French text code" ) );

                    THROW( XPCLITextNotSetException, ErrorCategory );
                }
            }
            break;
        }

        //ds none
        default:
        {
            LogError( _s( L"[_getText] invalid language" ) );

            THROW( XPCLITextNotSetException, ErrorCategory );
        }
    }
}

const int CPCLIImplementationEasycashPPP::_receiveTransaction( const boost::shared_ptr< CPCLIMessage > p_pInOutMessage )
{
    //ds escape if communication not available
    if( NULL == comm )
    {
        LogError( _s( L"[_receiveTransaction] communication object not available." ) );

        return eErrorCode_COMMUNICATION_NO_EFT;
    }

    //ds default ok (already instanciated here to be able to return this value at the end)
    int iReturnCode = eErrorCode_OK;

    //ds local counter for receive cycles
    unsigned int uCycleCounter = 0;

    //ds receive string holder
    std::string strReceiveMsg;

    //ds start timeout timer 1
    CLASSIC_CONTEXT_WIDE_SINGLETON->getpTimer( 1 )->run( m_uReceiveTimeOut );

    //ds while not timed out
    while ( false == CLASSIC_CONTEXT_WIDE_SINGLETON->getpTimer( 1 )->isElapsed( ) && false == m_bIsTransactionAlreadyReceived )
    {
        //ds increment for each cycle
        ++uCycleCounter;

		LogInfo( TString::sprintf( _s( L"[_receiveTransaction] receive cycle: %u - time elapsed: %f Seconds." )) % uCycleCounter % static_cast< double >( ( CLASSIC_CONTEXT_WIDE_SINGLETON->getpTimer(0)->getuElapsedMilliseconds( ) / 1000.0 ) ) );

		//ds start reading port
        if( comm->receive( strReceiveMsg ) )
        {
            //ds fill out in_outMsg with received and parsed data (this value will contain the response code of the host)
            iReturnCode = _fillInOutMessage( strReceiveMsg, p_pInOutMessage );

            //ds only produce tickets if okay
            if( eErrorCode_OK == iReturnCode && false == p_pInOutMessage->eftError )
            {
                //ds if receipt is set
                if( "" != p_pInOutMessage->receiptText )
                {
                    //ds print according receipt
                    p_pInOutMessage->receiptText = CPCLIStringUtilities::justifyTicket( p_pInOutMessage->receiptText, p_pInOutMessage->receiptFormat );
                    switch( p_pInOutMessage->eftMethod )
                    {
                        case(  EFTOPEN ):
                        {
                            CPCLIFileUtilities::writeFileEntry( p_pInOutMessage->receiptText, p_pInOutMessage->openPrintFile );
                            break;
                        }
                        case( EFTCLOSE ):
                        {
                            CPCLIFileUtilities::writeFileEntry( p_pInOutMessage->receiptText, p_pInOutMessage->closePrintFile );
                            break;
                        }
                        case( EFTTRX ):
                        {
                            CPCLIFileUtilities::writeFileEntry( p_pInOutMessage->receiptText, p_pInOutMessage->trxPrintFile );
                            break;
                        }
                        case( EFTINIT ):
                        {
                            CPCLIFileUtilities::writeFileEntry( p_pInOutMessage->receiptText, p_pInOutMessage->iniPrintFile );
                            break;
                        }
                    }
                }

                //ds if efttrx and not already set
                if ( p_pInOutMessage->eftMethod == EFTTRX && false == m_bIsTransactionAlreadyReceived )
                {
                    try
                    {
                        //ds build receipt
                        this->buildClientReceipt( p_pInOutMessage, "" );

                        //gv write merchant receipt for Luxemburg
                        if ( ePPIIReceiptSignature_ClientSignature == p_pInOutMessage->receiptSignature )
                        {
                            this->buildMerchantReceipt( p_pInOutMessage );
                        }
                    }
                    catch( const XPCLINULLPointerException& /*ex*/ )
                    {
                        //ds log error but do not abort transaction (special case)
                        LogError( _s( L"[_receiveTransaction] could not build receipt." ) );
                    }
                    
                    //ds add to table
                    if ( TRXTYPE_VOID_GOODS_PAYM == p_pInOutMessage->trxType ||
                         TRXTYPE_CREDIT          == p_pInOutMessage->trxType )
                    {
                        ccTable->setEntry( p_pInOutMessage->cardType, p_pInOutMessage->cardName, p_pInOutMessage->currency, -( p_pInOutMessage->getAmount( ) ) );    
                    }
                    else
                    {
                        ccTable->setEntry( p_pInOutMessage->cardType, p_pInOutMessage->cardName, p_pInOutMessage->currency, p_pInOutMessage->getAmount( ) );
                    }

                    //ds set boolean to done
                    m_bIsTransactionAlreadyReceived = true;
                }
            }
            break;
        }

        //ds read timeout
        osably::COSABLYOsApi::sleepMilliseconds( 10 );
    }

    //ds if timed out
    if ( CLASSIC_CONTEXT_WIDE_SINGLETON->getpTimer( 1 )->isElapsed( ) )
    {
		LogWarning( TString::sprintf( _s( L"[_receiveTransaction] receive timeout: %u Milliseconds reached." ) ) % m_uReceiveTimeOut );
        p_pInOutMessage->setDisplayCode( eErrorCode_EFT_TIMEOUT );

        //ds timeout reached
        return eErrorCode_EFT_TIMEOUT;
    }

    //ds delete display file
    handleDisplayText( ePCLIDisplayTextOperation_RemoveFile );

    //ds stop timer
    CLASSIC_CONTEXT_WIDE_SINGLETON->getpTimer( 1 )->reset( );

    //gv set the default display text in case it is empty so far
    if( true == p_pInOutMessage->displayText.empty( ) )
    {
        p_pInOutMessage->setDisplayCode( iReturnCode );
        p_pInOutMessage->setDisplayTxt( );
    }

    return iReturnCode;
}

const int CPCLIImplementationEasycashPPP::_receiveEndOfDay( const boost::shared_ptr< CPCLIMessage > p_pInOutMessage )
{
    LogInfo( _s( L"[_receiveEndOfDay] building ticket." )   );

    try
    {
        //ds build End of Day ticket
        _buildEndOfDayReceipt( p_pInOutMessage );
    }
    catch( const XPCLIPersistenceException& /*ex*/ )
    {
        LogError( _s( L"[_receiveEndOfDay] could not create end of day ticket due a persistence error." ) );

        //ds persistence error
        return eErrorCode_NOT_OK;    
    }
    catch( const XPCLINULLPointerException& /*ex*/ )
    {
        LogError( _s( L"[_receiveEndOfDay] could not create end of day ticket due an invalid pointer." ) );

        //ds null pointer exception
        return eErrorCode_NOT_OK;
    }

    //ds went fine if still here
    return eErrorCode_OK;
}

void CPCLIImplementationEasycashPPP::_buildEndOfDayReceipt( const boost::shared_ptr< CPCLIMessage > p_pInOutMessage ) const
{
    //ds escape if ticket not available
    if( NULL == ticket )
    {
        LogError( _s( L"[_buildEndOfDayReceipt] ticket poiner is empty." ) );

        THROW( XPCLINULLPointerException, ErrorCategory );
    }

    //ds clean the ticket (only one instance for the whole implementation)
    ticket->setTicket( "" );
    //gv overload
    ticket->setTicket( _s( L"" ) );

    //ds line instance to write format blocks
    TString strLine;

    //ds create header
    ticket->appendLine( _s( L"easycash Loyalty Solutions" ) );

    //ds set title
    ticket->appendLine( _getText( eEasycashPPP_eEndOfDayTitle, p_pInOutMessage->geteLanguageCode() ) );
    ticket->appendLine( _s( L" " ), CPCLITicket::LEFT );

    //ds get and set IDs
    strLine = _s( L"Branch-ID: " ) + CPCLITicket::BTS + m_strBranchID;
    ticket->appendLine( strLine, CPCLITicket::BLOCK );
    strLine = _s( L"Terminal-ID: " ) + CPCLITicket::BTS + m_strTerminalID;
    ticket->appendLine( strLine, CPCLITicket::BLOCK );

    //ds create a time line
    strLine = _getText( eEasycashPPP_eEndOfDayTime, p_pInOutMessage->geteLanguageCode() ) + _s( L":" ) + CPCLITicket::BTS + ticket->getDateDDMMJJJJ( _s( L"." ) ) + _s( L"/" ) + ticket->getTimeHHMMSS( _s( L":" ) );
    ticket->appendLine( strLine, CPCLITicket::BLOCK );

    //ds create a separator line
    strLine = ticket->getSequence( _s( L"-" ), p_pInOutMessage->receiptFormat );
    ticket->appendLine( strLine, CPCLITicket::LEFT );

    //ds get all end of day data instances from db
    CPCLIPersistenceEasycashPPPEndOfDay::TPtrVector vecRetrieved( CPCLIPersistenceEasycashPPPEndOfDay::persistentRetrieve( ) );

    //ds if not empty
    if( 0 < vecRetrieved.size( ) )
    {
        //ds total values
        unsigned int uNumberTotal = 0;
        double dAmountTotal       = 0;

        //ds loop through the vector and build the text for each card name
        for ( CPCLIPersistenceEasycashPPPEndOfDay::TPtrVector::const_iterator cIter = vecRetrieved.begin( ); cIter != vecRetrieved.end( ); ++cIter )
        {   
            //ds check if flag is correct
            if( true == ( *cIter )->isPersistentStored( ) )
            {
                //ds get the card name
                TString strCardName ( ( *cIter )->getCardName( ) );

                //ds replace all the underlines in the card name with a space
                for( unsigned int i = 0; i < strCardName.length( ); ++i )
                {
                    if( TChar::underline( ) == strCardName[i] )
                    {
                        //ds replace it
                        strCardName.replace( i, TChar::space( ) );
                    }
                }
            
                //ds plot cardname
                ticket->appendLine( strCardName.to_native_string( ), CPCLITicket::LEFT );

                //ds get all counters
                const unsigned int uPaymentCounter     = ( *cIter )->getPaymentCounter( );
                const unsigned int uVoidPaymentCounter = ( *cIter )->getVoidPaymentCounter( );
                const unsigned int uCreditCounter      = ( *cIter )->getCreditCounter( );
                const unsigned int uVoidCreditCounter  = ( *cIter )->getVoidCreditCounter( );

                //ds get all amounts
                const double dPaymentAmount     = ( *cIter )->getPaymentAmount( );
                const double dVoidPaymentAmount = ( *cIter )->getVoidPaymentAmount( );
                const double dCreditAmount      = ( *cIter )->getCreditAmount( );
                const double dVoidCreditAmount  = ( *cIter )->getVoidCreditAmount( );

                //ds release instance
                ( *cIter )->persistentRemove( );

                //ds calculate total values
                uNumberTotal += uPaymentCounter + uVoidPaymentCounter + uCreditCounter + uVoidCreditCounter;
                dAmountTotal += dPaymentAmount - dVoidPaymentAmount - dCreditAmount + dVoidCreditAmount;

                //ds separate digits (for correct 1.23 display)
                const unsigned int uFirstAmountOfGOODS_PAYM       = static_cast< int >( dPaymentAmount / 100 );
                const unsigned int uSecondAmountOfGOODS_PAYM      = static_cast< int >( dPaymentAmount - ( uFirstAmountOfGOODS_PAYM * 100 ) );
                const unsigned int uFirstAmountOfVOID_GOODS_PAYM  = static_cast< int >( dVoidPaymentAmount / 100 );
                const unsigned int uSecondAmountOfVOID_GOODS_PAYM = static_cast< int >( dVoidPaymentAmount - ( uFirstAmountOfVOID_GOODS_PAYM * 100 ) );
                const unsigned int uFirstAmountOfCREDIT           = static_cast< int >( dCreditAmount / 100 );
                const unsigned int uSecondAmountOfCREDIT          = static_cast< int >( dCreditAmount - ( uFirstAmountOfCREDIT * 100 ) );
                const unsigned int uFirstAmountOfVOID_CREDIT      = static_cast< int >( dVoidCreditAmount / 100 );
                const unsigned int uSecondAmountOfVOID_CREDIT     = static_cast< int >( dVoidCreditAmount - ( uFirstAmountOfVOID_CREDIT * 100 ) );

                //ds Goods Payment
                TString strTLine( TString::sprintf( _s( L"%4u EUR %8u.%02u" ) ) % uPaymentCounter % uFirstAmountOfGOODS_PAYM % uSecondAmountOfGOODS_PAYM );
                strLine          = _getText( eEasycashPPP_eEndOfDayPayment, p_pInOutMessage->geteLanguageCode() ) + TChar::colon( ) + CPCLITicket::BTS + strTLine;
                ticket->appendLine( strLine, CPCLITicket::BLOCK );

                //ds Void Goods Payment
                strTLine = TString::sprintf( _s( L"%4u EUR %8u.%02u" ) ) % uVoidPaymentCounter % uFirstAmountOfVOID_GOODS_PAYM % uSecondAmountOfVOID_GOODS_PAYM;
                strLine  = _getText( eEasycashPPP_eEndOfDayVoidPayment, p_pInOutMessage->geteLanguageCode() ) + TChar::colon( ) + CPCLITicket::BTS + strTLine ;
                ticket->appendLine( strLine, CPCLITicket::BLOCK );

                //ds Credit
                strTLine = TString::sprintf( _s( L"%4u EUR %8u.%02u" ) ) % uCreditCounter % uFirstAmountOfCREDIT % uSecondAmountOfCREDIT;
                strLine  = _getText( eEasycashPPP_eEndOfDayCredit, p_pInOutMessage->geteLanguageCode() ) + TChar::colon( ) + CPCLITicket::BTS + strTLine;
                ticket->appendLine( strLine, CPCLITicket::BLOCK );

                //ds Void Credit
                strTLine = TString::sprintf( _s( L"%4u EUR %8u.%02u" ) ) % uVoidCreditCounter % uFirstAmountOfVOID_CREDIT % uSecondAmountOfVOID_CREDIT;
                strLine  = _getText( eEasycashPPP_eEndOfDayVoidCredit, p_pInOutMessage->geteLanguageCode() ) + TChar::colon( ) + CPCLITicket::BTS  + strTLine ;
                ticket->appendLine( strLine, CPCLITicket::BLOCK );
            }
        }

        //ds total values for calculation
        const int iFirstAmountTotal     = static_cast< int >( dAmountTotal / 100 );
        unsigned int uSecondAmountTotal = 0;

        //ds if positive
        if( 0 < iFirstAmountTotal )
        {
            uSecondAmountTotal = static_cast< int >( dAmountTotal - ( iFirstAmountTotal * 100 ) );
        }
        else
        {
            uSecondAmountTotal = static_cast< int >( ( iFirstAmountTotal * 100 ) - dAmountTotal );
        }

        //ds total line string
        const TString strTLine( TString::sprintf( _s( L"%4u EUR %8i.%02u" ) ) % uNumberTotal % iFirstAmountTotal % uSecondAmountTotal );
        strLine = _getText( eEasycashPPP_eEndOfDayTotal, p_pInOutMessage->geteLanguageCode() ) + TChar::colon( ) + CPCLITicket::BTS + strTLine;
        ticket->appendLine( strLine, CPCLITicket::BLOCK );
    }

    //ds no data set -> write an empty total line
    else
    {
        //ds spacing
        ticket->appendLine( " ", CPCLITicket::LEFT );

        //ds total line string
        const TString strTLine = TString::sprintf( _s( L"%4u EUR %8u.%02u" ) ) % 0 % 0 % 0;
        strLine = _getText( eEasycashPPP_eEndOfDayTotal, p_pInOutMessage->geteLanguageCode() ) + TChar::colon( ) + CPCLITicket::BTS + strTLine;
        ticket->appendLine( strLine, CPCLITicket::BLOCK );
    }

    //ds there cannot be any end of day persistence instances now 
    CPCLIPersistenceEasycashPPPEndOfDay::TPtrVector vecRetrievedCheck( CPCLIPersistenceEasycashPPPEndOfDay::persistentRetrieve( ) );

    //ds size must be zero
    if( 0 != vecRetrievedCheck.size( ) )
    {
        LogError( _s( L"[_buildEndOfDayReceipt] persistence remove not worked." ) );

        THROW( XPCLIPersistenceException, ErrorCategory );
    }
    ticket->appendLine( _s( L" " ), CPCLITicket::LEFT );

    //gv convert the ticket to std::string and justify it 
    p_pInOutMessage->receiptText = ( ticket->getTstrTicket( ) ).to_native_string( );
    p_pInOutMessage->receiptText = CPCLIStringUtilities::justifyTicket( p_pInOutMessage->receiptText, p_pInOutMessage->receiptFormat );
    ticket->setTicket( p_pInOutMessage->receiptText );


    //ds push the file in the files to print chain    
    ticket->flushToFile( p_pInOutMessage->eodPrintFile );

    //ds log the whole ticket
    LogInfo( _s( L"\n\n---------------------------EOD TICKET---------------------------\n" ) );
    LogInfo( TString::from_native_string( CPCLIFileUtilities::readFile( CPCLIFileUtilities::getPathFileName( ePCLIPathDescriptor_Print, p_pInOutMessage->eodPrintFile ) ) ) );
    LogInfo( _s( L"----------------------------------------------------------------\n\n" ) ); 
}


const int CPCLIImplementationEasycashPPP::_fillInOutMessage( const TByteStream& p_bsReceivedMessage, boost::shared_ptr< CPCLIMessage > p_pInOutMessage ) 
{
    //ds tree instance
    TXmlTree cTree;

    //ds try to build an xml tree out of received bytestream
    try
    {
        //ds build tree from bytestream
        cTree.fromByteStream( p_bsReceivedMessage, eCOREDTXmlTrimMode_Trim, eCOREDTXmlErrorChecking_StopOnEverything );

        //ds new line for better readable
        LogInfo( _s( L"\n\n----------------------------RESPONSE----------------------------\n" ) );

        //ds log it
        LogInfo( TString::from_native_string( cTree.toByteStream( eCOREDTXmlCodepage_iso8859_1 ).toStdString( ) ) );

        //ds new line for better readable
        LogInfo( _s( L"----------------------------------------------------------------\n\n" ) );
    }
    catch( const baselayer::XCOREDTXmlStructureReadingException& /*ex*/ )
    {
        //ds report error
        p_pInOutMessage->eftError = true;

        LogError( _s( L"[_fillInOutMessage] could not build XML tree of received response." ) );

        //ds set error text
        p_pInOutMessage->displayText = _getText( eEasycashPPP_eDisplayInvalidResponse, p_pInOutMessage->geteLanguageCode() ).to_native_string( );
        p_pInOutMessage->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );

        //ds always set to ok to escape receive loop
        return eErrorCode_OK;
    }

    //ds parse xml and store local return code to mask for return value (iReturnCode gets always set!)
    const int iReturnCode = _parseXMLTree( cTree, p_pInOutMessage );

    //ds if invalid return code
    if( false == m_bIsResponseValid || false == _isValidResponseCode( iReturnCode ) )
    {
		LogWarning( TString::sprintf( _s( L"[_fillInOutMessage] ReturnCode: %i invalid." ) ) % iReturnCode );
    }
    else
    {
		LogInfo( TString::sprintf( _s( L"[_fillInOutMessage] ReturnCode: %i" ) ) % iReturnCode );
    }

    //ds GV 04.10.2008: ResultCode is needed for OPI ReturnCode (WNX/DBL) //ds also if invalid?
    if( EFTTRX == p_pInOutMessage->eftMethod )
    { 
        const TString strResult( TString::from_numerical< int >( iReturnCode ) );  
        if( true == GLOBAL_CONFIG->get< ePGSIConfigParameter_Operation_DeprecatedXmlOutput >() )

        {
            p_pInOutMessage->xmlAdditionalParameters = CPCLIXmlUtilities::addTag2PepXmlParams( p_pInOutMessage->xmlAdditionalParameters, "ResultCode", strResult.to_native_string( ) );    
        }
        else
        {
            p_pInOutMessage->xmlAdditionalParameters = CPCLIXmlUtilities::addTag2PepXmlParams( p_pInOutMessage->xmlAdditionalParameters, "ResultCodeString", strResult.to_native_string( ) );    
        }
    }
    
    //ds if return code is ok (00) and no errors flagged (100% positive response)
    if( 0 == iReturnCode && false == p_pInOutMessage->eftError && true == m_bIsResponseValid )
    {
        try
        {
            //ds increment current sequence number
            m_pPersistenceTransaction->incrementSequenceNumber( );

            //ds update end of day parameters (zero amount and balance ignored)
            if( TRXTYPE_BALANCE != p_pInOutMessage->trxType && 0 != p_pInOutMessage->getAmount( ) )
            {
                //ds update persistence
                _updatePersistenceEndOfDay( p_pInOutMessage );

                //ds always set to ok to escape receive loop
                return eErrorCode_OK;
            }
            else
            {
                //ds always set to ok to escape receive loop
                return eErrorCode_OK;
            }
        }
        catch( const XPCLIPersistenceException& /*ex*/ )
        {
            LogError( _s( L"[_fillInOutMessage] could not update persistence." ) );

            //ds check if the problem is really on our side
            if( true == m_bAreAllResponseTagsSet )
            {
                //ds display message (error but transaction was okay)
                p_pInOutMessage->displayText = _getText( eEasycashPPP_eDisplayPersistenceFail, p_pInOutMessage->geteLanguageCode() ).to_native_string( );
                p_pInOutMessage->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
            }

            //ds always set to ok to escape receive loop
            return eErrorCode_OK;
        }
        catch( const XPCLIEmptyStringException& /*ex*/ )
        {
            LogError( _s( L"[_fillInOutMessage] card name not set." ) );

            //ds check if the problem is really on our side
            if( true == m_bAreAllResponseTagsSet )
            {
                //ds display message (error but transaction was okay)
                p_pInOutMessage->displayText = _getText( eEasycashPPP_eDisplayPersistenceFail, p_pInOutMessage->geteLanguageCode() ).to_native_string( );
                p_pInOutMessage->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );
            }

            //ds always set to ok to escape receive loop
            return eErrorCode_OK;
        }
    }

    //ds return code is not okay
    else
    {
        //ds report error
        p_pInOutMessage->eftError = true;

        LogError( _s( L"[_fillInOutMessage] DisplayText: " ) + TString::from_native_string( p_pInOutMessage->displayText ) );

        //ds set error text
        p_pInOutMessage->displayText = ( TString::from_native_string( p_pInOutMessage->displayText ) + _s( L" (" ) + TString::from_numerical< int >( iReturnCode ) + _c( ')' ) ).to_native_string( );
        p_pInOutMessage->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );

        //ds set to ok to escape receive loop
        return eErrorCode_OK;
    }
}

const int CPCLIImplementationEasycashPPP::_parseXMLTree( const TXmlTree& p_cTree, boost::shared_ptr< CPCLIMessage > p_pInOutMessage )
{
    //ds local return code, needed for maximum tolerant parsing (-1 does not mark a magic number, unparsed return codes are registered with a boolean)
    int iReturnCode = -1;

    //ds local tagcounter
    unsigned int uTagCounter = 0;

    //ds current number of essential tags we should receive
    static const unsigned int uNumberOfEssentialTags = 10;


    //ds PARSE XML TREE (in defined order by costumers reply scheme)
    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/MessageType" ) ) ) 
    {
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/MessageType" ) );

        //ds check if attribute is available
        if( true == ( *cNode ).getAttribute( _s( L"ID" ) ).first )
        {
            //ds get ID
            const TString strMessageTypeID( ( *cNode ).getAttribute( _s( L"ID" ) ).second );
            
            //ds invalid
            if( _s( L"210" ) != strMessageTypeID && _s( L"410" ) != strMessageTypeID )
            {
                LogWarning( _s( L"[_parseXMLTree] MessageType: " ) + strMessageTypeID + _s( L" unknown." ) );
            }
            
            //ds valid
            else
            {
                LogInfo( _s( L"[_parseXMLTree] MessageType: " ) + strMessageTypeID );
            } 
        }     
    }
    else
    {
        LogWarning( _s( L"[_parseXMLTree] missing tag: <MessageType>." ) );
    }

    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/CardNumber" ) ) ) 
    {
        //ds get card number
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/CardNumber" ) );
        const TString strCardNumber( ( *cNode ).getstrValue( ) );

        //ds check if its in pepper display range and valid
        if( 31 >= strCardNumber.length( ) && true == _isANumber( strCardNumber ) ) 
        {
            //ds set it
            p_pInOutMessage->cardNbr = strCardNumber.to_native_string( );

            //ds set card type and card name
            p_pInOutMessage->cardType = this->getCardType( p_pInOutMessage, p_pInOutMessage->cardNbr );

            //gv for Proficard and Projektwelt ask for a signature (and create a merchant receipt)
            if( ( 420 == p_pInOutMessage->cardType ) || ( 421 == p_pInOutMessage->cardType ) )
            {
                p_pInOutMessage->receiptSignature = ePPIIReceiptSignature_ClientSignature;
            }
            else 
            {
                //gv neither PIN nor signature required
                p_pInOutMessage->receiptSignature = ePPIIReceiptSignature_Nothing;
            }
            LogInfo( _s( L"[_parseXMLTree] p_pInOutMessage->receiptSignature = " ) + TString::sprintf( _s( L"%u" ) ) % p_pInOutMessage->receiptSignature ) ;

            //ds log it
            LogInfo( _s( L"[_parseXMLTree] p_pInOutMessage->cardNbr = " ) + TString::from_native_string( p_pInOutMessage->cardNbr ) );
        }
        else
        {
            //ds log it as error (card number is essential for persistence)
            LogError( _s( L"[_parseXMLTree] CardNumber: " ) + strCardNumber + _s( L" invalid." ) );
        }    
    }
    else
    {
        LogError( _s( L"[_parseXMLTree] missing tag: <CardNumber>." ) );
    }

    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/RequestType" ) ) ) 
    {
        //ds just register this tag
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/RequestType" ) );
        const TString strRequestType( ( *cNode ).getstrValue( ) );
        
        //ds invalid
        if( _s( L"00" ) != strRequestType && _s( L"20" ) != strRequestType )
        {
            LogWarning( _s( L"[_parseXMLTree] RequestType: " ) + strRequestType + _s( L" unknown." ) );
        }

        //ds valid
        else
        {
            LogInfo( _s( L"[_parseXMLTree] RequestType: " ) + strRequestType   );
        }     
    }
    else
    {
        LogWarning( _s( L"[_parseXMLTree] missing tag: <RequestType>." ) );
    }

    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/Amount" ) ) ) 
    {
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/Amount" ) );
        const TString strAmount( ( *cNode ).getstrValue( ) );

        try
        {
            //ds get the amount out of the response
            double dAmount = strAmount.to_numerical< double >( );

            if( 999999999999.0 >= dAmount )
            {
                //ds increment tag counter
                ++uTagCounter;

                //ds only set if no information loss is caused by server response
                if( 0 != dAmount )
                {
                    //ds set it if in defined range
                    p_pInOutMessage->setAmount( dAmount );

                    //ds and log it
					LogInfo( TString::sprintf( _s( L"[_parseXMLTree] p_pInOutMessage->amount = %f" ) ) % ( p_pInOutMessage->getAmount() ) );
                }
                else
                {
                    //ds dont set it but log information
                    LogInfo( _s( L"[_parseXMLTree] Amount: " ) + strAmount );    
                }
            }
            else
            {
                //ds dont set it but log information
                LogWarning( _s( L"[_parseXMLTree] Amount: " ) + strAmount + _s( L" invalid." ) );
            }
        }
        catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
        {
                //ds just log it (wayne if format error at runtime)
                LogWarning( _s( L"[_parseXMLTree] Amount: " ) + strAmount + _s( L" could not be converted to double." ) );
        }        
    }
    else
    {
        LogWarning( _s( L"[_parseXMLTree] missing tag: <Amount>." ) );
    }

    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/TraceNumber" ) ) ) 
    {
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/TraceNumber" ) );
        const TString strTraceNumber( ( *cNode ).getstrValue( ) );

        try
        {
            //ds check if number is valid
            if( 999999 > strTraceNumber.to_numerical< unsigned int >( ) )
            {
                p_pInOutMessage->trxRefNbrOut = strTraceNumber.to_native_string( );
                p_pInOutMessage->traceNr      = strTraceNumber.to_native_string( );

                //ds log it
                LogInfo( _s( L"[_parseXMLTree] p_pInOutMessage->traceNr = " ) + TString::from_native_string( p_pInOutMessage->traceNr )   );

                //ds increment tag counter
                ++uTagCounter;
        
                if( true == GLOBAL_CONFIG->get< ePGSIConfigParameter_Operation_DeprecatedXmlOutput >() )
                {
                    p_pInOutMessage->xmlAdditionalParameters = CPCLIXmlUtilities::addTag2PepXmlParams( p_pInOutMessage->xmlAdditionalParameters, "TraceNumber", p_pInOutMessage->traceNr );
                }
                else
                {
                        p_pInOutMessage->xmlAdditionalParameters = CPCLIXmlUtilities::addTag2PepXmlParams(p_pInOutMessage->xmlAdditionalParameters, "TraceNumberString", p_pInOutMessage->traceNr);
                }
            }
            else
            {
                //ds dont set it but log information
                LogWarning( _s( L"[_parseXMLTree] TraceNumber: " ) + strTraceNumber + _s( L" invalid." ) );
            }
        }
        catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
        {
                //ds just log it (wayne if format error at runtime)
                LogWarning( _s( L"[_parseXMLTree] TraceNumber: " ) + strTraceNumber + _s( L" could not be converted to unsigned integer." ) );
        }        
    }
    else
    {
        LogWarning( _s( L"[_parseXMLTree] missing tag: <TraceNumber>." ) );
    }

    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/Time" ) ) ) 
    {
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/Time" ) );
        const TString strTime( ( *cNode ).getstrValue( ) );
        
        //ds check if valid in terms of length
        if( 6 == strTime.length( ) )
        { 
            try
            {
                //ds if it fits to normal time structure
                if( 25 > strTime.substr(  0, 2 ).to_numerical< unsigned int >( ) && 
                    60 > strTime.substr(  2, 2 ).to_numerical< unsigned int >( ) && 
                    60 > strTime.substr(  4, 2 ).to_numerical< unsigned int >( ) )
                {
                    //ds set it
                    p_pInOutMessage->trxTime = strTime.to_native_string( );

                    //ds log information
                    LogInfo( _s( L"[_parseXMLTree] p_pInOutMessage->trxTime = " ) + TString::from_native_string( p_pInOutMessage->trxTime ) );

                    //ds increment tag counter
                    ++uTagCounter;
                }
                else
                {
                    //ds dont set it but log information
                    LogWarning( _s( L"[_parseXMLTree] Time: " ) + strTime + _s( L"invalid" ) );
                }
            }
            catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
            {
                //ds just log it (wayne if format error at runtime)
                LogWarning( _s( L"[_parseXMLTree] Time: " ) + strTime + _s( L" could not be converted to unsigned integers." ) );
            }
        }
        else
        {
            //ds dont set it but log information
            LogWarning( _s( L"[_parseXMLTree] Time: " ) + strTime + _s( L" must be 6 digits long." ) );   
        }       
    }
    else
    {
        LogWarning( _s( L"[_parseXMLTree] missing tag: <Time>." ) );
    }

    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/Date" ) ) ) 
    {
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/Date" ) );
        const TString strDate( ( *cNode ).getstrValue( ) );
        
        //ds check if valid in terms of length
        if( 4 == strDate.length( ) )
        { 
            try
            {
                //ds if it fits to normal time structure
                if( 13 > strDate.substr(  0, 2 ).to_numerical< unsigned int >( ) && 32 > strDate.substr(  2, 2 ).to_numerical< unsigned int >( ) )
                {
                    //ds get the single digit pairs for date (for swap)
                    const TString strMonth( strDate.substr( 0, 2 ) );
                    const TString strDay( strDate.substr( 2, 2 ) );

                    //ds get current year
                    TTime cCurrentTime    = TTime::getNowLocal( );
                    const TString strYear( TString::from_numerical< unsigned int >( cCurrentTime.getuYear( ) ) );

                    //ds set it
                    p_pInOutMessage->trxDate = ( strDay + strMonth + strYear ).to_native_string( );

                    //ds and log it
                    LogInfo( _s( L"[_parseXMLTree] p_pInOutMessage->trxDate = " ) + TString::from_native_string( p_pInOutMessage->trxDate ) );

                    //ds increment tag counter
                    ++uTagCounter;
                }
                else
                {
                    //ds dont set it but log information
                    LogWarning( _s( L"[_parseXMLTree] Date: " ) + strDate + _s( L" invalid." ) );
                }
            }
            catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
            {
                //ds just log it (wayne if format error at runtime)
                LogWarning( _s( L"[_parseXMLTree] Date: " ) + strDate + _s( L" could not be converted to unsigned integers." ) );
            }
        }
        else
        {
            //ds dont set it but log information
            LogWarning( _s( L"[_parseXMLTree] Date: " ) + strDate + _s( L" must be 4 digits long." ) );   
        }   
    }
    else
    {
        LogWarning( _s( L"[_parseXMLTree] missing tag: <Date>." ) );
    }

    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/ExpiryDate" ) ) ) 
    {
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/ExpiryDate" ) );
        const TString strExpiryDate( ( *cNode ).getstrValue( ) );

        //ds check if valid in terms of length
        if( 4 == strExpiryDate.length( ) )
        { 
            try
            {
                //ds if it fits to normal time structure
                if( 100 > strExpiryDate.substr(  0, 2 ).to_numerical< unsigned int >( ) && 13 > strExpiryDate.substr(  2, 2 ).to_numerical< unsigned int >( ) )
                {
                    //ds get the single digit pairs for expiry date
                    const TString strYear( strExpiryDate.substr( 0, 2 ) );
                    const TString strMonth( strExpiryDate.substr( 2, 2 ) );

                    //ds set the expiry date in pepper style
                    p_pInOutMessage->expDate = ( strMonth + strYear ).to_native_string( ); 

                    //ds loglog
                    LogInfo( _s( L"[_parseXMLTree] p_pInOutMessage->expDate = " ) + TString::from_native_string( p_pInOutMessage->expDate ) );

                    //ds increment tag counter
                    ++uTagCounter;
                }
                else
                {
                    //ds dont set it but log information
                    LogWarning( ( _s( L"[_parseXMLTree] ExpiryDate: " ) + strExpiryDate ) );
                }
            }
            catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
            {
                //ds just log it (wayne if format error at runtime)
                LogWarning( _s( L"[_parseXMLTree] ExpiryDate: " ) + strExpiryDate + _s( L" could not be converted to unsigned integers." ) );
            }
        }
        else
        {
            //ds dont set it but log information
            LogWarning( _s( L"[_parseXMLTree] ExpiryDate: " ) + strExpiryDate + _s( L" must be 4 digits long." ) );   
        }       
    }
    else
    {
        LogWarning( _s( L"[_parseXMLTree] missing tag: <ExpiryDate>." ) );
    }

    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/TransactionDate" ) ) ) 
    {
        //ds just log it
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/TransactionDate" ) );
        const TString strTransactionDate( ( *cNode ).getstrValue( ) );
        
        //ds if incorrect
        if( _s( L"0000" ) != strTransactionDate )
        {
            LogWarning( _s( L"[_parseXMLTree] TransactionDate: " ) + strTransactionDate + _s( L" unknown." ) );
        }
        else
        {
            LogInfo( _s( L"[_parseXMLTree] TransactionDate: " ) + strTransactionDate );
        }       
    }
    else
    {
        LogWarning( _s( L"[_parseXMLTree] missing tag: <TransactionDate>." ) );
    }

    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/EntryMode" ) ) ) 
    {
        //ds just log it
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/EntryMode" ) );
        const TString strEntryMode( ( *cNode ).getstrValue( ) );

        //ds invalid
        if( _s( L"01" ) != strEntryMode && _s( L"02" ) != strEntryMode && _s( L"03" ) != strEntryMode )
        {
            LogWarning( _s( L"[_parseXMLTree] EntryMode: " ) + strEntryMode + _s( L" unknown." ) );
        }
    
        //ds valid
        else
        {
            LogInfo( _s( L"[_parseXMLTree] EntryMode: " ) + strEntryMode );
        }      
    }
    else
    {
        LogWarning( _s( L"[_parseXMLTree] missing tag: <EntryMode>." ) );
    }

    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/Condition" ) ) ) 
    {
        //ds just log it
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/Condition" ) );
        const TString strCondition( ( *cNode ).getstrValue( ) );
        
        //ds if incorrect
        if( _s( L"00" ) != strCondition )
        {
            LogWarning( _s( L"[_parseXMLTree] Condition: " ) + strCondition + _s( L" unknown." ) );
        }
        else
        {
            LogInfo( _s( L"[_parseXMLTree] Condition: " ) + strCondition );
        }
    }
    else
    {
        LogWarning( _s( L"[_parseXMLTree] missing tag: <Condition>." ) );
    }

    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/Response-ID" ) ) ) 
    {
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/Response-ID" ) );
        const TString strResponse_ID( ( *cNode ).getstrValue( ) );

        try
        {
            //ds check if number is valid
            if( 99999999 > strResponse_ID.to_numerical< unsigned int >( ) )
            {
                p_pInOutMessage->authNbr = strResponse_ID.to_native_string( );
    
                //ds log it
                LogInfo( _s ( L"[_parseXMLTree] p_pInOutMessage->authNbr = " ) + TString::from_native_string( p_pInOutMessage->authNbr ) );
        
                //ds increment tag counter
                ++uTagCounter;
            }
            else
            {
                //ds dont set it but log information
                LogWarning( _s( L"[_parseXMLTree] Response-ID: " ) + strResponse_ID + _s( L" invalid." ) );
            }
        }
        catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
        {
                //ds just log it (wayne if format error at runtime)
                LogWarning( _s( L"[_parseXMLTree] Response-ID: " ) + strResponse_ID + _s( L" could not be converted to unsigned integer." ) );
        }   
    }
    else
    {
        LogWarning( _s( L"[_parseXMLTree] missing tag: <Response-ID>." ) );
    }

    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/ResponseCode" ) ) ) 
    {
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/ResponseCode" ) );
        const TString strResponseCode( ( *cNode ).getstrValue( ) );

        try
        {
            //ds set the return code
            iReturnCode = strResponseCode.to_numerical< int >( );

            //ds if extended output desired
            if( true == GLOBAL_CONFIG->get< ePGSIConfigParameter_Operation_DeprecatedXmlOutput >() )
            {
                p_pInOutMessage->xmlAdditionalParameters = CPCLIXmlUtilities::addTag2PepXmlParams( p_pInOutMessage->xmlAdditionalParameters, "ResultCode", strResponseCode.to_native_string( ) );   
            }
            else
            {
                p_pInOutMessage->xmlAdditionalParameters = CPCLIXmlUtilities::addTag2PepXmlParams( p_pInOutMessage->xmlAdditionalParameters, "ResultCodeString", strResponseCode.to_native_string( ) );   
            }

            LogInfo( _s( L"[_parseXMLTree] ResponseCode: " ) + strResponseCode );

            //ds increment tag counter
            ++uTagCounter;

            //ds set the boolean to true, else remains false
            m_bIsResponseValid = true;
        }
        catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
        {
            //ds dont set it and set log warning as string
            LogError( _s( L"[_parseXMLTree] ResponseCode: " ) + strResponseCode + _s( L" could not be converted to integer." ) );

            //ds report error
            p_pInOutMessage->eftError = true;
        }      
    }

    //ds if response code is not set mark error (server produces nonsens)
    else
    {
        //ds report error
        p_pInOutMessage->eftError = true;

        //ds log
        LogError( _s( L"[_parseXMLTree] missing tag: <ResponseCode>." ) );
    }

    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/Terminal-ID" ) ) ) 
    {
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/Terminal-ID" ) );
        const TString strTerminal_ID( ( *cNode ).getstrValue( ) );

        try
        {
            //ds check if number is valid
            if( 99999999 > strTerminal_ID.to_numerical< unsigned int >( ) )
            {
                p_pInOutMessage->terminalID = strTerminal_ID.to_native_string( );

                //ds log it
                LogInfo( _s( L"[_parseXMLTree] p_pInOutMessage->terminalID = " ) + TString::from_native_string( p_pInOutMessage->terminalID ) );

                //ds increment tag counter
                ++uTagCounter;
            }
            else
            {
                //ds dont set it but log information
                LogWarning( _s( L"[_parseXMLTree] Terminal-ID: " ) + strTerminal_ID + _s( L" invalid." ) );
            }
        }
        catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
        {
                //ds just log it (wayne if format error at runtime)
                LogWarning( _s( L"[_parseXMLTree] Terminal-ID: " ) + strTerminal_ID + _s( L" could not be converted to unsigned integer." ) );
        }      
    }
    else
    {
        LogWarning( _s( L"[_parseXMLTree] missing tag: <Terminal-ID>." ) );
    }

    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/Branch-ID" ) ) ) 
    {
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/Branch-ID" ) );
        const TString strBranch_ID( ( *cNode ).getstrValue( ) );

        //ds check if number is valid
        if( 16 > strBranch_ID.length( ) && true == _isANumber( strBranch_ID ) )
        {
            p_pInOutMessage->contractNbr = strBranch_ID.to_native_string( );

            //ds if extended output desired
            if( true == GLOBAL_CONFIG->get< ePGSIConfigParameter_Operation_DeprecatedXmlOutput >() )
            {
                p_pInOutMessage->xmlAdditionalParameters = CPCLIXmlUtilities::addTag2PepXmlParams( p_pInOutMessage->xmlAdditionalParameters, "Branch-ID", ( *cNode ).getstrValue( ).to_native_string( ) );   
            }
            else
            {
                p_pInOutMessage->xmlAdditionalParameters = CPCLIXmlUtilities::addTag2PepXmlParams( p_pInOutMessage->xmlAdditionalParameters, "BranchIdentificationNumber", ( *cNode ).getstrValue( ).to_native_string( ) );   
            }

            //ds log
            LogInfo( _s( L"[_parseXMLTree] p_pInOutMessage->contractNbr = " ) + TString::from_native_string( p_pInOutMessage->contractNbr ) );

            //ds increment tag counter
            ++uTagCounter;
        }
        else
        {
            //ds dont set it but log information
            LogWarning( _s( L"[_parseXMLTree] Branch-ID: " ) + strBranch_ID + _s( L" invalid." ) );
        }      
    }
    else
    {
        LogWarning( _s( L"[_parseXMLTree] missing tag: <Branch-ID>." ) );
    }

    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/Message" ) ) ) 
    {
        //ds get message tag information
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/Message" ) );
        const TString strMessage( ( *cNode ).getstrValue( ) );
        LogInfo( _s( L"[_parseXMLTree] Message: " ) + strMessage );

        //ds hand over response information to display text
        p_pInOutMessage->displayText = ( _getResponseInformation( strMessage, p_pInOutMessage ) ).to_native_string( );
        p_pInOutMessage->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );

        LogInfo( _s( L"[_parseXMLTree] p_pInOutMessage->displayText = " ) + TString::from_native_string( p_pInOutMessage->displayText ) );

        //ds if display text is not empty
        if( false == p_pInOutMessage->displayText.empty() )
        {
            //ds increment tag counter
            ++uTagCounter;

            //ds change amount for successful balance (host returns 0 in amount tag)
            if( TRXTYPE_BALANCE == p_pInOutMessage->trxType && 0 == iReturnCode )
            {
                try
                {
                    //ds set amount
                    p_pInOutMessage->setAmount( _getAmountFromString( TString::from_native_string( p_pInOutMessage->displayText ), _c( L',' ) ) );
                
                    //ds log it
					LogInfo( TString::sprintf( _s( L"[_parseXMLTree] p_pInOutMessage->amount = %f (BALANCE)" ) ) % ( p_pInOutMessage->getAmount() ) );    
                }
                catch( const XPCLIParsingFailedException& /*ex*/ )
                {
                    //ds just log and leave amount as it is
                    LogError( _s( L"[_parseXMLTree] parsing of <Message> tag not possible." ) );   
                }
                catch( const XPCLIStringConversionFailedException& /*ex*/ )
                {
                    //ds just log and leave amount as it is
                    LogError( _s( L"[_parseXMLTree] could not convert string to numerical." ) );
                }    
            }
        }
        else
        {
            //bk set the display text according to the return value
            p_pInOutMessage->setDisplayTxt();

            //ds log
            LogWarning( _s( L"[_parseXMLTree] <Message> tag empty." ) );
        }      
    }
    else
    {
        //ds inform about missing tags
        p_pInOutMessage->displayText = ( _getText( eEasycashPPP_eDisplayNoResponseInformation, p_pInOutMessage->geteLanguageCode() ) ).to_native_string( );
        p_pInOutMessage->setDisplayCode( DISPLAY_DONT_CHANGE_CODE );

        //ds log
        LogWarning( _s( L"[_parseXMLTree] missing tag: <Message>." ) );
    }

    if( p_cTree.isNodeByPathExisting(_s( L"/AutSys/Reply/CCTI" ) ) ) 
    {
        //ds just log it
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/CCTI" ) ); 
        const TString strCCTI( ( *cNode ).getstrValue( ) );
        
        //ds if incorrect
        if( _s( L"89" ) != strCCTI )
        {
            LogWarning( _s( L"[_parseXMLTree] CCTI: " ) + strCCTI + _s( L" unknown." ) );
        }
        else
        {
            LogInfo( _s( L"[_parseXMLTree] CCTI: " ) + strCCTI );
        }      
    }
    else
    {
        LogWarning( _s( L"[_parseXMLTree] missing tag: <CCTI>." ) );
    }

    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/Currency" ) ) ) 
    {
        //ds log it
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/Currency" ) );
        const TString strCurrency( ( *cNode ).getstrValue( ) );
        
        //ds if incorrect
        if( _s( L"978" ) != strCurrency )
        {
            LogWarning( _s( L"[_parseXMLTree] Currency: " ) + strCurrency + _s( L" unknown." ) );
        }
        else
        {
            LogInfo( _s( L"[_parseXMLTree] Currency: " ) + strCurrency );
        }
    }
    else
    {
        LogWarning( _s( L"[_parseXMLTree] missing tag: <Currency>." ) );
    }

    if( p_cTree.isNodeByPathExisting( _s( L"/AutSys/Reply/Sequence" ) ) ) 
    {
        //ds log it
        const TXmlTree::TNode& cNode = p_cTree.getNodeByPath( _s( L"/AutSys/Reply/Sequence" ) );
        const TString strSequence( ( *cNode ).getstrValue( ) );

        //ds valid
        if( 9 > strSequence.length( ) )
        {
            LogInfo( _s( L"[_parseXMLTree] Sequence: " ) + strSequence );
        }
        else
        {
            LogWarning( _s( L"[_parseXMLTree] Sequence: " ) + strSequence + _s( L" invalid." ) );
        }
    }
    else
    {
        LogWarning( _s( L"[_parseXMLTree] missing tag: <Sequence>." ) );
    }

	LogInfo( TString::sprintf( _s( L"[_parseXMLTree] essential tags: %u" ) ) % uTagCounter );

    //ds check number of tags and set bool
    if( uNumberOfEssentialTags == uTagCounter )
    {
        m_bAreAllResponseTagsSet = true;
    }
    else
    {
        m_bAreAllResponseTagsSet = false;
    }

    //ds return parsed return code
    return iReturnCode;
}

const TString CPCLIImplementationEasycashPPP::_getResponseInformation( const TString& p_strMessageTag, const boost::shared_ptr< CPCLIMessage > p_pInOutMessage ) const
{
    //ds return instance
    TString strResponseInformation( p_strMessageTag );

    //ds unicode string to find     //' '                          //'/ue'                              //' '
    const TString strStringToFind = TChar::space( ) + _c( L'g' ) + TChar::uuml( ) + _s( L"ltig bis" ) + TChar::space( );

    //ds find position to cut
    const unsigned int uEndCutPosition = p_strMessageTag.find( strStringToFind );

    //ds check if shorten is possible
    if( TString::npos( ) != uEndCutPosition )
    {
        //ds cut the string
        strResponseInformation = p_strMessageTag.substr( 0, uEndCutPosition );
    }

    //ds replace with correct language if not german
    if( ePPIILanguage_German != p_pInOutMessage->geteLanguageCode() )
    {
        //ds contruct unicode string to find              //.               //' '                                 //:               //' '
        const TString strStringToReplace( _s( L"Akt" ) + TChar::period( ) + TChar::space( ) + _s( L"Guthaben" ) + TChar::colon( ) + TChar::space( ) );

        //ds get start position
        const unsigned int uStartReplacePosition = strResponseInformation.find( strStringToReplace );

        //ds check if replacement is possible
        if( TString::npos( ) != uStartReplacePosition )
        {
            //ds replace the string
            strResponseInformation.replace( uStartReplacePosition, strStringToReplace.length( ), _getText( eEasycashPPP_eDisplayResponseInformation, p_pInOutMessage->geteLanguageCode() ) );
        }
    }

    //ds return the string
    return strResponseInformation;
}

const double CPCLIImplementationEasycashPPP::_getAmountFromString( TString p_strString, const TChar& p_chSeparator ) const
{
    //ds position holders
    unsigned int uCommaPosition = 0;
    unsigned int uStartPosition = 0;

    //ds digit holders
    TString strDigitsBeforeComma ( _s( L"INVALID" ) );
    TString strDigitsAfterComma  ( _s( L"INVALID" ) );

    //ds max amount for pepper       123456789012
    static const double dMaxAmount = 999999999999.0;

    //ds correct comma found bool (in case for multiple commas in the string)
    bool bFoundCorrectComma = false; 

    //ds search until correct comma is found or no more commas are in string (also includes forgotten comma)
    while( true != bFoundCorrectComma && TString::npos( ) != uCommaPosition )
    {
        //ds get current comma position (this breaks the loop if no more comma is found)
        uCommaPosition = p_strString.find( p_chSeparator );

        //ds escape if no comma found
        if( TString::npos( ) == uCommaPosition )
        {
            LogError( _s( L"[_getAmountFromString] string format invalid for amount parsing." ) );

            THROW( XPCLIParsingFailedException, ErrorCategory );
        }

        //ds get current start position -> loop from the comma position reverse through string and search for the last appearance (example: A1234567,89 -> loop from 7 to A)
        for( unsigned int i = uCommaPosition - 1; i >= 0; --i )
        {
            //ds if not a number
            if( false == _isANumber( p_strString[i] ) )
            {
                //ds set the start point
                uStartPosition = i;

                //ds break the loop
                break;    
            }
        }
    
        //ds read from calculated start position (length checked)
        strDigitsBeforeComma = p_strString.substr( uStartPosition + 1, uCommaPosition - uStartPosition -1 );
        
        //ds read the digits after comma if possible and check if not 3 digits are numbers after the comma (would be no amount)
        if( uCommaPosition + 3 <= p_strString.length( ) )
        { 
            //ds if string is longer than 3 digits after the comma
            if( uCommaPosition + 4 <= p_strString.length( ) )
            {
                //ds check if not 3 digits after the comma are numbers (would be no amount)
                if( false == _isANumber( p_strString[ uCommaPosition + 3] ) )
                {
                    //ds just these 2 digits after the comma are numbers, set them
                    strDigitsAfterComma = p_strString.substr( uCommaPosition + 1, 2 );
                }
                else
                {
                    //ds set to invalid (but dont stop parsing)
                    strDigitsAfterComma = _s( L"INVALID" );
                }
            }
            else
            {
                //ds get the 2 digits after the comma 
                strDigitsAfterComma = p_strString.substr( uCommaPosition + 1, 2 );
            }
        }
        else
        {
            //ds set to invalid (but dont stop parsing)
            strDigitsAfterComma = _s( L"INVALID" );
        }

        //ds evaluate strings if both are numbers
        if( true == _isANumber( strDigitsBeforeComma ) &&
            true == _isANumber( strDigitsAfterComma )  )
        {
            //ds we found the correct comma -> escapes loop with correct values
            bFoundCorrectComma = true;
        }
        else
        {
            //ds cut the searched part away and try again (big erase not implemented for TStrings)
            p_strString = p_strString.substr( uCommaPosition + 1, p_strString.length( ) );
        }
    }

    try
    {
        //ds if parsing went well
        if( true == bFoundCorrectComma )
        {
            //ds calculate amount (no exception possible here)
            double dAmount = strDigitsBeforeComma.to_numerical< double >( ) * 100 + strDigitsAfterComma.to_numerical< double >( );

            //ds check if amount is in pepper range
            if( dMaxAmount > dAmount )
            {
                //ds return amount
                return dAmount;
            }
            else
            {
                //ds log error
				LogWarning( TString::sprintf( _s( L"[_getAmountFromString] Amount: %f must be equal or shorter than 12 digits." ) ) % dAmount );

                //ds return max
                return dMaxAmount;
            }
        }

        //ds nothing found
        else
        {
            LogError( _s( L"[_getAmountFromString] no valid amount found in string." ) );

            THROW( XPCLIParsingFailedException, ErrorCategory );
        }
    }
    catch( const baselayer::XCOREDTStringConversionFailedException& /*ex*/ )
    {
        LogError( _s( L"[_getAmountFromString] could not convert string amount to double." ) );

        THROW( XPCLIStringConversionFailedException, ErrorCategory );
    }
}

const bool CPCLIImplementationEasycashPPP::_isValidResponseCode( const int& p_iResponseCode ) const
{
    //ds check in defined range
    switch( p_iResponseCode )
    {
        //ds all valid cases from 2011/04/28 (fallthrough intended)
        case ( 0 ):
        case ( 2 ):
        case ( 5 ):
        case ( 12 ):
        case ( 13 ):
        case ( 14 ):
        case ( 30 ):
        case ( 33 ):
        case ( 34 ):
        case ( 43 ):
        case ( 56 ):
        case ( 58 ):
        case ( 77 ):
        case ( 80 ):
        case ( 91 ):
        case ( 92 ):
        {
            return true;
        }
        default:
        {
            return false;
        }
    }
}

const bool CPCLIImplementationEasycashPPP::_isANumber( const TChar& p_chChar ) const
{
    if( ( _c( L'0' ) != p_chChar &&
          _c( L'1' ) != p_chChar &&
          _c( L'2' ) != p_chChar &&
          _c( L'3' ) != p_chChar &&
          _c( L'4' ) != p_chChar &&
          _c( L'5' ) != p_chChar &&
          _c( L'6' ) != p_chChar &&
          _c( L'7' ) != p_chChar &&
          _c( L'8' ) != p_chChar &&
          _c( L'9' ) != p_chChar ) ||
        ( TChar( )   == p_chChar ) )
    {
        //ds return false if not found
        return false;
    }
    else
    {
        //ds return true if the char is a number
        return true;
    }
}

const bool CPCLIImplementationEasycashPPP::_isANumber( const TString& p_strString ) const
{
    //ds define valid data characters
    TString strValidCharacters( _s( L"0123456789" ) );

    //ds search characters in input string
    if( TString::npos( ) != p_strString.find_first_not_of( strValidCharacters.to_TCharVector( ), 0 ) || TString( ) == p_strString )
    {
        //ds return false if not found
        return false;
    }
    else
    {
        //ds return true if all chars are numbers
        return true;
    } 
}

void CPCLIImplementationEasycashPPP::_updatePersistenceEndOfDay( const boost::shared_ptr< CPCLIMessage > p_pInOutMessage ) const
{
    //ds UPDATE CARD NAMES
    //ds get current card name
    TString strCardName( TString::from_native_string( p_pInOutMessage->cardName ) );

    //ds throw exception if card name is not set
    if( TString( ) == strCardName )
    {
        LogError( _s( L"[_updatePersistenceEndOfDay] card name string not set." ) );

        THROW( XPCLIEmptyStringException, ErrorCategory );
    }

    //ds replace all the spaces in the card name with an underline
    for( unsigned int i = 0; i < strCardName.length( ); ++i )
    {
        if( TChar::space( ) == strCardName[i] )
        {
            //ds replace it
            strCardName.replace( i, TChar::underline( ) );
        }
    }

    //ds get the persistence instance for this card number
    CPCLIPersistenceEasycashPPPEndOfDay::TPtr pPersistenceEndOfDay( _getPersistenceEndOfDay( strCardName ) );

    //ds update persistence
    switch( p_pInOutMessage->trxType )
    {
        case( TRXTYPE_GOODS_PAYM ):
        {
            pPersistenceEndOfDay->setPaymentCounter( pPersistenceEndOfDay->getPaymentCounter( ) + 1 );
            pPersistenceEndOfDay->setPaymentAmount( pPersistenceEndOfDay->getPaymentAmount( ) + p_pInOutMessage->getAmount( ) );
            break;
        }
        case( TRXTYPE_VOID_GOODS_PAYM ):
        {
            pPersistenceEndOfDay->setVoidPaymentCounter( pPersistenceEndOfDay->getVoidPaymentCounter( ) + 1 );
            pPersistenceEndOfDay->setVoidPaymentAmount( pPersistenceEndOfDay->getVoidPaymentAmount( ) + p_pInOutMessage->getAmount( ) );
            break;
        }
        case( TRXTYPE_CREDIT ):
        {
            pPersistenceEndOfDay->setCreditCounter( pPersistenceEndOfDay->getCreditCounter( ) + 1 );
            pPersistenceEndOfDay->setCreditAmount( pPersistenceEndOfDay->getCreditAmount( ) + p_pInOutMessage->getAmount( ) );
            break;
        }
        case(  TRXTYPE_VOID_CREDIT ):
        {
            pPersistenceEndOfDay->setVoidCreditCounter( pPersistenceEndOfDay->getVoidCreditCounter( ) + 1 );
            pPersistenceEndOfDay->setVoidCreditAmount( pPersistenceEndOfDay->getVoidCreditAmount( ) + p_pInOutMessage->getAmount( ) );
            break;
        }
        default:
        {
            LogError( _s( L"[_updatePersistenceEndOfDay] invalid transaction type." ) );
            THROW( XPCLIPersistenceException, ErrorCategory );
            break;
        }
    }
}


CPCLIPersistenceEasycashPPPEndOfDay::TPtr CPCLIImplementationEasycashPPP::_getPersistenceEndOfDay( const TString& p_strCardName ) const
{
    //ds check for empty argument
    if( TString( ) == p_strCardName )
    {
        LogError( _s( L"[_getPersistenceEndOfDay] empty string received." ) );

        THROW( XPCLIEmptyStringException, ErrorCategory ); 
    }
 
    //ds get all end of day data instances from db
    CPCLIPersistenceEasycashPPPEndOfDay::TPtrVector vecRetrieved( CPCLIPersistenceEasycashPPPEndOfDay::persistentRetrieve( ) );

    //ds loop through the vector and check all card names for the current one
    for ( CPCLIPersistenceEasycashPPPEndOfDay::TPtrVector::const_iterator cIter = vecRetrieved.begin( ); cIter != vecRetrieved.end( ); ++cIter )
    {
        //ds only check if really stored
        if( true == ( *cIter )->isPersistentStored( ) )
        { 
            //ds compare card name with stored one
            if( p_strCardName == ( *cIter )->getCardName( ) )
            {    
                //ds return the found instance
                return ( *cIter );
            }
        }
    }

    //ds if we are still here, return a new instance
    return CPCLIPersistenceEasycashPPPEndOfDay::TPtr( new CPCLIPersistenceEasycashPPPEndOfDay( p_strCardName ) );
}

} //namespace pep2
