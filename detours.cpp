#include "cdetour.h"
#include "common.h"
#include "module.h"
#include "addresses.h"
#include "commands.h"
#include "interfaces/cs2_interfaces.h"
#include "detours.h"
#include "ctimer.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "entity/cbasemodelentity.h"
#include "entity/ccsweaponbase.h"

#include "tier0/memdbgon.h"

extern CGlobalVars *gpGlobals;

DECLARE_DETOUR(Host_Say, Detour_Host_Say, &modules::server);
DECLARE_DETOUR(UTIL_SayTextFilter, Detour_UTIL_SayTextFilter, &modules::server);
DECLARE_DETOUR(UTIL_SayText2Filter, Detour_UTIL_SayText2Filter, &modules::server);
DECLARE_DETOUR(VoiceShouldHear, Detour_VoiceShouldHear, &modules::server);
DECLARE_DETOUR(CSoundEmitterSystem_EmitSound, Detour_CSoundEmitterSystem_EmitSound, &modules::server);
DECLARE_DETOUR(CCSWeaponBase_Spawn, Detour_CCSWeaponBase_Spawn, &modules::server);

void FASTCALL Detour_CCSWeaponBase_Spawn(CBaseEntity *pThis, void *a2)
{
	const char *pszClassName = pThis->m_pEntity->m_designerName.String();

	Message("Weapon spawn: %s\n", pszClassName);

	CCSWeaponBase_Spawn(pThis, a2);

	FixWeapon((CCSWeaponBase *)pThis);
}

void FASTCALL Detour_CSoundEmitterSystem_EmitSound(ISoundEmitterSystemBase *pSoundEmitterSystem, CEntityIndex *a2, IRecipientFilter &filter, uint32 a4, void *a5)
{
	//ConMsg("Detour_CSoundEmitterSystem_EmitSound\n");
	CSoundEmitterSystem_EmitSound(pSoundEmitterSystem, a2, filter, a4, a5);
}

bool FASTCALL Detour_VoiceShouldHear(CCSPlayerController *a, CCSPlayerController *b, bool a3, bool voice)
{
	//if (!voice)
	//{
	//	Message("block");
	//	return false;
	//}

	// Message("a: %s, b: %s\n", &a->m_iszPlayerName(), &b->m_iszPlayerName());
	// Message("VoiceShouldHear: %llx %llx %i %i\n", a, b, a3, voice);

	return VoiceShouldHear(a, b, a3, voice);
}

void FASTCALL Detour_UTIL_SayTextFilter(IRecipientFilter &filter, const char *pText, CCSPlayerController *pPlayer, uint64 eMessageType)
{
	int entindex = filter.GetRecipientIndex(0).Get();
	CCSPlayerController *target = (CCSPlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)entindex);

#ifdef _DEBUG
	if (target)
		Message("Chat from %s to %s: %s\n", pPlayer ? &pPlayer->m_iszPlayerName() : "console", &target->m_iszPlayerName(), pText);
#endif

	if (pPlayer)
		return UTIL_SayTextFilter(filter, pText, pPlayer, eMessageType);

	char buf[256];
	V_snprintf(buf, sizeof(buf), "%s %s", " \7CONSOLE:\4", pText + sizeof("Console:"));

	UTIL_SayTextFilter(filter, buf, pPlayer, eMessageType);
}

void FASTCALL Detour_UTIL_SayText2Filter(
	IRecipientFilter &filter,
	CCSPlayerController *pEntity,
	uint64 eMessageType,
	const char *msg_name,
	const char *param1,
	const char *param2,
	const char *param3,
	const char *param4)
{
	int entindex = filter.GetRecipientIndex(0).Get() + 1;
	CCSPlayerController *target = (CCSPlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)entindex);

#ifdef _DEBUG
	if (target)
		Message("Chat from %s to %s: %s\n", param1, &target->m_iszPlayerName(), param2);
#endif

	UTIL_SayText2Filter(filter, pEntity, eMessageType, msg_name, param1, param2, param3, param4);
}

void FASTCALL Detour_Host_Say(CCSPlayerController *pController, CCommand *args, bool teamonly, int unk1, const char *unk2)
{
	Host_Say(pController, args, teamonly, unk1, unk2);

	if (g_pEntitySystem == nullptr)
		g_pEntitySystem = interfaces::pGameResourceServiceServer->GetGameEntitySystem();

	const char *pMessage = args->Arg(1);

	if (*pMessage == '!' || *pMessage == '/')
		ParseChatCommand(pMessage, pController);

	if (*pMessage == '/')
		return;
}

void Detour_Log()
{
	return;
}

bool FASTCALL Detour_IsChannelEnabled(LoggingChannelID_t channelID, LoggingSeverity_t severity)
{
	return false;
}

CDetour<decltype(Detour_Log)> g_LoggingDetours[] =
{
	CDetour<decltype(Detour_Log)>(&modules::tier0, Detour_Log, "Msg" ),
	//CDetour<decltype(Detour_Log)>( modules::tier0, Detour_Log, "?ConMsg@@YAXPEBDZZ" ),
	//CDetour<decltype(Detour_Log)>( modules::tier0, Detour_Log, "?ConColorMsg@@YAXAEBVColor@@PEBDZZ" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "ConDMsg" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "DevMsg" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "Warning" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "DevWarning" ),
	//CDetour<decltype(Detour_Log)>( modules::tier0, Detour_Log, "?DevWarning@@YAXPEBDZZ" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "LoggingSystem_Log" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "LoggingSystem_LogDirect" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "LoggingSystem_LogAssert" ),
	//CDetour<decltype(Detour_Log)>( modules::tier0, Detour_IsChannelEnabled, "LoggingSystem_IsChannelEnabled" ),
};

void ToggleLogs()
{
	static bool bBlock = false;

	if (!bBlock)
	{
		for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
			g_LoggingDetours[i].EnableDetour();
	}
	else
	{
		for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
			g_LoggingDetours[i].DisableDetour();
	}

	bBlock = !bBlock;
}

CUtlVector<CDetourBase *> g_vecDetours;

void InitDetours()
{
	g_vecDetours.RemoveAll();

	for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
		g_LoggingDetours[i].CreateDetour();

	UTIL_SayTextFilter.CreateDetour();
	UTIL_SayTextFilter.EnableDetour();

	UTIL_SayText2Filter.CreateDetour();
	UTIL_SayText2Filter.EnableDetour();

	Host_Say.CreateDetour();
	Host_Say.EnableDetour();

	VoiceShouldHear.CreateDetour();
	VoiceShouldHear.EnableDetour();

	CSoundEmitterSystem_EmitSound.CreateDetour();
	CSoundEmitterSystem_EmitSound.EnableDetour();

	CCSWeaponBase_Spawn.CreateDetour();
	CCSWeaponBase_Spawn.EnableDetour();
}

void FlushAllDetours()
{
	FOR_EACH_VEC(g_vecDetours, i)
	{
		Message("Removing detour %s\n", g_vecDetours[i]->GetName());
		g_vecDetours[i]->FreeDetour();
	}

	g_vecDetours.RemoveAll();
}
