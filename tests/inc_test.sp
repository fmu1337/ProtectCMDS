// Compiled in CI to check that package/.../protectcmds.inc matches the extension's natives and forwards.
#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <protectcmds>

public Action ProtectCMDS_OnClientPreConnect(int UserId, const char[] ip, const char[] name, const char[] password, char rejectReason[255])
{
	return Plugin_Continue;
}

public void ProtectCMDS_OnClientPreConnectPost(int UserId, const char[] ip, const char[] name, const char[] password)
{
}

public Action ProtectCMDS_OnClientCommand(int client, const char[] command)
{
	return Plugin_Continue;
}

public Action OnClientVoice(int client)
{
	return Plugin_Continue;
}

public void OnPluginStart()
{
	PrintToServer("%f", ProtectCMDS_GetAbsoluteFrameTime());
}
