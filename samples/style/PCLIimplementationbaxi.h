#ifdef _WINDOWS
#ifndef _X64
#ifndef _PCLIimplementationbaxi_h_
#define _PCLIimplementationbaxi_h_

//ds base class
#include "PCLIimplementation.h"

//ds specific
#include "..\\auxiliary\\baxiwrapper\\functionparameters.h"

//******************************************************************************** PEPPER - DONT CHANGE ********************************************************************************

#define BAXI_Default_Timeout                     90000
#define BAXI_Default_BaudRate                    9600

#define BAXI_DummyRefNbr                         "dummyRefNbr"    //gv used for "force offline" (Dankort)


//**************************************************************************************************************************************************************************************

//ds functions provided by dll interface
typedef bool ( *pInitializeWrapper )( CBAXIBasicParameters* p_pBAXIBasicParameters );
typedef void ( *pExitWrapper )( );
typedef bool ( *pOpenBAXI )( void* p_pBAXIConfigurationParameters );
typedef bool ( *pSendData )( void* p_pSendDataString );
typedef bool ( *pProcessTransaction )( void* p_ptBAXITransactionParameters );
typedef bool ( *pProcessAdministration )( void* p_pBAXIAdministrationParameters );
typedef bool ( *pTransferCardData )( void* p_pBAXITransferCardDataParameters );
typedef bool ( *pCloseBAXI )( );
typedef bool ( *pGetBaxiwrapperVersion )( char *        chBaxiVersion, 
                                          unsigned int* uPepperCountries,
                                          unsigned int* uPepperMajorRevision,
                                          unsigned int* uPepperMinorRevision,
                                          unsigned int* uPepperSubversionRevision,
                                          unsigned int* uPepperApiMajorRevision,
                                          unsigned int* uPepperApiMinorRevision,
                                          unsigned int* uPepperOsArchitecture,
                                          unsigned int* uPepperReleaseType,
                                          unsigned int* uPepperConfigurationType );

namespace pep2
{

/**
* @brief This class represents an implementation of the BAXI protocol for pepper. 
*/
class CPCLIImplementationBAXI : public IPCLIImplementation
{

//ds ctor/dtor
public:

    /**
    * @brief The default constructor. Sets all members to empty/invalid and state to started.
    */
    CPCLIImplementationBAXI( );

    /**
    * @brief The default virtual destructor. Closes BAXI if possible and UnInitializes COM.
    */
    virtual ~CPCLIImplementationBAXI( );

//ds attributes
private:

    //ds parameter instance
    boost::shared_ptr< CBAXIBasicParameters > m_pBAXIBasicParameters;

    //ds parameter instance for all configuration settings (needed as member because it gets set during different pepper calls)
    CBAXIConfigurationParameters m_cBAXIConfigurationParameters;

    //ds needed for failed dll loadings
    bool m_bIsWrapperLoaded;

	//dh a buffer holdin "loyalty information" no idea what this is for...
	TString m_strBufferLoyaltyInfo;

	//dh several long values for flags and values
    long m_lEnableVAT;
    long m_lAmountVAT;
    long m_lEnableCashback;
    long m_lAmountCashback;


//ds helpers
private:

    //ds DLL "functions"
    pInitializeWrapper     _initializeWrapper;
    pExitWrapper           _exitWrapper;
    pOpenBAXI              _openBAXI;
    pSendData              _sendData;
    pProcessTransaction    _processTransaction;
    pProcessAdministration _processAdministration;
    pTransferCardData      _transferCardData;
    pCloseBAXI             _closeBAXI;
    pGetBaxiwrapperVersion _getBaxiwrapperVersion;
    TString                 m_strReportFileName;

    //ds logging
    void _logSendCall( const TString& p_strFunctioName , const TString& p_strFunctionParameters ) const;
    void _logSendSuccess( const TString& p_strFunctioName1 ) const;
    void _logSendFailure( const TString& p_strFunctioName2 ) const;

    //gv convert encoding and format received receipt
    const std::string _formatReceiveReceipt( const char* p_charText, unsigned short p_uReceiptFormat ) const;

    //gv this is the character set Pepper expects
    EPPIITerminalCodePage m_eExpectedEncoding;

    /**
    * @brief An initialize function for the loading, registring, instanciating and event linking of BAXI
    *
    * @throws XPCLIDLLImportException If an internal error occurs.
    */
    const bool _loadWrapper( );

//******************************************************************************** PEPPER - DONT CHANGE ********************************************************************************

public:

    static const std::string version;
    static std::string getImplVersion() {return version;};

    //gv mandatory parameter for meeting security requirements of Nets
    std::string m_strVendorInfoExtended;
	
	// don't change these method signatures
	// these must be implemented

	bool isEFTAvailable(boost::shared_ptr< CPCLIMessage > in_outMsg,
                                unsigned char * lowlayer_state = NULL,
								bool withoutTimer = false, 
								bool waitBefore = false);

	long specificationDependantValidation(short method, boost::shared_ptr< CPCLIMessage > in_outMsg);

	void buildClientReceipt(boost::shared_ptr< CPCLIMessage >  in_outMsg, const char * receiveData = "");
	
	void buildMerchantReceipt(boost::shared_ptr< CPCLIMessage >  in_outMsg, const char * receiveData = "");

	long executeInit(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long executeUtility(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long openEFTDevice(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long closeEFTDevice(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long translateEftConfigDriver(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long translateEftOpen(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long translateEftTrx(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long translateEftUtility(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long translateEftEndOfDay(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long translateEftInit(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
    short getCardType (boost::shared_ptr< CPCLIMessage > in_outMsg,
                       std::string key1 = "",
                       std::string key2 = "",
					   std::string key3 = "",
					   std::string key4 = "",
                       std::string key5 = "" );
	
	long updateStateVariables(short method, boost::shared_ptr< CPCLIMessage > in_outMsg);
	
	long startIdleProcess(boost::shared_ptr< CPCLIMessage >  in_outMsg, bool newFlag = false);
	
	long stopIdleProcess(bool stopFlag = false);
	
	int getUserInputTimeout();
	
	std::string getTimeoutTicketMsg();
	
	std::string getNegCCEODTicketMsg();
	
	std::string getNegDCEODTicketMsg();
	
	long getVersion(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long setDisconnectedState(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long setConnectedState(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long stopProtocol(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long startProtocol(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long initDevice(boost::shared_ptr< CPCLIMessage >  in_outMsg);

    void addTicketHeaderFooter(std::string printFile, boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long send( short method, boost::shared_ptr< CPCLIMessage > in_outMsg);
	
	long receive(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long prepareTrxMsg(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long prepareEndOfDayMsg(boost::shared_ptr< CPCLIMessage >  in_outMsg);
	
	long prepareStatusMsg(boost::shared_ptr< CPCLIMessage > in_outMsg);
	
	
	// these methods must be present, but are not yet used
	void debugHighLevel();
	void initStateMachine();
	void addHeader();
	void addTail();
	void debugLowLevel();

//**************************************************************************************************************************************************************************************

};

} // namespace pep2

#endif // #ifndef _PCLIimplementationbaxi_h_
#endif // #ifndef _X64
#endif // #ifdef _WINDOWS
