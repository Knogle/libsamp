#define SAMP_COMPAT
#include <open.mp>

#define OP_MODE_NAME ("Object 0.3.7 probe")
#define OP_OBJECT_COUNT (18)
#define OP_PLAYER_OBJECT_COUNT (6)
#define OP_GATE_LEFT (16)
#define OP_GATE_RIGHT (17)

static gObjectProbeObjects[OP_OBJECT_COUNT];
static gPlayerProbeObjects[MAX_PLAYERS][OP_PLAYER_OBJECT_COUNT];
static bool:gObjectProbeBuilt;
static bool:gGateOpen;
static bool:gMaterialVariant;

static const Float:SPAWN_X = 1958.3783;
static const Float:SPAWN_Y = 1343.1572;
static const Float:SPAWN_Z = 15.3746;
static const Float:SPAWN_A = 270.0;

static const gProbeModels[OP_OBJECT_COUNT] = {
	19316, 19894, 18997, 19425, 18842, 18818,
	19909, 19907, 19905, 18777, 19005, 19378,
	19313, 19333, 19649, 11692, 985, 986
};

static const Float:gProbeBaseX[OP_OBJECT_COUNT] = {
	1944.0, 1952.0, 1960.0, 1968.0, 1976.0, 1984.0,
	1944.0, 1952.0, 1960.0, 1968.0, 1976.0, 1984.0,
	1944.0, 1952.0, 1960.0, 1968.0, 1953.0, 1965.0
};

static const Float:gProbeBaseY[OP_OBJECT_COUNT] = {
	1360.0, 1360.0, 1360.0, 1360.0, 1360.0, 1360.0,
	1368.0, 1368.0, 1368.0, 1368.0, 1368.0, 1368.0,
	1376.0, 1376.0, 1376.0, 1376.0, 1386.0, 1386.0
};

static const Float:gProbeBaseZ[OP_OBJECT_COUNT] = {
	14.8, 14.8, 14.8, 14.8, 14.8, 14.8,
	14.8, 14.8, 14.8, 14.8, 14.8, 14.8,
	14.8, 14.8, 14.8, 14.8, 14.8, 14.8
};

static const Float:gProbeRotZ[OP_OBJECT_COUNT] = {
	0.0, 30.0, 60.0, 90.0, 120.0, 150.0,
	180.0, 210.0, 240.0, 270.0, 300.0, 330.0,
	20.0, 70.0, 120.0, 170.0, 90.0, 270.0
};

static const gPlayerProbeModels[OP_PLAYER_OBJECT_COUNT] = {
	19316, 19894, 18842, 19425, 19909, 18784
};

main()
{
}

stock ResetObjectProbeSlots()
{
	for (new i = 0; i < OP_OBJECT_COUNT; i++)
	{
		gObjectProbeObjects[i] = INVALID_OBJECT_ID;
	}
	gObjectProbeBuilt = false;
	gGateOpen = false;
	gMaterialVariant = false;
	return 1;
}

stock ResetPlayerObjectProbeSlots(playerid)
{
	for (new i = 0; i < OP_PLAYER_OBJECT_COUNT; i++)
	{
		gPlayerProbeObjects[playerid][i] = INVALID_OBJECT_ID;
	}
	return 1;
}

stock DestroyObjectProbe()
{
	for (new i = 0; i < OP_OBJECT_COUNT; i++)
	{
		if (gObjectProbeObjects[i] != INVALID_OBJECT_ID && IsValidObject(gObjectProbeObjects[i]))
		{
			DestroyObject(gObjectProbeObjects[i]);
		}
	}
	ResetObjectProbeSlots();
	print("[oprobe] global objects destroyed");
	return 1;
}

stock DestroyPlayerObjectProbe(playerid)
{
	for (new i = 0; i < OP_PLAYER_OBJECT_COUNT; i++)
	{
		if (gPlayerProbeObjects[playerid][i] != INVALID_OBJECT_ID)
		{
			DestroyPlayerObject(playerid, gPlayerProbeObjects[playerid][i]);
		}
	}
	ResetPlayerObjectProbeSlots(playerid);
	printf("[oprobe] player objects destroyed player=%d", playerid);
	return 1;
}

stock BuildObjectProbe()
{
	if (gObjectProbeBuilt)
	{
		DestroyObjectProbe();
	}

	for (new i = 0; i < OP_OBJECT_COUNT; i++)
	{
		gObjectProbeObjects[i] = CreateObject(gProbeModels[i], gProbeBaseX[i], gProbeBaseY[i], gProbeBaseZ[i],
			0.0, 0.0, gProbeRotZ[i], 220.0);
		printf("[oprobe] create global slot=%d object=%d model=%d pos=%.3f %.3f %.3f rotz=%.3f",
			i, gObjectProbeObjects[i], gProbeModels[i], gProbeBaseX[i], gProbeBaseY[i], gProbeBaseZ[i], gProbeRotZ[i]);
	}

	if (gObjectProbeObjects[2] != INVALID_OBJECT_ID)
	{
		SetObjectMaterialText(gObjectProbeObjects[2], "OBSERVED_037", 0, OBJECT_MATERIAL_SIZE_256x128,
			"Arial", 24, true, 0xFFFFFFFF, 0x334455AA, OBJECT_MATERIAL_TEXT_ALIGN_CENT);
	}
	if (gObjectProbeObjects[3] != INVALID_OBJECT_ID)
	{
		SetObjectNoCameraCol(gObjectProbeObjects[3]);
	}

	gObjectProbeBuilt = true;
	print("[oprobe] global object probe built");
	return 1;
}

stock BuildPlayerObjectProbe(playerid)
{
	DestroyPlayerObjectProbe(playerid);

	for (new i = 0; i < OP_PLAYER_OBJECT_COUNT; i++)
	{
		new Float:x = 1944.0 + float(i * 8);
		new Float:y = 1398.0;
		new Float:z = 14.8;
		gPlayerProbeObjects[playerid][i] = CreatePlayerObject(playerid, gPlayerProbeModels[i], x, y, z,
			0.0, 0.0, float(i * 45), 220.0);
		printf("[oprobe] create player player=%d slot=%d object=%d model=%d pos=%.3f %.3f %.3f",
			playerid, i, gPlayerProbeObjects[playerid][i], gPlayerProbeModels[i], x, y, z);
	}
	return 1;
}

stock ToggleObjectProbeMaterials(playerid)
{
	if (!gObjectProbeBuilt)
	{
		BuildObjectProbe();
	}

	gMaterialVariant = !gMaterialVariant;
	if (gObjectProbeObjects[0] != INVALID_OBJECT_ID)
	{
		SetObjectMaterial(gObjectProbeObjects[0], 0, 18646, gMaterialVariant ? "matcolours" : "none",
			gMaterialVariant ? "white" : "none", gMaterialVariant ? 0xFFFFFFFF : 0);
	}
	if (gObjectProbeObjects[2] != INVALID_OBJECT_ID)
	{
		SetObjectMaterialText(gObjectProbeObjects[2], gMaterialVariant ? "VARIANT_A" : "VARIANT_B", 0,
			OBJECT_MATERIAL_SIZE_256x128, "Arial", 24, true,
			gMaterialVariant ? 0xFFCC66FF : 0x66CCFFFF,
			gMaterialVariant ? 0x223344AA : 0x442211AA, OBJECT_MATERIAL_TEXT_ALIGN_CENT);
	}

	printf("[oprobe] material toggle player=%d variant=%d", playerid, gMaterialVariant);
	SendClientMessage(playerid, 0x66CCFFFF, "[oprobe] material/text-material update sent.");
	return 1;
}

stock ToggleObjectProbeGates(playerid)
{
	if (!gObjectProbeBuilt)
	{
		BuildObjectProbe();
	}

	gGateOpen = !gGateOpen;
	new Float:leftX = gProbeBaseX[OP_GATE_LEFT] + (gGateOpen ? -5.0 : 0.0);
	new Float:rightX = gProbeBaseX[OP_GATE_RIGHT] + (gGateOpen ? 5.0 : 0.0);
	new Float:gateZ = gProbeBaseZ[OP_GATE_LEFT] + (gGateOpen ? 0.8 : 0.0);

	MoveObject(gObjectProbeObjects[OP_GATE_LEFT], leftX, gProbeBaseY[OP_GATE_LEFT], gateZ, 1.75, 0.0, 0.0, 90.0);
	MoveObject(gObjectProbeObjects[OP_GATE_RIGHT], rightX, gProbeBaseY[OP_GATE_RIGHT], gateZ, 1.75, 0.0, 0.0, 270.0);

	printf("[oprobe] move gates player=%d open=%d left=%d right=%d left_target=%.3f %.3f %.3f right_target=%.3f %.3f %.3f",
		playerid, gGateOpen, gObjectProbeObjects[OP_GATE_LEFT], gObjectProbeObjects[OP_GATE_RIGHT],
		leftX, gProbeBaseY[OP_GATE_LEFT], gateZ, rightX, gProbeBaseY[OP_GATE_RIGHT], gateZ);
	SendClientMessage(playerid, 0x66CCFFFF, "[oprobe] MoveObject gate toggle sent.");
	return 1;
}

stock StopObjectProbeGates(playerid)
{
	if (gObjectProbeObjects[OP_GATE_LEFT] != INVALID_OBJECT_ID)
	{
		StopObject(gObjectProbeObjects[OP_GATE_LEFT]);
	}
	if (gObjectProbeObjects[OP_GATE_RIGHT] != INVALID_OBJECT_ID)
	{
		StopObject(gObjectProbeObjects[OP_GATE_RIGHT]);
	}
	printf("[oprobe] stop gates player=%d", playerid);
	SendClientMessage(playerid, 0xFFCC66FF, "[oprobe] StopObject sent for both gates.");
	return 1;
}

stock ResetObjectProbeGates(playerid)
{
	if (!gObjectProbeBuilt)
	{
		BuildObjectProbe();
	}
	gGateOpen = false;
	MoveObject(gObjectProbeObjects[OP_GATE_LEFT], gProbeBaseX[OP_GATE_LEFT], gProbeBaseY[OP_GATE_LEFT],
		gProbeBaseZ[OP_GATE_LEFT], 2.25, 0.0, 0.0, 90.0);
	MoveObject(gObjectProbeObjects[OP_GATE_RIGHT], gProbeBaseX[OP_GATE_RIGHT], gProbeBaseY[OP_GATE_RIGHT],
		gProbeBaseZ[OP_GATE_RIGHT], 2.25, 0.0, 0.0, 270.0);
	printf("[oprobe] reset gates player=%d", playerid);
	SendClientMessage(playerid, 0x66FF66FF, "[oprobe] gates reset.");
	return 1;
}

stock SendObjectProbeHelp(playerid)
{
	SendClientMessage(playerid, 0xFFFFFFFF, "[oprobe] /oprobe /opdestroy /opplayer /opmove /opstop /opreset /opmat /ophelp");
	SendClientMessage(playerid, 0xFFFFFFFF, "[oprobe] Watch samp_probe.log gta_object_info lines after enabling object_info + gta_asset_hooks.");
	return 1;
}

stock SetupObjectProbePlayer(playerid)
{
	SetPlayerInterior(playerid, 0);
	SetPlayerVirtualWorld(playerid, 0);
	SetPlayerPos(playerid, SPAWN_X, SPAWN_Y, SPAWN_Z);
	SetPlayerFacingAngle(playerid, SPAWN_A);
	SetCameraBehindPlayer(playerid);
	TogglePlayerClock(playerid, false);
	BuildObjectProbe();
	BuildPlayerObjectProbe(playerid);
	return 1;
}

public OnGameModeInit()
{
	SetGameModeText(OP_MODE_NAME);
	ShowPlayerMarkers(PLAYER_MARKERS_MODE_GLOBAL);
	ShowNameTags(true);
	AddPlayerClass(0, SPAWN_X, SPAWN_Y, SPAWN_Z, SPAWN_A, 0, 0, 0, 0, 0, 0);
	ResetObjectProbeSlots();
	print("\n----------------------------------");
	print("  Object 0.3.7 probe");
	print("----------------------------------\n");
	return 1;
}

public OnGameModeExit()
{
	DestroyObjectProbe();
	return 1;
}

public OnPlayerConnect(playerid)
{
	ResetPlayerObjectProbeSlots(playerid);
	SendObjectProbeHelp(playerid);
	return 1;
}

public OnPlayerDisconnect(playerid, reason)
{
	#pragma unused reason

	DestroyPlayerObjectProbe(playerid);
	return 1;
}

public OnPlayerSpawn(playerid)
{
	SetupObjectProbePlayer(playerid);
	return 1;
}

public OnObjectMoved(objectid)
{
	printf("[oprobe] object moved object=%d left=%d right=%d open=%d",
		objectid, gObjectProbeObjects[OP_GATE_LEFT], gObjectProbeObjects[OP_GATE_RIGHT], gGateOpen);
	return 1;
}

public OnPlayerCommandText(playerid, cmdtext[])
{
	if (!strcmp(cmdtext, "/ophelp", true))
	{
		SendObjectProbeHelp(playerid);
		return 1;
	}
	if (!strcmp(cmdtext, "/oprobe", true))
	{
		BuildObjectProbe();
		BuildPlayerObjectProbe(playerid);
		SendClientMessage(playerid, 0x66FF66FF, "[oprobe] object probe rebuilt.");
		return 1;
	}
	if (!strcmp(cmdtext, "/opdestroy", true))
	{
		DestroyObjectProbe();
		DestroyPlayerObjectProbe(playerid);
		SendClientMessage(playerid, 0xFFCC66FF, "[oprobe] all probe objects destroyed.");
		return 1;
	}
	if (!strcmp(cmdtext, "/opplayer", true))
	{
		BuildPlayerObjectProbe(playerid);
		SendClientMessage(playerid, 0x66FF66FF, "[oprobe] player objects rebuilt.");
		return 1;
	}
	if (!strcmp(cmdtext, "/opmove", true))
	{
		ToggleObjectProbeGates(playerid);
		return 1;
	}
	if (!strcmp(cmdtext, "/opstop", true))
	{
		StopObjectProbeGates(playerid);
		return 1;
	}
	if (!strcmp(cmdtext, "/opreset", true))
	{
		ResetObjectProbeGates(playerid);
		return 1;
	}
	if (!strcmp(cmdtext, "/opmat", true))
	{
		ToggleObjectProbeMaterials(playerid);
		return 1;
	}
	return 0;
}
