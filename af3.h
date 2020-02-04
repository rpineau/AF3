//
//  Created by Rodolphe Pineau on 2020/01/10.
//  AF3 X2 plugin

#ifndef __AF3__
#define __AF3__
#include <math.h>
#include <string.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#include <exception>
#include <typeinfo>
#include <stdexcept>

#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"

#include "StopWatch.h"

// #define PLUGIN_DEBUG 2

#define DRIVER_VERSION      1.0

#define SERIAL_BUFFER_SIZE 256
#define MAX_TIMEOUT 1000
#define LOG_BUFFER_SIZE 256

enum AF3_Errors    {PLUGIN_OK = 0, NOT_CONNECTED, ND_CANT_CONNECT, AF3_BAD_CMD_RESPONSE, COMMAND_FAILED};
enum MotorDir       {NORMAL = 0 , REVERSE};
enum MotorStatus    {IDLE = 0, MOVING};


class CAf3Controller
{
public:
    CAf3Controller();
    ~CAf3Controller();

    int         Connect(const char *pszPort);
    int         Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; };

    void        SetSerxPointer(SerXInterface *p) { m_pSerx = p; };
    void        setSleeper(SleeperInterface *pSleeper) { m_pSleeper = pSleeper; };

	// device settings
	void		resetDevice();
	
    // move commands
    int         haltFocuser();
    int         gotoPosition(int nPos);
    int         moveRelativeToPosision(int nSteps);

    // command complete functions
    int         isGoToComplete(bool &bComplete);
    int         isMotorMoving(bool &bMoving);

    // getter and setter
    void        setDebugLog(bool bEnable) {m_bDebugLog = bEnable; };

    int         getDeviceStatus(int &nStatus);

    int         getFirmwareVersion(char *pszVersion, int nStrMaxLen);
    int         getTemperature(double &dTemperature);
    int         getPosition(int &nPosition);
    int         syncMotorPosition(int nPos);
    int         getPosLimit(int &nLimit);
    int         setPosLimit(int nLimit);
    
    int         getStepSize(int &nStepSize);
    int         setStepSize(int nStepSize);
    
    int         getSpeed(int &nSpeed);
    int         setSpeed(int nSpeed);
    
    int         getMoveCurrentMultiplier(int &nMoveCurrentMultiplier);
    int         setMoveCurrentMultiplier(int nMoveCurrentMultiplier);

    int         getHoldCurrentMultiplier(int &nHoldCurrentMultiplier);
    int         setHoldCurrentMultiplier(int nHoldCurrentMultiplier);

    
    int         getReverseEnable(bool &bEnable);
    int         setReverseEnable(bool bEnable);

    int         getMaxMouvement(int &nMaxMove);
    int         setMaxMouvement(int nMaxMove);
    
protected:

    int             sendCommand(const char *pszCmd, char *pszResult, int nResultMaxLen);
    int             readResponse(char *pszRespBuffer, int nBufferLen);

    SerXInterface   *m_pSerx;
    SleeperInterface    *m_pSleeper;

    bool            m_bDebugLog;
    bool            m_bIsConnected;
    char            m_szFirmwareVersion[SERIAL_BUFFER_SIZE];
    char            m_szLogBuffer[LOG_BUFFER_SIZE];

    int             m_nCurPos;
    int             m_nTargetPos;
    int             m_nPosLimit;
    bool            m_bMoving;
    int             m_nLastPosition;
    
	bool			m_bResetOnConnect;

    CStopWatch      m_MoveTimer;
    
    std::string&    trim(std::string &str, const std::string &filter );
    std::string&    ltrim(std::string &str, const std::string &filter);
    std::string&    rtrim(std::string &str, const std::string &filter);
    std::string     findField(std::vector<std::string> &svFields, const std::string& token);

	
#ifdef PLUGIN_DEBUG
    std::string m_sLogfilePath;
    // timestamp for logs
    char *timestamp;
    time_t ltime;
    FILE *Logfile;      // LogFile

#endif

};

#endif //__AF3__
