#ifndef _ROOTCONSOLE_H_
#define _ROOTCONSOLE_H_

#include <IShareSys.h>

// SourceMod's "sm <command>" menu as seen at run time. One build of the extension runs on SourceMod 1.7
// (IRootConsole v2) and on 1.8+ (v3), whose handler interfaces differ, so these declarations follow the
// layout both share and Add() picks the registration call the running SourceMod understands.
namespace PCMDSRootConsole {
	class ICommandArgs {
	public:
		virtual const char *Arg(int n) const = 0;
		virtual int ArgC() const = 0;
		virtual const char *ArgS() const = 0;
	};

	// v3 calls OnRootConsoleCommand (slot 1). v2 calls OnRootConsoleCommand2 (also slot 1, same arguments)
	// for commands added with AddRootConsoleCommand2 and never slot 0.
	class ICommand {
	public:
		virtual unsigned int GetApiVersion() const { return 3; }
		virtual void OnRootConsoleCommand(const char *pCmdName, const ICommandArgs *pArgs) = 0;
	};

	class IRootConsole : public SourceMod::SMInterface {
	public:
		virtual bool AddRootConsoleCommand(const char *pCmd, const char *pText, ICommand *pHandler) = 0;
		virtual bool RemoveRootConsoleCommand(const char *pCmd, ICommand *pHandler) = 0;
		virtual void ConsolePrint(const char *pFormat, ...) = 0;
		virtual void DrawGenericOption(const char *pCmd, const char *pText) = 0;
		virtual bool AddRootConsoleCommand2(const char *pCmd, const char *pText, ICommand *pHandler) = 0;
		// v3 only
		virtual bool AddRootConsoleCommand3(const char *pCmd, const char *pText, ICommand *pHandler) = 0;
	};

	// Adds "sm <pCmd>"; returns the console to print to, or nullptr
	IRootConsole *Add(const char *pCmd, const char *pText, ICommand *pHandler);
	void Remove(const char *pCmd, ICommand *pHandler);
}

#endif
