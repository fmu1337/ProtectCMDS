#ifndef _RATELIMIT_H
#define _RATELIMIT_H

#include <helpers/symbolmanager.h>
#include <tier1/strtools.h>

double *g_pNet_time = nullptr;

class CRateLimit
{
public:
	CRateLimit()
	{
		Init();
	};

	CRateLimit(unsigned int iMaxPerSec, double fResetDelay = 1.0, const char *pDebugName = "unknown")
	{
		Init();
		
		m_nMaxPerSec = iMaxPerSec;
		m_flResetDelay = fResetDelay;
		Reset();
 		SetDebugName(pDebugName);
 	}

	void SetLimit(unsigned int iMaxPerSec, double fResetDelay = 1.0, const char *pDebugName = "unknown")
	{
		Init();
		
		m_nMaxPerSec = iMaxPerSec;
		m_flResetDelay = fResetDelay;
		Reset();
 		SetDebugName(pDebugName);
	}

public:

	void Init()
	{
		if (g_pNet_time) return;
		
		if (!g_pSymbolManager) return;
		
		g_pNet_time = reinterpret_cast<double *>(g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "net_time"));
		if (!g_pNet_time) {
			g_pSM->LogError(myself, "Failed to search net_time");
			
			return;
		}
	}

	void SetDebugName(const char* name)
	{
		Q_strncpy(m_szDebugName, name, sizeof(m_szDebugName));
	}

	inline double Now()
	{
		return *g_pNet_time;
	}

	void Reset()
	{
		m_flNextResetTime = Now() + m_flResetDelay;
		m_nCounter = 0;
		m_bShowMsg = false;
	}

	bool Check(unsigned int iCounter = 1)
	{
		if ( m_nMaxPerSec == 0 ) return true;

		if ( m_flNextResetTime < Now() ) Reset();
		
		m_nCounter += iCounter;

		bool bResult = m_nCounter <= m_nMaxPerSec;
		
		if( !bResult && !m_bShowMsg )
		{
			Msg("Rate limit \"%s\" exceeded (counter %u, mps %u, reset %f)\n", m_szDebugName, m_nCounter, m_nMaxPerSec, m_flResetDelay);
			
			m_bShowMsg = true;
		}

		return bResult;
	}

private:
	double m_flNextResetTime;
	double m_flResetDelay;

	unsigned int m_nCounter;
	unsigned int m_nMaxPerSec;
	
	bool m_bShowMsg;

	char m_szDebugName[64];
};

#endif // !_RATELIMIT_H