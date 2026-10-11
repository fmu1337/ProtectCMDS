#ifndef _HTTP_H
#define _HTTP_H

#include <string>

class CURL {
	public:
		bool Parse(const std::string &sUrl);

		bool bHTTPS = false;
		std::string sHostName;
		std::string Path;
		int iPort = 80;
};

class CResponseHTTP {
	public:
		int Status = 0;
		std::string Location;
		std::string Content;
};

class CHTTP {
	public:
		CHTTP();
		~CHTTP();

		void SetTimeout(int iTimeout);
		void SetUserAgent(const char *pUserAgent);

		//Follows redirects, returns true only on "200 OK"
		bool GET(const std::string &sUrl, std::string &sContent);

		const std::string &GetError() { return m_sError; }

	private:
		bool Request(const CURL &URL, CResponseHTTP &Response);

		bool OpenConnection(const CURL &URL);
		bool ParseResponse(const std::string &sAnswer, CResponseHTTP &Response);

		void Clean();

		int						m_iTCPSock		= -1;
		int						m_iTimeout		= 10000;
		std::string				m_sUserAgent	= "PCMDS_Updater";
		std::string				m_sError;
};

#endif