#include <stdio.h>
#include <string.h>
#include "x2focuser.h"

#include "../../licensedinterfaces/theskyxfacadefordriversinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/basiciniutilinterface.h"
#include "../../licensedinterfaces/mutexinterface.h"
#include "../../licensedinterfaces/basicstringinterface.h"
#include "../../licensedinterfaces/tickcountinterface.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serialportparams2interface.h"

X2Focuser::X2Focuser(const char* pszDisplayName, 
												const int& nInstanceIndex,
												SerXInterface						* pSerXIn, 
												TheSkyXFacadeForDriversInterface	* pTheSkyXIn, 
												SleeperInterface					* pSleeperIn,
												BasicIniUtilInterface				* pIniUtilIn,
												LoggerInterface						* pLoggerIn,
												MutexInterface						* pIOMutexIn,
												TickCountInterface					* pTickCountIn)

{
	m_pSerX							= pSerXIn;		
	m_pTheSkyXForMounts				= pTheSkyXIn;
	m_pSleeper						= pSleeperIn;
	m_pIniUtil						= pIniUtilIn;
	m_pLogger						= pLoggerIn;	
	m_pIOMutex						= pIOMutexIn;
	m_pTickCount					= pTickCountIn;
	
	m_bLinked = false;
	m_nPosition = 0;
    m_fLastTemp = -273.15f; // aboslute zero :)

    // Read in settings
    if (m_pIniUtil) {
    }

    m_Af3Controller.SetSerxPointer(m_pSerX);
    m_Af3Controller.setSleeper(m_pSleeper);
}

X2Focuser::~X2Focuser()
{
    //Delete objects used through composition
	if (GetSerX())
		delete GetSerX();
	if (GetTheSkyXFacadeForDrivers())
		delete GetTheSkyXFacadeForDrivers();
	if (GetSleeper())
		delete GetSleeper();
	if (GetSimpleIniUtil())
		delete GetSimpleIniUtil();
	if (GetLogger())
		delete GetLogger();
	if (GetMutex())
		delete GetMutex();

}

#pragma mark - DriverRootInterface

int	X2Focuser::queryAbstraction(const char* pszName, void** ppVal)
{
    *ppVal = NULL;

    if (!strcmp(pszName, LinkInterface_Name))
        *ppVal = (LinkInterface*)this;

    else if (!strcmp(pszName, FocuserGotoInterface2_Name))
        *ppVal = (FocuserGotoInterface2*)this;

    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);

    else if (!strcmp(pszName, X2GUIEventInterface_Name))
        *ppVal = dynamic_cast<X2GUIEventInterface*>(this);

    else if (!strcmp(pszName, FocuserTemperatureInterface_Name))
        *ppVal = dynamic_cast<FocuserTemperatureInterface*>(this);

    else if (!strcmp(pszName, LoggerInterface_Name))
        *ppVal = GetLogger();

    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);

    else if (!strcmp(pszName, SerialPortParams2Interface_Name))
        *ppVal = dynamic_cast<SerialPortParams2Interface*>(this);

    return SB_OK;
}

#pragma mark - DriverInfoInterface
void X2Focuser::driverInfoDetailedInfo(BasicStringInterface& str) const
{
        str = "Focuser X2 plugin by Rodolphe Pineau";
}

double X2Focuser::driverInfoVersion(void) const							
{
	return DRIVER_VERSION;
}

void X2Focuser::deviceInfoNameShort(BasicStringInterface& str) const
{
    str="AF3";
}

void X2Focuser::deviceInfoNameLong(BasicStringInterface& str) const				
{
    deviceInfoNameShort(str);
}

void X2Focuser::deviceInfoDetailedDescription(BasicStringInterface& str) const		
{
	str = "AF3 Controller";
}

void X2Focuser::deviceInfoFirmwareVersion(BasicStringInterface& str)				
{
    if(!m_bLinked) {
        str="NA";
    }
    else {
        X2MutexLocker ml(GetMutex());
        // get firmware version
        char cFirmware[SERIAL_BUFFER_SIZE];
        m_Af3Controller.getFirmwareVersion(cFirmware, SERIAL_BUFFER_SIZE);
        str = cFirmware;
    }
}

void X2Focuser::deviceInfoModel(BasicStringInterface& str)							
{
    str="AF3";
}

#pragma mark - LinkInterface
int	X2Focuser::establishLink(void)
{
    char szPort[DRIVER_MAX_STRING];
    int nErr;

    X2MutexLocker ml(GetMutex());
    // get serial port device name
    portNameOnToCharPtr(szPort,DRIVER_MAX_STRING);
    nErr = m_Af3Controller.Connect(szPort);
    if(nErr)
        m_bLinked = false;
    else
        m_bLinked = true;

    return nErr;
}

int	X2Focuser::terminateLink(void)
{
    if(!m_bLinked)
        return SB_OK;

    X2MutexLocker ml(GetMutex());
    m_Af3Controller.haltFocuser();
    m_Af3Controller.Disconnect();
    m_bLinked = false;

	return SB_OK;
}

bool X2Focuser::isLinked(void) const
{
	return m_bLinked;
}

#pragma mark - ModalSettingsDialogInterface
int	X2Focuser::initModalSettingsDialog(void)
{
    return SB_OK;
}

int	X2Focuser::execModalSettingsDialog(void)
{
    int nErr = SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*					ui = uiutil.X2UI();
    X2GUIExchangeInterface*			dx = NULL;//Comes after ui is loaded
    bool bPressedOK = false;
    int nPosition = 0;
    int nPosLimit = 0;
    int nTmp;
    bool bReverse;
    mUiEnabled = false;

    if (NULL == ui)
        return ERR_POINTER;

    if ((nErr = ui->loadUserInterface("af3.ui", deviceType(), m_nPrivateMulitInstanceIndex)))
        return nErr;

    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;

    X2MutexLocker ml(GetMutex());
	// set controls values
    if(m_bLinked) {
        // new position (set to current )
        nErr = m_Af3Controller.getPosition(nPosition);
        if(nErr)
            return nErr;
        dx->setEnabled("newPos", true);
        dx->setEnabled("pushButton", true);
        dx->setPropertyInt("newPos", "value", nPosition);

        dx->setEnabled("posLimit", true);
        dx->setEnabled("pushButton_2", true);
        nErr = m_Af3Controller.getPosLimit(nPosLimit);
        dx->setPropertyInt("posLimit", "value", nPosLimit);
        
        dx->setEnabled("comboBox", true);
        nErr = m_Af3Controller.getStepSize(nTmp);
        if(nErr)
            return nErr;
        dx->setCurrentIndex("comboBox", nTmp-1);

        dx->setEnabled("comboBox_2", true);
        nErr = m_Af3Controller.getSpeed(nTmp);
        if(nErr)
            return nErr;
        dx->setCurrentIndex("comboBox_2", nTmp-1);
        
        dx->setEnabled("moveMult", true);
        nErr = m_Af3Controller.getMoveCurrentMultiplier(nTmp);
        dx->setPropertyInt("moveMult", "value", nTmp);

        dx->setEnabled("holdMult", true);
        nErr = m_Af3Controller.getHoldCurrentMultiplier(nTmp);
        dx->setPropertyInt("holdMult", "value", nTmp);

        dx->setEnabled("checkBox", true);
        nErr = m_Af3Controller.getReverseEnable(bReverse);
        dx->setChecked("checkBox", bReverse?1:0);
        
        dx->setEnabled("pushButton_3", true);
    }
    else {
        // disable all controls
        dx->setEnabled("newPos", false);
        dx->setEnabled("pushButton", false);
        dx->setEnabled("posLimit", false);
        dx->setEnabled("pushButton_2", false);
        dx->setEnabled("comboBox", false);
        dx->setEnabled("comboBox_2", false);
        dx->setEnabled("moveMult", false);
        dx->setEnabled("holdMult", false);
        dx->setEnabled("checkBox", false);
        dx->setEnabled("pushButton_3", false);
        
    }

    //Display the user interface
    mUiEnabled = true;
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;
    mUiEnabled = false;

    //Retreive values from the user interface
    if (bPressedOK) {
        nErr = SB_OK;
        if(m_bLinked){
            // update move current multiplier
            dx->propertyInt("moveMult", "value", nTmp);
            m_Af3Controller.setMoveCurrentMultiplier(nTmp);
            // update hold current multiplier
            dx->propertyInt("holdMult", "value", nTmp);
            m_Af3Controller.setHoldCurrentMultiplier(nTmp);
        }
    }
    return nErr;
}

void X2Focuser::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    int nErr = SB_OK;
    int nTmpVal;
    int nIndex;
    bool bTmp;
    
    char szErrorMessage[LOG_BUFFER_SIZE];

    if(!m_bLinked)
        return;

    if(!mUiEnabled)
        return;
    
    if (!strcmp(pszEvent, "on_timer")) {
        // update move current multiplier
        uiex->propertyInt("moveMult", "value", nTmpVal);
        m_Af3Controller.setMoveCurrentMultiplier(nTmpVal);
        // update hold current multiplier
        uiex->propertyInt("holdMult", "value", nTmpVal);
        m_Af3Controller.setHoldCurrentMultiplier(nTmpVal);
    }
    // new position
    else if (!strcmp(pszEvent, "on_pushButton_clicked")) {
        uiex->propertyInt("newPos", "value", nTmpVal);
        nErr = m_Af3Controller.syncMotorPosition(nTmpVal);
        if(nErr) {
            snprintf(szErrorMessage, LOG_BUFFER_SIZE, "Error setting new position : Error %d", nErr);
            uiex->messageBox("Set New Position", szErrorMessage);
            return;
        }
    }

    else if (!strcmp(pszEvent, "on_pushButton_2_clicked")) {
        uiex->propertyInt("posLimit", "value", nTmpVal);
        nErr = m_Af3Controller.setPosLimit(nTmpVal);
        if(nErr) {
            snprintf(szErrorMessage, LOG_BUFFER_SIZE, "Error setting new limit : Error %d", nErr);
            uiex->messageBox("Set New Limit", szErrorMessage);
            return;
        }
    }

    else if (!strcmp(pszEvent, "on_pushButton_3_clicked")) {
        m_Af3Controller.resetDevice();
        // reload all data
        nErr = m_Af3Controller.getPosition(nTmpVal);
        uiex->setPropertyInt("newPos", "value", nTmpVal);
        
        nErr = m_Af3Controller.getPosLimit(nTmpVal);
        uiex->setPropertyInt("posLimit", "value", nTmpVal);
        m_Af3Controller.setMaxMouvement(nTmpVal);   // do not limit movement to arbitrary value, this break automation

        nErr = m_Af3Controller.getStepSize(nTmpVal);
        uiex->setCurrentIndex("comboBox", nTmpVal-1);

        nErr = m_Af3Controller.getSpeed(nTmpVal);
        uiex->setCurrentIndex("comboBox_2", nTmpVal-1);
        
        nErr = m_Af3Controller.getMoveCurrentMultiplier(nTmpVal);
        uiex->setPropertyInt("moveMult", "value", nTmpVal);

        nErr = m_Af3Controller.getHoldCurrentMultiplier(nTmpVal);
        uiex->setPropertyInt("holdMult", "value", nTmpVal);

        nErr = m_Af3Controller.getReverseEnable(bTmp);
        uiex->setChecked("checkBox", bTmp?1:0);

    }

    else if (!strcmp(pszEvent, "on_comboBox_currentIndexChanged")) {
        nIndex = uiex->currentIndex("comboBox");
        nErr = m_Af3Controller.setStepSize(nIndex+1);
        if(nErr) {
            snprintf(szErrorMessage, LOG_BUFFER_SIZE, "Error setting new step size : Error %d", nErr);
            uiex->messageBox("Set New Step Size", szErrorMessage);
            return;
        }
    }

    else if (!strcmp(pszEvent, "on_comboBox_2_currentIndexChanged")) {
        nIndex = uiex->currentIndex("comboBox_2");
        nErr = m_Af3Controller.setSpeed(nIndex+1);
        if(nErr) {
            snprintf(szErrorMessage, LOG_BUFFER_SIZE, "Error setting new speed : Error %d", nErr);
            uiex->messageBox("Set New Speed", szErrorMessage);
            return;
        }
    }
    
    else if (!strcmp(pszEvent, "on_checkBox_stateChanged")) {
        bTmp = uiex->isChecked("checkBox")==1?true:false;
        nErr = m_Af3Controller.setReverseEnable(bTmp);
        if(nErr) {
            snprintf(szErrorMessage, LOG_BUFFER_SIZE, "Error changing direction : Error %d", nErr);
            uiex->messageBox("Change Direction", szErrorMessage);
            return;
        }
    }
}

#pragma mark - FocuserGotoInterface2
int	X2Focuser::focPosition(int& nPosition)
{
    int nErr;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());

    nErr = m_Af3Controller.getPosition(nPosition);
    m_nPosition = nPosition;
    return nErr;
}

int	X2Focuser::focMinimumLimit(int& nMinLimit) 		
{
	nMinLimit = 0;
    return SB_OK;
}

int	X2Focuser::focMaximumLimit(int& nPosLimit)			
{
    int nErr = SB_OK;
    
    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    m_Af3Controller.getPosLimit(nPosLimit);

	return nErr;
}

int	X2Focuser::focAbort()								
{   int nErr;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    nErr = m_Af3Controller.haltFocuser();
    return nErr;
}

int	X2Focuser::startFocGoto(const int& nRelativeOffset)	
{
    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    m_Af3Controller.moveRelativeToPosision(nRelativeOffset);
    return SB_OK;
}

int	X2Focuser::isCompleteFocGoto(bool& bComplete) const
{
    int nErr;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2Focuser* pMe = (X2Focuser*)this;
    X2MutexLocker ml(pMe->GetMutex());
	nErr = pMe->m_Af3Controller.isGoToComplete(bComplete);

    return nErr;
}

int	X2Focuser::endFocGoto(void)
{
    int nErr;
    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    nErr = m_Af3Controller.getPosition(m_nPosition);
    return nErr;
}

int X2Focuser::amountCountFocGoto(void) const					
{ 
	return 3;
}

int	X2Focuser::amountNameFromIndexFocGoto(const int& nZeroBasedIndex, BasicStringInterface& strDisplayName, int& nAmount)
{
	switch (nZeroBasedIndex)
	{
		default:
		case 0: strDisplayName="10 steps"; nAmount=10;break;
		case 1: strDisplayName="100 steps"; nAmount=100;break;
		case 2: strDisplayName="1000 steps"; nAmount=1000;break;
	}
	return SB_OK;
}

int	X2Focuser::amountIndexFocGoto(void)
{
	return 0;
}

#pragma mark - FocuserTemperatureInterface
int X2Focuser::focTemperature(double &dTemperature)
{
    int nErr = SB_OK;

    if(!m_bLinked) {
        dTemperature = -100.0;
        return NOT_CONNECTED;
    }
    X2MutexLocker ml(GetMutex());

    // Taken from Richard's Robofocus plugin code.
    // this prevent us from asking the temperature too often
    static CStopWatch timer;

    if(timer.GetElapsedSeconds() > 30.0f || m_fLastTemp < -99.0f) {
        nErr = m_Af3Controller.getTemperature(m_fLastTemp);
        timer.Reset();
    }

    dTemperature = m_fLastTemp;

    return nErr;
}

#pragma mark - SerialPortParams2Interface

void X2Focuser::portName(BasicStringInterface& str) const
{
    char szPortName[DRIVER_MAX_STRING];

    portNameOnToCharPtr(szPortName, DRIVER_MAX_STRING);

    str = szPortName;

}

void X2Focuser::setPortName(const char* pszPort)
{
    if (m_pIniUtil)
        m_pIniUtil->writeString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort);

}


void X2Focuser::portNameOnToCharPtr(char* pszPort, const int& nMaxSize) const
{
    if (NULL == pszPort)
        return;

    snprintf(pszPort, nMaxSize, DEF_PORT_NAME);

    if (m_pIniUtil)
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort, pszPort, nMaxSize);
    
}




