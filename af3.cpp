//
//  Created by Rodolphe Pineau on 2020/01/10.
//  AF3 X2 plugin


#include "af3.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>
#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif
#ifdef SB_WIN_BUILD
#include <time.h>
#endif


CAf3Controller::CAf3Controller()
{

    m_pSerx = NULL;

    m_bDebugLog = false;
    m_bIsConnected = false;

    m_nCurPos = 0;
    m_nTargetPos = 0;
    m_nPosLimit = 1000000;
    m_bMoving = false;
    m_nLastPosition = 0;
    
	m_bResetOnConnect = false;

#ifdef PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\AF3Log.txt";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/AF3Log.txt";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/AF3Log.txt";
#endif
    Logfile = fopen(m_sLogfilePath.c_str(), "w");
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CAf3Controller::CAf3Controller] Version %3.2f build 2020_02_01_1605.\n", timestamp, DRIVER_VERSION);
    fprintf(Logfile, "[%s] [CAf3Controller] Constructor Called.\n", timestamp);
    fflush(Logfile);
#endif
}

CAf3Controller::~CAf3Controller()
{
#ifdef	PLUGIN_DEBUG
    // Close LogFile
    if (Logfile) fclose(Logfile);
#endif
}

int CAf3Controller::Connect(const char *pszPort)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    
    if(!m_pSerx)
        return ERR_COMMNOLINK;

#ifdef PLUGIN_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CAf3Controller::Connect Called %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif

    // 115.2K 8N1
    nErr = m_pSerx->open(pszPort, 115200, SerXInterface::B_NOPARITY, "-RTS_CONTROL 1");
    if( nErr == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected)
        return nErr;

    m_pSleeper->sleep(2000);

#ifdef PLUGIN_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CAf3Controller::Connect connected to %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif
	
#ifdef PLUGIN_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CAf3Controller::Connect getting device firnware\n", timestamp);
	fflush(Logfile);
#endif
	nErr = getFirmwareVersion(m_szFirmwareVersion, SERIAL_BUFFER_SIZE);
	if(nErr) {
		m_bIsConnected = false;
#ifdef PLUGIN_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CAf3Controller::Connect **** ERROR **** getting device firmware\n", timestamp);
		fflush(Logfile);
#endif
        return nErr;
    }
	#ifdef PLUGIN_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CAf3Controller::Connect device firnware = %s\n", timestamp, m_szFirmwareVersion);
		fflush(Logfile);
	#endif

    getPosition(m_nPosLimit);
    setMaxMouvement(m_nPosLimit); // we want to move as we see fit
    sendCommand("SBUF300", szResp, SERIAL_BUFFER_SIZE);
    
	return nErr;
}

void CAf3Controller::Disconnect()
{
    if(m_bIsConnected && m_pSerx) {
        m_pSerx->close();
    }
 
	m_bIsConnected = false;
}

#pragma mark - Device settings

void CAf3Controller::resetDevice()
{
    int nErr;
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ;

    nErr = sendCommand("[RSET]", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return;

    return;

}

#pragma mark move commands
int CAf3Controller::haltFocuser()
{
    int nErr;
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = sendCommand("[STOP]", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(!strstr(szResp,"OK"))
        nErr = ERR_CMDFAILED;

    return nErr;
}

int CAf3Controller::gotoPosition(int nPos)
{
    int nErr;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if (nPos>m_nPosLimit)
        return ERR_LIMITSEXCEEDED;

#ifdef PLUGIN_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CAf3Controller::gotoPosition goto position  : %d\n", timestamp, nPos);
    fflush(Logfile);
#endif

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "[STRG%d]", nPos);
    nErr = sendCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    if(!strstr(szResp,"OK"))
        return ERR_CMDFAILED;

    nErr = sendCommand("[SMOV]", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    if(!strstr(szResp,"OK"))
        return ERR_CMDFAILED;

    m_nTargetPos = nPos;
    m_nLastPosition = m_nCurPos;
    
    m_MoveTimer.Reset();
    return nErr;
}

int CAf3Controller::moveRelativeToPosision(int nSteps)
{
    int nErr;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#ifdef PLUGIN_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CAf3Controller::gotoPosition goto relative position  : %d\n", timestamp, nSteps);
    fflush(Logfile);
#endif

    m_nTargetPos = m_nCurPos + nSteps;
    nErr = gotoPosition(m_nTargetPos);
    return nErr;
}

#pragma mark command complete functions

int CAf3Controller::isGoToComplete(bool &bComplete)
{
    int nErr = PLUGIN_OK;
    bool bIsMoving;
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    bComplete = false;
    
    nErr = isMotorMoving(bIsMoving);
    if(nErr)
        return nErr;
    if(bIsMoving)
        return nErr;
    
    nErr = getPosition(m_nCurPos);
    if(nErr)
        return nErr;

    if(m_nCurPos == m_nTargetPos)
        bComplete = true;
    else
        bComplete = false;
    return nErr;
}

int CAf3Controller::isMotorMoving(bool &bMoving)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    int nMoving;
    
    if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    bMoving = false;
    // give the focuser some time to actually move.
    if(m_MoveTimer.GetElapsedSeconds()<1)
        return nErr;
    m_MoveTimer.Reset();
    
    nErr = sendCommand("[GMOV]", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    nMoving = atoi(szResp);
    bMoving = (nMoving==1);

    return nErr;
}

#pragma mark - getters and setters

int CAf3Controller::getFirmwareVersion(char *pszVersion, int nStrMaxLen)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    nErr = sendCommand("[GFRM]", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
#ifdef PLUGIN_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CAf3Controller::getFirmwareVersion szResp : %s\n", timestamp, szResp);
    fflush(Logfile);
#endif
    strncpy(pszVersion, szResp, nStrMaxLen);
    return nErr;
}

int CAf3Controller::getTemperature(double &dTemperature)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = sendCommand("[GTMC]", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // convert string value to double
    dTemperature = atof(szResp);
	if(dTemperature == -127.0)
		dTemperature = -100.0;
    return nErr;
}

int CAf3Controller::getPosition(int &nPosition)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = sendCommand("[GPOS]", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    m_nCurPos = atoi(szResp);
    nPosition = m_nCurPos;
    
    return nErr;
}


int CAf3Controller::syncMotorPosition(int nPos)
{
    int nErr = PLUGIN_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "[SPOS%d]", nPos);
    
    nErr = sendCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(!strstr(szResp,"OK"))
        nErr = ERR_CMDFAILED;
    m_nCurPos = nPos;
    return nErr;
}



int CAf3Controller::getPosLimit(int &nLimit)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    nErr = sendCommand("[GMXP]", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return 0;

    m_nPosLimit = atoi(szResp);
    nLimit = m_nPosLimit;

    return nErr;
}

int CAf3Controller::setPosLimit(int nLimit)
{
    int nErr = PLUGIN_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "[SMXP%d]", nLimit);
    
    nErr = sendCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(!strstr(szResp,"OK"))
        nErr = ERR_CMDFAILED;

    m_nPosLimit = nLimit;
    setMaxMouvement(m_nPosLimit); // we want to move as we see fit
    return nErr;
}

int CAf3Controller::getStepSize(int &nStepSize)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    nErr = sendCommand("[GSTP]", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return 0;

    nStepSize = atoi(szResp);

    return nErr;
}

int CAf3Controller::setStepSize(int nStepSize)
{
    int nErr = PLUGIN_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "[SSTP%d]", nStepSize);
    
    nErr = sendCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(!strstr(szResp,"OK"))
        nErr = ERR_CMDFAILED;

    return nErr;
}

int CAf3Controller::getSpeed(int &nSpeed)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    nErr = sendCommand("[GSPD]", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return 0;

    nSpeed = atoi(szResp);

    return nErr;
}

int CAf3Controller::setSpeed(int nSpeed)
{
    int nErr = PLUGIN_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "[SSPD%d]", nSpeed);
    
    nErr = sendCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(!strstr(szResp,"OK"))
        nErr = ERR_CMDFAILED;

    return nErr;
}

int CAf3Controller::getMoveCurrentMultiplier(int &nMoveCurrentMultiplier)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    nErr = sendCommand("[GMMM]", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return 0;

    nMoveCurrentMultiplier = atoi(szResp);

    return nErr;
}

int CAf3Controller::setMoveCurrentMultiplier(int nMoveCurrentMultiplier)
{
    int nErr = PLUGIN_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "[SMMM%d]", nMoveCurrentMultiplier);
    
    nErr = sendCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(!strstr(szResp,"OK"))
        nErr = ERR_CMDFAILED;

    return nErr;
}

int CAf3Controller::getHoldCurrentMultiplier(int &nHoldCurrentMultiplier)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    nErr = sendCommand("[GMHM]", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return 0;

    nHoldCurrentMultiplier = atoi(szResp);

    return nErr;
}

int CAf3Controller::setHoldCurrentMultiplier(int nHoldCurrentMultiplier)
{
    int nErr = PLUGIN_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "[SMHM%d]", nHoldCurrentMultiplier);
    
    nErr = sendCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(!strstr(szResp,"OK"))
        nErr = ERR_CMDFAILED;

    return nErr;
}

int CAf3Controller::getReverseEnable(bool &bEnable)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    nErr = sendCommand("[GREV]", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return 0;

    bEnable = atoi(szResp)==1?true:false;

    return nErr;
}

int CAf3Controller::setReverseEnable(bool bEnable)
{
    int nErr = PLUGIN_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "[SREV%d]", bEnable?1:0);
    nErr = sendCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(!strstr(szResp,"OK"))
        nErr = ERR_CMDFAILED;

    return nErr;}

int CAf3Controller::getMaxMouvement(int &nMaxMove)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;
    nErr = sendCommand("[GMXM]", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return 0;

    nMaxMove = atoi(szResp);

    return nErr;

}

int CAf3Controller::setMaxMouvement(int nMaxMove)
{
    int nErr = PLUGIN_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "[SMXM%d]", nMaxMove);
    
    nErr = sendCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(!strstr(szResp,"OK"))
        nErr = ERR_CMDFAILED;

    return nErr;

}


#pragma mark - command and response functions

int CAf3Controller::sendCommand(const char *pszszCmd, char *pszResult, int nResultMaxLen)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    unsigned long  ulBytesWrite;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    m_pSerx->purgeTxRx();
#ifdef PLUGIN_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CAf3Controller::sendCommand Sending %s\n", timestamp, pszszCmd);
	fflush(Logfile);
#endif
    nErr = m_pSerx->writeFile((void *)pszszCmd, strlen(pszszCmd), ulBytesWrite);
    m_pSerx->flushTx();

    if(nErr){
        return nErr;
    }

    if(pszResult) {
        // read response
        nErr = readResponse(szResp, SERIAL_BUFFER_SIZE);
        if(nErr){
            return nErr;
        }
#ifdef PLUGIN_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CAf3Controller::sendCommand response \"%s\"\n", timestamp, szResp);
		fflush(Logfile);
#endif

        strncpy(pszResult, szResp, nResultMaxLen);
#ifdef PLUGIN_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CAf3Controller::sendCommand response copied to pszResult : \"%s\"\n", timestamp, pszResult);
		fflush(Logfile);
#endif
    }
    return nErr;
}

int CAf3Controller::readResponse(char *pszRespBuffer, int nBufferLen)
{
    int nErr = PLUGIN_OK;
    unsigned long ulBytesRead = 0;
    unsigned long ulTotalBytesRead = 0;
    char *pszBufPtr;
    std::string sTmp;
    std::string sResp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    memset(pszRespBuffer, 0, (size_t) nBufferLen);
    pszBufPtr = pszRespBuffer;

    do {
        nErr = m_pSerx->readFile(pszBufPtr, 1, ulBytesRead, MAX_TIMEOUT);
        if(nErr) {
            return nErr;
        }

        if (ulBytesRead !=1) {// timeout
#ifdef PLUGIN_DEBUG
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(Logfile, "[%s] CAf3Controller::readResponse timeout\n", timestamp);
			fflush(Logfile);
#endif
            nErr = ERR_NORESPONSE;
            break;
        }
        ulTotalBytesRead += ulBytesRead;
    } while (*pszBufPtr++ != ')' && ulTotalBytesRead < nBufferLen );

	// cleanup the response
	sTmp.assign(pszRespBuffer);
	sResp = trim(sTmp,"()");
	strncpy(pszRespBuffer, sResp.c_str(), SERIAL_BUFFER_SIZE);

    return nErr;
}

std::string& CAf3Controller::trim(std::string &str, const std::string& filter )
{
    return ltrim(rtrim(str, filter), filter);
}

std::string& CAf3Controller::ltrim(std::string& str, const std::string& filter)
{
    str.erase(0, str.find_first_not_of(filter));
    return str;
}

std::string& CAf3Controller::rtrim(std::string& str, const std::string& filter)
{
    str.erase(str.find_last_not_of(filter) + 1);
    return str;
}

std::string CAf3Controller::findField(std::vector<std::string> &svFields, const std::string& token)
{
    for(int i=0; i<svFields.size(); i++){
        if(svFields[i].find(token)!= -1) {
            return svFields[i];
        }
    }
    return std::string();
}
