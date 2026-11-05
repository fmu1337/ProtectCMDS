#include "smsdk_ext.h"
#include "rootconsole.h"

namespace PCMDSRootConsole {
	static IRootConsole *g_pRootConsole = nullptr;

	IRootConsole *Add(const char *pCmd, const char *pText, ICommand *pHandler) {
		SourceMod::SMInterface *pIface = nullptr;
		// v2 is what both 1.7 and 1.8+ accept; the version it reports decides the call
		if (!sharesys->RequestInterface("IRootConsole", 2, myself, &pIface) || !pIface) return nullptr;

		IRootConsole *pRootConsole = static_cast<IRootConsole *>(pIface);
		bool bAdded = pRootConsole->GetInterfaceVersion() >= 3
			? pRootConsole->AddRootConsoleCommand3(pCmd, pText, pHandler)
			: pRootConsole->AddRootConsoleCommand2(pCmd, pText, pHandler);

		if (!bAdded) return nullptr;

		g_pRootConsole = pRootConsole;

		return pRootConsole;
	}

	void Remove(const char *pCmd, ICommand *pHandler) {
		if (!g_pRootConsole) return;

		g_pRootConsole->RemoveRootConsoleCommand(pCmd, pHandler);
		g_pRootConsole = nullptr;
	}
}
