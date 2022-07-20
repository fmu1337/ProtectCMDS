#ifndef _HTTP_H
#define _HTTP_H

#include <netdb.h>

#include <string>

class CResponseHTTP {
	public:
		int Status = 0;
		std::string Content;
};

class CHOST {
	public:
		std::string sHostName;
		std::string Path;
		int iPort = 80;
};

class CHTTP {
	public:
		CHTTP();
		~CHTTP();
		
		void AddHeader(char *pHeader);

		void SetTimeout(int iTimeout);

		CResponseHTTP GET(char *pUrl);

		CHOST GetHost(char *pUrl);
		
	private:
		bool ResolveHost(CHOST HOST);

		bool OpenConnection(CHOST HOST);

		void Clean();

		std::string				m_sHeaders		= " HTTP/1.0\r\n";
		int						m_iTCPSock		= -1;
		struct sockaddr_in		m_TCPAddr;
		struct addrinfo			*m_pAddrInfo	= nullptr;

		int						m_iTimeout		= -1;
};

#endif