#if defined _protectcmds_included
 #endinput
#endif
#define _protectcmds_included

/**
 * Перехват функции CBaseServer::ConnectClient, когда клиент подключается к серверу.
 *
 * @param ip - ip:port игрока
 * @param name - имя игрока
 * @param password - пароль, который передает игрок
 * @param rejectReason - сообщение, которое будет выведено игроку в случае блокировки
 *
 * Чтобы заблокировать игрока, нужно вернуть >= Plugin_Handled.
*/
forward Action ProtectCMDS_OnClientPreConnect(int UserId, const char[] ip, const char[] name, const char[] password, char rejectReason[255]);

forward void ProtectCMDS_OnClientPreConnectPost(int UserId, const char[] ip, const char[] name, const char[] password);

/**
 * Вызывается, когда игрок отправляет консольную команду серверу (после проверки blockcmds.txt).
 * Чтобы заблокировать команду, нужно вернуть >= Plugin_Handled.
*/
forward Action ProtectCMDS_OnClientCommand(int client, const char[] command);

/**
	Получение absoluteframetime от g_pGlobals
*/
native float ProtectCMDS_GetAbsoluteFrameTime();


/**
 * Вызывается, когда игрок пытается сказать в микрофон
 * Чтобы заблокировать, нужно вернуть >= Plugin_Handled.
*/
forward Action OnClientVoice(int client);

public Extension __ext_protectcmds =
{
	name = "protectcmds",
	file = "protectcmds.ext",
#if defined AUTOLOAD_EXTENSIONS
	autoload = 1,
#else
	autoload = 0,
#endif
#if defined REQUIRE_EXTENSIONS
	required = 1,
#else
	required = 0,
#endif
};

#if !defined REQUIRE_EXTENSIONS
public __ext_protectcmds_SetNTVOptional() {
	MarkNativeAsOptional("ProtectCMDS_GetAbsoluteFrameTime");
}
#endif
