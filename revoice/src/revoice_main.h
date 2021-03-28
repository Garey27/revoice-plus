#pragma once

#include "revoice_shared.h"
#include "revoice_player.h"

bool Revoice_Load();
void Revoice_Main_DeInit();
bool Revoice_Main_Init();

void CvarValue2_PreHook(const edict_t *pEnt, int requestID, const char *cvarName, const char *cvarValue);
qboolean ClientConnect_PostHook(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128]);
void OnClientCommandReceiving(edict_t* pClient);
void ServerActivate_PostHook(edict_t *pEdictList, int edictCount, int clientMax);
void ServerDeactivate_PostHook();
void StartFrame_PostHook();
void PlayerPreThink(edict_t* pEntity);


