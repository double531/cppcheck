#ifndef _PCLIimplementationeasycashppp_h_
#define _PCLIimplementationeasycashppp_h_


#include "PCLIimplementation.h"
#include "COREDT.h"
#include "PCLIpersistenceeasycashpppendofday.h"
#include "PCLIpersistenceeasycashppptransaction.h"


//******************************************************************************** PEPPER - DONT CHANGE ********************************************************************************

#define Card_NoExpirationDate            "0000"        //ds substitute with according define in framework if available

//gv Hornbach parameters for Baustellennummer and Referenznummer
#define XMLAdditionalParameterConstructionSiteNumber "ConstructionSiteNumber"
#define XMLAdditionalParameterReferenceNumber        "ReferenceNumber"

//**************************************************************************************************************************************************************************************


namespace pput
{
    /**
    * @brief A friend class for unittesting to have access to private methods. 
    */
    class CPPUTImplementationOfSpecificationEasycashPPPSuite;
}


namespace pep2
{


/**
* @brief These enumerators are used to set automatically the language of texts. 
*/
enum ETextCode
{
    /**
    * @brief A Display Text enum for an invalid response. 
    */
    eEasycashPPP_eDisplayInvalidResponse,

    /**
    * @brief A Display Text enum for an invalid response code. 
    */
    eEasycashPPP_eDisplayInvalidResponseCode,

    /**
    * @brief A Display Text enum for a persistence failure. 
    */
    eEasycashPPP_eDisplayPersistenceFail,

    /**
    * @brief A Display Text enum for the message tag. 
    */
    eEasycashPPP_eDisplayResponseInformation,

    /**
    * @brief A Display Text enum if the message tag is missing. 
    */
    eEasycashPPP_eDisplayNoResponseInformation,

    /**
    * @brief A EndOfDay Text enum for the title. 
    */
    eEasycashPPP_eEndOfDayTitle,

    /**
    * @brief A EndOfDay Text enum for the time format. 
    */
    eEasycashPPP_eEndOfDayTime,

    /**
    * @brief A EndOfDay Text enum for a purchase. 
    */
    eEasycashPPP_eEndOfDayPayment,

    /**
    * @brief A EndOfDay Text enum for a reversal purchase. 
    */
    eEasycashPPP_eEndOfDayVoidPayment,

    /**
    * @brief A EndOfDay Text enum for a credit. 
    */
    eEasycashPPP_eEndOfDayCredit,

    /**
    * @brief A EndOfDay Text enum for a reversal credit. 
    */
    eEasycashPPP_eEndOfDayVoidCredit,

    /**
    * @brief A EndOfDay Text enum for the total. 
    */
    eEasycashPPP_eEndOfDayTotal,

    /**
    * @brief A Transaction Text enum for a purchase. 
    */
    eEasycashPPP_eTransactionPayment,

    /**
    * @brief A Transaction Text enum for a void payment. 
    */
    eEasycashPPP_eTransactionVoidPayment,

    /**
    * @brief A Transaction Text enum for a credit. 
    */
    eEasycashPPP_eTransactionCredit,

    /**
    * @brief A Transaction Text enum for a void credit. 
    */
    eEasycashPPP_eTransactionVoidCredit,

    /**
    * @brief A Transaction Text enum for the current balance. 
    */
    eEasycashPPP_eTransactionBalance,

    /**
    * @brief A Transaction Text enum for the expiration. 
    */
    eEasycashPPP_eTransactionExpiration, 

    /**
    * @brief A Transaction Text enum for the total. 
    */
    eEasycashPPP_eTransactionTotal,

    /**
    * @brief A Transaction Text enum for the current balance amount. 
    */
    eEasycashPPP_eTransactionBalanceAmount  
}; 


/**
* @brief This class represents an implementation of the easycash PrePaidPlus protocol for pepper 10.5.0. 
*/
class CPCLIImplementationEasycashPPP : public IPCLIImplementation
{

//ds ctor/dtor
public:
	
    /**
    * @brief The default constructor. Sets all members to empty/invalid and state to started.
    */
	CPCLIImplementationEasycashPPP( );
	
    /**
    * @brief The default virtual destructor. Not specialized due no dynamic memory is unhandled de & allocated.
    */
	virtual ~CPCLIImplementationEasycashPPP( );

//ds attributes                                                                                                                                                                                  
private:

    /**
    * @internal The Terminal-ID for the current pepper instance, set by eftConfigDriver.
    */
    TString      m_strTerminalID;

    /**
    * @internal The Branch-ID for the current pepper instance, set by eftConfigDriver.
    */
    TString      m_strBranchID;

    /**
    * @internal The Timeout for the receive tries, defaulted to 90000ms or set by eftConfigDriver.
    */
    unsigned int m_uReceiveTimeOut;

    /**
    * @internal A smart pointer to the transaction persistence instance of the current pepper instance.
    */
    CPCLIPersistenceEasycashPPPTransaction::TPtr m_pPersistenceTransaction;

    /**
    * @internal A boolean to mark if the response from HOST is valid.
    */
    bool m_bIsResponseValid;

    /**
    * @internal A boolean to mark if all essential Tags in the Response were set, needed for advanced display texting.
    */
    bool m_bAreAllResponseTagsSet;

    /**
    * @internal A boolean to mark if the current transaction has already been received.
    */
    bool m_bIsTransactionAlreadyReceived;

    /**
    * @internal The Hornbach "IDNr" transmitted by the ECR in eftTrx.
    */
    TString m_strReferenceNumber;

    /**
    * @internal The Hornbach "BStNr" transmitted by the ECR in eftTrx.
    */
    TString m_strConstructionSiteNumber;

//ds friends
private:

    /**
    * @brief A friend class for unittesting.
    */
    friend class pput::CPPUTImplementationOfSpecificationEasycashPPPSuite;

//ds helpers
private:

    /**
    * @brief A getter function to retrieve a text in the desired language.
    *
    * @param p_eTextCode The enum for the desired text.
    * @param p_uLanguageCode The language code of pepper.
    *
    * @return The text as TString.
    *
    * @throws XPCLITextNotSetException If according text was not found or text are not defined for desired language.
    */
    const TString _getText( const ETextCode& p_eTextCode, EPPIILanguage p_eLanguage ) const;

    /**
    * @brief The receive function to receive data from a specified IP and port number.
    *
    * @param p_pInOutMessage The IN/OUT message of pepper.
    *
    * @return EErrorCodes.
    */
    const int _receiveTransaction( boost::shared_ptr< CPCLIMessage > p_pInOutMessage );

    /**
    * @brief A mocked receive function to simulate EndOfDay response from terminal.
    *
    * @param p_pInOutMessage The IN/OUT message of pepper (only need reading access here).
    *
    * @return EErrorCodes.
    */
    const int _receiveEndOfDay( const boost::shared_ptr< CPCLIMessage > p_pInOutMessage );

    /**
    * @brief The function to create a End of Day ticket out of the IN/OUT message.
    *
    * @param p_pInOutMessage The IN/OUT message of pepper (only need reading access here).
    *
    * @throws XPCLIStringConversionFailedException If a conversion error happened.
    */
    void _buildEndOfDayReceipt( const boost::shared_ptr< CPCLIMessage > p_pInOutMessage ) const;

    /**
    * @brief The classic fill function to write formatted data to the IN/OUT message of pepper.
    *
    * @param p_bsReceivedMessage The received message from Host.
    * @param p_pInOutMessage The IN/OUT message of pepper.
    *
    * @return EErrorCodes.
    */
	const int _fillInOutMessage( const TByteStream& p_bsReceivedMessage, boost::shared_ptr< CPCLIMessage > p_pInOutMessage );

    /**
    * @brief A parser function to parse the XML tree created of the received response.
    *
    * @param p_cTree The XML tree created of the received response.
    * @param p_pInOutMessage The IN/OUT message of pepper.
    *
    * @return The response code from the Host.
    */
    const int _parseXMLTree( const TXmlTree& p_cTree, boost::shared_ptr< CPCLIMessage > p_pInOutMessage );

    /**
    * @brief A getter function to get a formatted response information for direct display.
    *
    * @param p_strMessageTag The string to treat as response information.
    * @param p_pInOutMessage The IN/OUT message of pepper.
    *
    * @return The response information of the Host in correct language and format
    */
    const TString _getResponseInformation( const TString& p_strMessageTag, const boost::shared_ptr< CPCLIMessage > p_pInOutMessage ) const;
    
    /**
    * @brief A getter function to parse and retrieve the amount out of a string, separated by a specified symbol.
    *
    * @param p_strString The string to find the amount in.
    * @param p_chSeparator The separator symbol of the amount.
    *
    * @return The response code from the Host.
    *
    * @throws XPCLIParsingFailedException If parsing was not possible.
    * @throws XPCLIStringConversionFailedException If a conversion error happened.
    */
    const double _getAmountFromString( TString p_strString, const TChar& p_chSeparator ) const;

    /**
    * @brief A validator function to retrieve if the passed response code is valid or not.
    *
    * @param p_iResponseCode The response code to check.
    *
    * @return true if valid, false if invalid.
    */
    const bool _isValidResponseCode( const int& p_iResponseCode ) const;

    /**
    * @brief A overloaded validator function to retrieve if the passed character is numerical or not.
    *
    * @param p_chChar The character to check.
    *
    * @return true if numerical, false if not numerical.
    */
    const bool _isANumber( const TChar& p_chChar ) const;

    /**
    * @brief A overloaded validator function to retrieve if the passed string contains only numericals or not.
    *
    * @param p_strString The string to check.
    *
    * @return true if all characters are numerical, false if not.
    */
    const bool _isANumber( const TString& p_strString ) const;

    /**
    * @brief A function to update all parameters needed by eftEndOfDay.
    *
    * @param p_pInOutMessage The IN/OUT message of pepper.
    *
    * @throws XPCLIEmptyStringException If card name is not set.
    * @throws XPCLIPersistenceException If a internal persistence error occured.
    */
    void _updatePersistenceEndOfDay( const boost::shared_ptr< CPCLIMessage > p_pInOutMessage ) const;

    /**
    * @brief A round function to get a handle to a persistence instance.
    * Automatically initializes the persistence with the given card name if nothing was found.
    *
    * @param p_strDouble The double value to round.
    *
    * @return a valid smart pointer to the instance.
    *
    * @throws XPCLIEmptyStringException If an empty string was passed.
    * @throws XPCLIPersistenceException If a internal persistence error occured.
    */
    CPCLIPersistenceEasycashPPPEndOfDay::TPtr _getPersistenceEndOfDay( const TString& p_strCardName ) const;

//******************************************************************************** PEPPER - DONT CHANGE ********************************************************************************

public:

    static const std::string version;
    static std::string getImplVersion( ) { return version; };

	bool isEFTAvailable( boost::shared_ptr< CPCLIMessage > in_outMsg,
                                unsigned char * lowlayer_state = NULL,
								bool withoutTimer = false, 
								bool waitBefore = false );

	long specificationDependantValidation( short method, boost::shared_ptr< CPCLIMessage > in_outMsg );

	void buildClientReceipt( boost::shared_ptr< CPCLIMessage >  in_outMsg, const char * receiveData = "" );	
	void buildMerchantReceipt( boost::shared_ptr< CPCLIMessage >  in_outMsg, const char * receiveData = "" );

	long executeInit( boost::shared_ptr< CPCLIMessage >  in_outMsg );	
	long executeUtility( boost::shared_ptr< CPCLIMessage >  in_outMsg );
	
	long openEFTDevice( boost::shared_ptr< CPCLIMessage >  in_outMsg );
	long closeEFTDevice( boost::shared_ptr< CPCLIMessage >  in_outMsg );
	
	long translateEftConfigDriver( boost::shared_ptr< CPCLIMessage >  in_outMsg );	
	long translateEftOpen( boost::shared_ptr< CPCLIMessage >  in_outMsg );
	long translateEftTrx( boost::shared_ptr< CPCLIMessage >  in_outMsg );
	long translateEftUtility( boost::shared_ptr< CPCLIMessage >  in_outMsg );
	long translateEftEndOfDay( boost::shared_ptr< CPCLIMessage >  in_outMsg );
	long translateEftInit( boost::shared_ptr< CPCLIMessage >  in_outMsg );
	
    short getCardType ( boost::shared_ptr< CPCLIMessage > in_outMsg,
                        std::string key1 = "",
                        std::string key2 = "",
					    std::string key3 = "",
					    std::string key4 = "",
                        std::string key5 = "" );
	
	long startIdleProcess( boost::shared_ptr< CPCLIMessage >  in_outMsg, bool newFlag = false );	
	long stopIdleProcess( bool stopFlag = false );
	
	long getVersion( boost::shared_ptr< CPCLIMessage >  in_outMsg );
	
	long setDisconnectedState( boost::shared_ptr< CPCLIMessage >  in_outMsg );
	long setConnectedState( boost::shared_ptr< CPCLIMessage >  in_outMsg );
	
	long stopProtocol( boost::shared_ptr< CPCLIMessage >  in_outMsg );
	long startProtocol( boost::shared_ptr< CPCLIMessage >  in_outMsg );
	
	long initDevice( boost::shared_ptr< CPCLIMessage >  in_outMsg );
    void addTicketHeaderFooter( std::string printFile, boost::shared_ptr< CPCLIMessage >  in_outMsg );
	
	long send( short method, boost::shared_ptr< CPCLIMessage > in_outMsg );
	long receive( boost::shared_ptr< CPCLIMessage >  in_outMsg );
	
	long prepareTrxMsg( boost::shared_ptr< CPCLIMessage >  in_outMsg );
	long prepareEndOfDayMsg( boost::shared_ptr< CPCLIMessage >  in_outMsg );
	long prepareOpenMsg( boost::shared_ptr< CPCLIMessage > in_outMsg );
	long prepareCloseMsg( boost::shared_ptr< CPCLIMessage > in_outMsg );
    long prepareStatusMsg( boost::shared_ptr< CPCLIMessage > in_outMsg );

//************************************************************************************************************************************************************************************** 
  
};


} //namespace pep2


#endif //_PCLIimplementationeasycashppp_h_
