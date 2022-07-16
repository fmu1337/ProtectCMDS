#ifndef IPROTECTCMDS_H
#define IPROTECTCMDS_H
#ifdef _WIN32
#pragma once
#endif

#define INTERFACE_PROTECTCMDS "IProtectCMDS001"

class IProtectCMDS {
	public:
		virtual void *GetExtensionPtr() =0;
};

#endif // IPROTECTCMDS_H