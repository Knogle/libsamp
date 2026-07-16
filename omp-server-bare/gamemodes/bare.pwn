#include <open.mp>

#define GAME_MODE_NAME ("Bare vehicle/object stream test")
#define WELCOME_GAME_TEXT ("~w~open.mp: ~g~Vehicle/Object Stream Test")

static const PLAYER_TEAM = 0;
static const gPlayerSkins[] = {
	0,   // CJ
	7,   // Casino worker
	19,  // Beach visitor
	21,  // Striped shirt
	29,  // Biker
	46,  // Busy woman
	60,  // Businessman
	101, // Balla
	170, // Elvis
	217, // Staff
	250, // Biker
	287  // Army
};

static const Float:SPAWN_X = 306.0034;
static const Float:SPAWN_Y = 2042.9145;
static const Float:SPAWN_Z = 16.6400;
static const Float:SPAWN_ANGLE = 0.0000;

static const WEAPON:TEST_PRIMARY_WEAPON = WEAPON_M4;
static const TEST_PRIMARY_AMMO = 600;
static const WEAPON:TEST_SECONDARY_WEAPON = WEAPON_DEAGLE;
static const TEST_SECONDARY_AMMO = 120;
static const WEAPON:TEST_MELEE_WEAPON = WEAPON_KNIFE;
static const TEST_MELEE_AMMO = 1;
static const Float:DEFAULT_GRAVITY = 0.008;
static const Float:SMALL_RPC_TEST_GRAVITY = 0.002;

// Live-checked 2026-07-16: HTTP 200, Content-Type audio/mpeg, 128 kbit/s stereo.
static const gRpcAudioTestUrl[] = "http://stream.radioparadise.com/mp3-128";
static const Float:RPC_AUDIO_TEST_OFFSET_X = 5.0;
static const Float:RPC_AUDIO_TEST_RADIUS = 30.0;

#define RPC_MAP_ICON_SLOT_COUNT (100)
#define RPC_MAP_ICON_STYLE_COUNT (4)
#define RPC_MAP_ICON_SLOTS_PER_STYLE (25)
#define RPC_MAP_ICON_GRID_WIDTH (5)
#define RPC_MAP_ICON_TICK_MS (1000)
static const Float:RPC_MAP_ICON_GRID_SPACING = 4.0;
static const Float:RPC_MAP_ICON_CLUSTER_DISTANCE = 32.0;

// Safe radar sprites: marker types 1, 2, 4 and 56 are intentionally omitted
// because the official documentation warns that they can crash the map legend.
static const gRpcMapIconMarkerTypes[RPC_MAP_ICON_SLOTS_PER_STYLE] = {
	0, 3,
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27
};

static const gRpcMapIconColours[RPC_MAP_ICON_STYLE_COUNT] = {
	0xFF4040FF, // Local: red
	0x40FF40FF, // Global: green
	0x4080FFFF, // Local checkpoint: blue
	0xFFD040FF  // Global checkpoint: yellow
};

static const CLASS_SELECTION_INTERIOR = 14;
static const Float:CLASS_SELECTION_X = 258.4893;
static const Float:CLASS_SELECTION_Y = -41.4008;
static const Float:CLASS_SELECTION_Z = 1002.0234;
static const Float:CLASS_SELECTION_ANGLE = 270.0;

static const Float:CLASS_CAMERA_X = 256.0815;
static const Float:CLASS_CAMERA_Y = -43.0475;
static const Float:CLASS_CAMERA_Z = 1004.0234;

#define TEST_VEHICLE_COUNT (4)
#define A51_BUILDING_COUNT (7)
#define SPAWN_CUSTOM_OBJECT_COUNT (8)
#define SAMP_OBJECT_SCAN_TOTAL (1433)
#define SAMP_OBJECT_SCAN_DEFAULT_HOLD_MS (2000)
#define SAMP_OBJECT_SCAN_DEFAULT_GAP_MS (1000)
#define SAMP_OBJECT_SCAN_MIN_DELAY_MS (100)
#define SAMP_OBJECT_SCAN_MAX_DELAY_MS (10000)
#define SAMP_OBJECT_SCAN_PHASE_HOLD (0)
#define SAMP_OBJECT_SCAN_PHASE_GAP (1)

forward SampObjectScanTick();
forward RpcMapIconBatchTick();

static gVehicleIds[TEST_VEHICLE_COUNT];
static gSpawnCustomObjects[SPAWN_CUSTOM_OBJECT_COUNT];
static gA51LandObject = INVALID_OBJECT_ID;
static gA51FenceObject = INVALID_OBJECT_ID;
static gA51BuildingObjects[A51_BUILDING_COUNT];
static gA51NorthernGate = INVALID_OBJECT_ID;
static gA51EasternGate = INVALID_OBJECT_ID;
static Text3D:gA51GateLabels[2];
static bool:gA51NorthernGateOpen = false;
static bool:gA51EasternGateOpen = false;
static bool:gSampObjectScanActive = false;
static gSampObjectScanPlayer = INVALID_PLAYER_ID;
static gSampObjectScanIndex = 0;
static gSampObjectScanObject = INVALID_OBJECT_ID;
static gSampObjectScanTimer = 0;
static gSampObjectScanHoldMs = SAMP_OBJECT_SCAN_DEFAULT_HOLD_MS;
static gSampObjectScanGapMs = SAMP_OBJECT_SCAN_DEFAULT_GAP_MS;
static gSampObjectScanPhase = SAMP_OBJECT_SCAN_PHASE_HOLD;
static gRpcTestPickup = -1;
static gRpcTestCab = INVALID_VEHICLE_ID;
static gRpcTestTrailer = INVALID_VEHICLE_ID;
static gRpcTestModVehicle = INVALID_VEHICLE_ID;
static bool:gRpcVehicleParamsEnabled = false;
static bool:gRpcVehicleObjectiveEnabled = false;
static bool:gRpcBoundsEnabled = false;
static bool:gRpcSpecialEnabled = false;
static bool:gRpcRenameAlternate = false;
static gRpcCameraObject = INVALID_OBJECT_ID;
static gRpcEditObject = INVALID_OBJECT_ID;
static gRpcAttachedObject = INVALID_OBJECT_ID;
static gRpcAttachedObjectPlayer = INVALID_PLAYER_ID;
static bool:gRpcShopEnabled[MAX_PLAYERS];
static bool:gRpcColorAlternate[MAX_PLAYERS];
static bool:gRpcMapIconBatchActive = false;
static gRpcMapIconBatchSource = INVALID_PLAYER_ID;
static gRpcMapIconBatchTimer = 0;
static gRpcMapIconBatchTickCount = 0;

static const gVehicleModels[TEST_VEHICLE_COUNT] = {
	411, // Infernus
	411, // Infernus
	411, // Infernus
	411  // Infernus
};

static const Float:gVehicleSpawns[TEST_VEHICLE_COUNT][4] = {
	{299.7166, 2024.0541, 16.6400, 0.0},
	{298.1449, 2004.6697, 16.6400, 0.0},
	{297.0971, 1984.7615, 16.6400, 0.0},
	{295.5254, 1967.4728, 16.6400, 0.0}
};

static const gSpawnCustomModelIds[SPAWN_CUSTOM_OBJECT_COUNT] = {
	11692, // A51LandBit1
	18808, // Tube50m1
	18809, // Tube50mGlass1
	19074, // Cage20mx20mx10mv2
	19313, // a51fensin
	19905, // A51Building1
	19907, // A51Building2
	19909  // A51Building3
};

static const Float:gSpawnCustomObjectSpawns[SPAWN_CUSTOM_OBJECT_COUNT][6] = {
	{335.0034, 2043.9145, 16.0500, 0.0, 0.0, 0.0},
	{315.0034, 2065.9145, 23.3000, 0.0, 90.0, 0.0},
	{295.0034, 2065.9145, 23.3000, 0.0, 90.0, 0.0},
	{306.0034, 2079.9145, 21.6400, 0.0, 0.0, 45.0},
	{331.0034, 2030.9145, 20.0000, 0.0, 0.0, 90.0},
	{353.0034, 2027.9145, 16.4500, 0.0, 0.0, 180.0},
	{260.0034, 2038.9145, 16.5500, 0.0, 0.0, 0.0},
	{268.0034, 2066.9145, 16.8500, 0.0, 0.0, 270.0}
};

stock SkipCommandSpaces(const text[], pos)
{
	while (text[pos] == ' ' || text[pos] == '\t')
	{
		pos++;
	}
	return pos;
}

stock NextCommandToken(const text[], pos)
{
	while (text[pos] != '\0' && text[pos] != ' ' && text[pos] != '\t')
	{
		pos++;
	}
	return SkipCommandSpaces(text, pos);
}

stock bool:IsTestVehicleSlot(slot)
{
	return slot >= 0 && slot < TEST_VEHICLE_COUNT && gVehicleIds[slot] != INVALID_VEHICLE_ID;
}

stock SendVehicleHealthLine(playerid, slot)
{
	new message[144];
	new Float:health = 0.0;

	if (!IsTestVehicleSlot(slot))
	{
		format(message, sizeof(message), "[bare-vtest] vehicle slot %d is invalid.", slot);
		SendClientMessage(playerid, 0xFF6666FF, message);
		return 0;
	}

	if (!GetVehicleHealth(gVehicleIds[slot], health))
	{
		format(message, sizeof(message), "[bare-vtest] vehicle[%d] id=%d GetVehicleHealth failed.", slot, gVehicleIds[slot]);
		SendClientMessage(playerid, 0xFF6666FF, message);
		return 0;
	}

	format(message, sizeof(message), "[bare-vtest] vehicle[%d] id=%d server health %.3f", slot, gVehicleIds[slot], health);
	SendClientMessage(playerid, 0x66FF66FF, message);
	printf("[bare-vtest] vehicle[%d] id=%d server health %.3f", slot, gVehicleIds[slot], health);
	return 1;
}

stock SendVehicleHealthSummary(playerid)
{
	SendClientMessage(playerid, 0xFFFFFFFF, "[bare-vtest] Server vehicle health:");
	for (new i = 0; i < TEST_VEHICLE_COUNT; i++)
	{
		SendVehicleHealthLine(playerid, i);
	}
	SendClientMessage(playerid, 0xFFFFFFFF, "[bare-vtest] Commands: /vhealth [slot health], /vhdmg <slot> <amount>, /vhfix <slot>, /vhkill <slot>");
}

stock SetTestVehicleHealth(playerid, slot, Float:health, const reason[])
{
	new message[144];

	if (!IsTestVehicleSlot(slot))
	{
		format(message, sizeof(message), "[bare-vtest] vehicle slot %d is invalid.", slot);
		SendClientMessage(playerid, 0xFF6666FF, message);
		return 0;
	}

	if (!SetVehicleHealth(gVehicleIds[slot], health))
	{
		format(message, sizeof(message), "[bare-vtest] SetVehicleHealth failed for vehicle[%d] id=%d.", slot, gVehicleIds[slot]);
		SendClientMessage(playerid, 0xFF6666FF, message);
		return 0;
	}

	format(message, sizeof(message), "[bare-vtest] %s vehicle[%d] id=%d health %.3f", reason, slot, gVehicleIds[slot], health);
	SendClientMessage(playerid, 0x66FF66FF, message);
	printf("[bare-vtest] %s vehicle[%d] id=%d health %.3f", reason, slot, gVehicleIds[slot], health);
	return 1;
}

/// Applies the two small client-RPC compatibility probes.
/// References: https://open.mp/docs/scripting/functions/SetPlayerWantedLevel
///             https://open.mp/docs/scripting/functions/SetGravity
stock ApplySmallRpcTest(playerid)
{
	SetPlayerWantedLevel(playerid, 6);
	SetGravity(SMALL_RPC_TEST_GRAVITY);
	SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Wanted=6, gravity=0.002 applied. Jump or drive to verify gravity.");
	SendClientMessage(playerid, 0xFFFFFFFF, "[bare-rpctest] Use /smallrpcreset to restore wanted=0 and gravity=0.008.");
	printf("[bare-rpctest] player=%d applied wanted=6 gravity=%.3f", playerid, SMALL_RPC_TEST_GRAVITY);
	return 1;
}

/// Restores the normal values after the small client-RPC probes.
/// References: https://open.mp/docs/scripting/functions/SetPlayerWantedLevel
///             https://open.mp/docs/scripting/functions/SetGravity
stock ResetSmallRpcTest(playerid)
{
	SetPlayerWantedLevel(playerid, 0);
	SetGravity(DEFAULT_GRAVITY);
	SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Wanted=0 and gravity=0.008 restored.");
	printf("[bare-rpctest] player=%d reset wanted=0 gravity=%.3f", playerid, DEFAULT_GRAVITY);
	return 1;
}

/// Lists the focused commands for RPCs 28, 33, 41, 42, 59, 69, 72, 75, 137 and 138.
stock SendPlayerAudioChatRpcHelp(playerid)
{
	SendClientMessage(playerid, 0xFFFFFFFF, "[bare-rpctest] /rpcaudiotest, /rpcaudiotestpos, /rpcstopaudio (fixed direct MP3 test)");
	SendClientMessage(playerid, 0xFFFFFFFF, "[bare-rpctest] /rpcedit, /rpccanceledit, /rpcshop, /rpcaudio <url>, /rpcaudiopos <url>");
	SendClientMessage(playerid, 0xFFFFFFFF, "[bare-rpctest] /rpcbubble <other player id>, /rpcteam <0-255>, /rpccolor, /rpcattach, /rpcattachoff");
	SendClientMessage(playerid, 0xFFFFFFFF, "[bare-rpctest] /rpcmapicons starts the 100-slot moving beacon; /rpcmapiconsoff removes it.");
	SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] RPC137/138 join/quit are automatic; RPC59 needs a second client/NPC as the visible target.");
	return 1;
}

/// Removes all 100 RPC map-icon slots from one observer.
/// Reference: https://open.mp/docs/scripting/functions/RemovePlayerMapIcon
stock RemoveRpcMapIconBatchForPlayer(playerid)
{
	for (new slot = 0; slot < RPC_MAP_ICON_SLOT_COUNT; slot++)
	{
		RemovePlayerMapIcon(playerid, slot);
	}
	return 1;
}

/// Removes the active batch from every connected observer and stops its timer.
/// References: https://open.mp/docs/scripting/functions/RemovePlayerMapIcon
///             https://open.mp/docs/scripting/functions/KillTimer
stock StopRpcMapIconBatch(playerid, bool:quiet)
{
	new bool:wasActive = gRpcMapIconBatchActive;
	new previousSource = gRpcMapIconBatchSource;

	if (gRpcMapIconBatchTimer != 0)
	{
		KillTimer(gRpcMapIconBatchTimer);
		gRpcMapIconBatchTimer = 0;
	}

	for (new observer = 0; observer < MAX_PLAYERS; observer++)
	{
		if (IsPlayerConnected(observer) && !IsPlayerNPC(observer))
		{
			RemoveRpcMapIconBatchForPlayer(observer);
		}
	}

	gRpcMapIconBatchActive = false;
	gRpcMapIconBatchSource = INVALID_PLAYER_ID;
	gRpcMapIconBatchTickCount = 0;

	if (!quiet && playerid != INVALID_PLAYER_ID && IsPlayerConnected(playerid))
	{
		SendClientMessage(playerid, 0x66FF66FF, wasActive ?
			"[bare-rpctest] RPC144 removed all 100 map-icon slots from every observer." :
			"[bare-rpctest] No RPC map-icon batch was active; all slots were cleared anyway.");
	}
	printf("[bare-rpctest] RPC144 map_icon_batch_stop requester=%d source=%d was_active=%d",
		playerid, previousSource, wasActive);
	return 1;
}

/// Sends all 100 RPC56 slots to one observer as four style-specific clusters.
/// References: https://open.mp/docs/scripting/functions/SetPlayerMapIcon
///             https://open.mp/docs/scripting/resources/mapiconstyles
///             https://open.mp/docs/scripting/resources/mapicons
stock ApplyRpcMapIconBatchForPlayer(playerid, Float:sourceX, Float:sourceY, Float:sourceZ)
{
	for (new slot = 0; slot < RPC_MAP_ICON_SLOT_COUNT; slot++)
	{
		new style = slot / RPC_MAP_ICON_SLOTS_PER_STYLE;
		new styleSlot = slot % RPC_MAP_ICON_SLOTS_PER_STYLE;
		new gridX = (styleSlot % RPC_MAP_ICON_GRID_WIDTH) - 2;
		new gridY = (styleSlot / RPC_MAP_ICON_GRID_WIDTH) - 2;
		new Float:clusterX = 0.0;
		new Float:clusterY = 0.0;

		switch (style)
		{
			case 0: clusterX = -RPC_MAP_ICON_CLUSTER_DISTANCE;
			case 1: clusterX = RPC_MAP_ICON_CLUSTER_DISTANCE;
			case 2: clusterY = -RPC_MAP_ICON_CLUSTER_DISTANCE;
			case 3: clusterY = RPC_MAP_ICON_CLUSTER_DISTANCE;
		}

		new markerType = gRpcMapIconMarkerTypes[styleSlot];
		new colour = markerType == 0 ? gRpcMapIconColours[style] : 0;
		SetPlayerMapIcon(
			playerid,
			slot,
			sourceX + clusterX + (float(gridX) * RPC_MAP_ICON_GRID_SPACING),
			sourceY + clusterY + (float(gridY) * RPC_MAP_ICON_GRID_SPACING),
			sourceZ,
			markerType,
			colour,
			MAPICON:style
		);
	}
	return 1;
}

/// Refreshes the moving player beacon, intentionally replacing all 100 slots.
/// References: https://open.mp/docs/scripting/functions/GetPlayerPos
///             https://open.mp/docs/scripting/functions/SetTimer
public RpcMapIconBatchTick()
{
	if (!gRpcMapIconBatchActive || gRpcMapIconBatchSource == INVALID_PLAYER_ID ||
		!IsPlayerConnected(gRpcMapIconBatchSource))
	{
		StopRpcMapIconBatch(INVALID_PLAYER_ID, true);
		return 1;
	}

	new Float:x, Float:y, Float:z;
	if (!GetPlayerPos(gRpcMapIconBatchSource, x, y, z))
	{
		StopRpcMapIconBatch(INVALID_PLAYER_ID, true);
		return 1;
	}

	new observers = 0;
	for (new observer = 0; observer < MAX_PLAYERS; observer++)
	{
		if (!IsPlayerConnected(observer) || IsPlayerNPC(observer))
		{
			continue;
		}
		ApplyRpcMapIconBatchForPlayer(observer, x, y, z);
		observers++;
	}

	gRpcMapIconBatchTickCount++;
	if (gRpcMapIconBatchTickCount <= 5 || (gRpcMapIconBatchTickCount % 30) == 0)
	{
		printf("[bare-rpctest] RPC56 map_icon_batch tick=%d source=%d observers=%d sets=%d pos=%.3f,%.3f,%.3f",
			gRpcMapIconBatchTickCount, gRpcMapIconBatchSource, observers,
			observers * RPC_MAP_ICON_SLOT_COUNT, x, y, z);
	}
	return 1;
}

/// Starts a repeating 100-slot map-icon batch centered around the player.
/// References: https://open.mp/docs/scripting/functions/SetPlayerMapIcon
///             https://open.mp/docs/scripting/functions/SetTimer
stock StartRpcMapIconBatch(playerid)
{
	if (gRpcMapIconBatchActive || gRpcMapIconBatchTimer != 0)
	{
		StopRpcMapIconBatch(playerid, true);
	}

	gRpcMapIconBatchSource = playerid;
	gRpcMapIconBatchActive = true;
	gRpcMapIconBatchTickCount = 0;
	gRpcMapIconBatchTimer = SetTimer("RpcMapIconBatchTick", RPC_MAP_ICON_TICK_MS, true);
	if (gRpcMapIconBatchTimer == 0)
	{
		gRpcMapIconBatchActive = false;
		gRpcMapIconBatchSource = INVALID_PLAYER_ID;
		SendClientMessage(playerid, 0xFF6666FF, "[bare-rpctest] Could not create the RPC map-icon timer.");
		return 1;
	}
	RpcMapIconBatchTick();
	if (!gRpcMapIconBatchActive)
	{
		SendClientMessage(playerid, 0xFF6666FF, "[bare-rpctest] Could not read the beacon player's position.");
		return 1;
	}

	SendClientMessage(playerid, 0x66FF66FF,
		"[bare-rpctest] RPC56 beacon active: 100 slots, 25 per style, refreshed every second for all observers.");
	SendClientMessage(playerid, 0xFFFFFFFF,
		"[bare-rpctest] Walk around while the replacement client watches HUD/map. Use /rpcmapiconsoff to send RPC144 x100.");
	printf("[bare-rpctest] RPC56 map_icon_batch_start source=%d timer=%d interval_ms=%d",
		playerid, gRpcMapIconBatchTimer, RPC_MAP_ICON_TICK_MS);
	return 1;
}

/// Starts the fixed direct-MP3 RPC41 probe, globally or five metres east of the player.
/// Reference: https://open.mp/docs/scripting/functions/PlayAudioStreamForPlayer
stock StartRpcAudioTest(playerid, bool:positional)
{
	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	PlayAudioStreamForPlayer(
		playerid,
		gRpcAudioTestUrl,
		x + RPC_AUDIO_TEST_OFFSET_X,
		y,
		z,
		RPC_AUDIO_TEST_RADIUS,
		positional
	);
	SendClientMessage(playerid, 0x66FF66FF, positional ?
		"[bare-rpctest] RPC41 fixed positional MP3 started five metres east; walk away to test attenuation." :
		"[bare-rpctest] RPC41 fixed non-positional MP3 started.");
	printf("[bare-rpctest] RPC41 fixed player=%d positional=%d url=%s", playerid, positional, gRpcAudioTestUrl);
	return 1;
}

stock DestroyRpcAttachedObject(const reason[])
{
	if (gRpcAttachedObject == INVALID_OBJECT_ID)
	{
		return 0;
	}

	printf("[bare-rpctest] RPC75 destroy object=%d player=%d reason=%s",
		gRpcAttachedObject, gRpcAttachedObjectPlayer, reason);
	DestroyObject(gRpcAttachedObject);
	gRpcAttachedObject = INVALID_OBJECT_ID;
	gRpcAttachedObjectPlayer = INVALID_PLAYER_ID;
	return 1;
}

stock SampObjectScanModelAt(index)
{
	// PROBE_TRACE + TODO_VERIFY: compacted from local SA-MP 0.3.7 SAMP.ide objs section.
	if (index < 72) return 11682 + index;
	index -= 72;
	if (index < 229) return 18631 + index;
	index -= 229;
	if (index < 337) return 18862 + index;
	index -= 337;
	if (index < 75) return 19200 + index;
	index -= 75;
	if (index < 319) return 19277 + index;
	index -= 319;
	if (index < 304) return 19597 + index;
	index -= 304;
	if (index < 97) return 19903 + index;
	return -1;
}

stock SampObjectScanIndexOfModel(modelid)
{
	if (modelid >= 11682 && modelid <= 11753) return modelid - 11682;
	if (modelid >= 18631 && modelid <= 18859) return 72 + modelid - 18631;
	if (modelid >= 18862 && modelid <= 19198) return 301 + modelid - 18862;
	if (modelid >= 19200 && modelid <= 19274) return 638 + modelid - 19200;
	if (modelid >= 19277 && modelid <= 19595) return 713 + modelid - 19277;
	if (modelid >= 19597 && modelid <= 19900) return 1032 + modelid - 19597;
	if (modelid >= 19903 && modelid <= 19999) return 1336 + modelid - 19903;
	return -1;
}

stock ClampSampObjectScanDelay(value, fallback)
{
	if (value <= 0)
	{
		return fallback;
	}
	if (value < SAMP_OBJECT_SCAN_MIN_DELAY_MS)
	{
		return SAMP_OBJECT_SCAN_MIN_DELAY_MS;
	}
	if (value > SAMP_OBJECT_SCAN_MAX_DELAY_MS)
	{
		return SAMP_OBJECT_SCAN_MAX_DELAY_MS;
	}
	return value;
}

stock ScheduleSampObjectScanTick(delayMs)
{
	if (gSampObjectScanTimer != 0)
	{
		KillTimer(gSampObjectScanTimer);
	}
	gSampObjectScanTimer = SetTimer("SampObjectScanTick", delayMs, false);
	return 1;
}

stock DestroySampObjectScanCurrent(const reason[])
{
	if (gSampObjectScanObject == INVALID_OBJECT_ID)
	{
		return 0;
	}

	new modelid = SampObjectScanModelAt(gSampObjectScanIndex);
	printf("[bare-vtest] sampobjscan destroy index=%d/%d model=%d object=%d reason=%s",
		gSampObjectScanIndex + 1, SAMP_OBJECT_SCAN_TOTAL, modelid, gSampObjectScanObject, reason);
	DestroyObject(gSampObjectScanObject);
	gSampObjectScanObject = INVALID_OBJECT_ID;
	return 1;
}

stock StopSampObjectScan(playerid, bool:quiet)
{
	new bool:had_scan = gSampObjectScanActive || gSampObjectScanTimer != 0 || gSampObjectScanObject != INVALID_OBJECT_ID;

	if (gSampObjectScanTimer != 0)
	{
		KillTimer(gSampObjectScanTimer);
		gSampObjectScanTimer = 0;
	}

	DestroySampObjectScanCurrent("stop");
	gSampObjectScanActive = false;
	gSampObjectScanPlayer = INVALID_PLAYER_ID;
	gSampObjectScanIndex = 0;
	gSampObjectScanPhase = SAMP_OBJECT_SCAN_PHASE_HOLD;

	if (had_scan && !quiet && playerid != INVALID_PLAYER_ID && IsPlayerConnected(playerid))
	{
		SendClientMessage(playerid, 0xFFCC66FF, "[bare-vtest] SAMP object scan stopped.");
	}
	if (had_scan)
	{
		printf("[bare-vtest] sampobjscan stopped");
	}
	return 1;
}

stock FinishSampObjectScan()
{
	new playerid = gSampObjectScanPlayer;
	if (gSampObjectScanTimer != 0)
	{
		KillTimer(gSampObjectScanTimer);
		gSampObjectScanTimer = 0;
	}

	gSampObjectScanActive = false;
	gSampObjectScanPlayer = INVALID_PLAYER_ID;
	gSampObjectScanIndex = 0;
	gSampObjectScanPhase = SAMP_OBJECT_SCAN_PHASE_HOLD;

	if (playerid != INVALID_PLAYER_ID && IsPlayerConnected(playerid))
	{
		SendClientMessage(playerid, 0x66FF66FF, "[bare-vtest] SAMP object scan complete.");
	}
	printf("[bare-vtest] sampobjscan complete total=%d hold_ms=%d gap_ms=%d",
		SAMP_OBJECT_SCAN_TOTAL, gSampObjectScanHoldMs, gSampObjectScanGapMs);
	return 1;
}

stock CreateSampObjectScanCurrent()
{
	if (!gSampObjectScanActive)
	{
		return 0;
	}

	new playerid = gSampObjectScanPlayer;
	if (playerid == INVALID_PLAYER_ID || !IsPlayerConnected(playerid))
	{
		StopSampObjectScan(INVALID_PLAYER_ID, true);
		return 0;
	}

	if (gSampObjectScanIndex < 0 || gSampObjectScanIndex >= SAMP_OBJECT_SCAN_TOTAL)
	{
		FinishSampObjectScan();
		return 0;
	}

	new Float:x = 0.0;
	new Float:y = 0.0;
	new Float:z = 0.0;
	GetPlayerPos(playerid, x, y, z);
	x += 4.0;
	z += 1.0;

	new modelid = SampObjectScanModelAt(gSampObjectScanIndex);
	gSampObjectScanObject = CreateObject(modelid, x, y, z, 0.0, 0.0, float(gSampObjectScanIndex % 360), 250.0);
	printf("[bare-vtest] sampobjscan create index=%d/%d model=%d object=%d pos=%.3f %.3f %.3f",
		gSampObjectScanIndex + 1, SAMP_OBJECT_SCAN_TOTAL, modelid, gSampObjectScanObject, x, y, z);

	if (gSampObjectScanObject == INVALID_OBJECT_ID || (gSampObjectScanIndex % 50) == 0)
	{
		new message[144];
		new color = 0x66CCFFFF;
		if (gSampObjectScanObject == INVALID_OBJECT_ID)
		{
			color = 0xFF6666FF;
		}
		format(message, sizeof(message), "[bare-vtest] sampobjscan %d/%d model=%d object=%d",
			gSampObjectScanIndex + 1, SAMP_OBJECT_SCAN_TOTAL, modelid, gSampObjectScanObject);
		SendClientMessage(playerid, color, message);
	}
	return 1;
}

stock StartSampObjectScan(playerid, startIndex, holdMs, gapMs)
{
	if (startIndex < 0 || startIndex >= SAMP_OBJECT_SCAN_TOTAL)
	{
		SendClientMessage(playerid, 0xFFCC66FF, "[bare-vtest] Usage: /sampobjscan [1-1433|modelid] [hold_ms] [gap_ms]");
		return 0;
	}

	if (gSampObjectScanActive)
	{
		StopSampObjectScan(gSampObjectScanPlayer, true);
	}

	gSampObjectScanActive = true;
	gSampObjectScanPlayer = playerid;
	gSampObjectScanIndex = startIndex;
	gSampObjectScanObject = INVALID_OBJECT_ID;
	gSampObjectScanHoldMs = ClampSampObjectScanDelay(holdMs, SAMP_OBJECT_SCAN_DEFAULT_HOLD_MS);
	gSampObjectScanGapMs = ClampSampObjectScanDelay(gapMs, SAMP_OBJECT_SCAN_DEFAULT_GAP_MS);
	gSampObjectScanPhase = SAMP_OBJECT_SCAN_PHASE_HOLD;

	new modelid = SampObjectScanModelAt(startIndex);
	new message[144];
	format(message, sizeof(message), "[bare-vtest] SAMP object scan starting at %d/%d model=%d hold=%d gap=%d.",
		startIndex + 1, SAMP_OBJECT_SCAN_TOTAL, modelid, gSampObjectScanHoldMs, gSampObjectScanGapMs);
	SendClientMessage(playerid, 0x66FF66FF, message);
	printf("[bare-vtest] sampobjscan start player=%d index=%d/%d model=%d hold_ms=%d gap_ms=%d",
		playerid, startIndex + 1, SAMP_OBJECT_SCAN_TOTAL, modelid, gSampObjectScanHoldMs, gSampObjectScanGapMs);

	CreateSampObjectScanCurrent();
	ScheduleSampObjectScanTick(gSampObjectScanHoldMs);
	return 1;
}

stock StartSampObjectScanCommand(playerid, const cmdtext[])
{
	new token = SkipCommandSpaces(cmdtext, 12);
	if (cmdtext[token] == '\0')
	{
		return StartSampObjectScan(playerid, 0, SAMP_OBJECT_SCAN_DEFAULT_HOLD_MS, SAMP_OBJECT_SCAN_DEFAULT_GAP_MS);
	}

	new value = strval(cmdtext[token]);
	new startIndex = -1;
	if (value >= 10000)
	{
		startIndex = SampObjectScanIndexOfModel(value);
	}
	else
	{
		startIndex = value - 1;
	}

	new holdMs = SAMP_OBJECT_SCAN_DEFAULT_HOLD_MS;
	new gapMs = SAMP_OBJECT_SCAN_DEFAULT_GAP_MS;
	token = NextCommandToken(cmdtext, token);
	if (cmdtext[token] != '\0')
	{
		holdMs = strval(cmdtext[token]);
		token = NextCommandToken(cmdtext, token);
		if (cmdtext[token] != '\0')
		{
			gapMs = strval(cmdtext[token]);
		}
	}

	return StartSampObjectScan(playerid, startIndex, holdMs, gapMs);
}

main()
{
}

SetupPlayerForClassSelection(playerid)
{
	SetPlayerInterior(playerid, CLASS_SELECTION_INTERIOR);
	SetPlayerVirtualWorld(playerid, 0);
	SetPlayerPos(playerid, CLASS_SELECTION_X, CLASS_SELECTION_Y, CLASS_SELECTION_Z);
	SetPlayerFacingAngle(playerid, CLASS_SELECTION_ANGLE);
	SetPlayerCameraPos(playerid, CLASS_CAMERA_X, CLASS_CAMERA_Y, CLASS_CAMERA_Z);
	SetPlayerCameraLookAt(playerid, CLASS_SELECTION_X, CLASS_SELECTION_Y, CLASS_SELECTION_Z);
}

/// Creates static vanilla GTA vehicles near the player spawn.
/// Reference: https://open.mp/docs/scripting/functions/AddStaticVehicleEx
CreateVehicleStreamTestFleet()
{
	for (new i = 0; i < TEST_VEHICLE_COUNT; i++)
	{
		gVehicleIds[i] = AddStaticVehicleEx(
			gVehicleModels[i],
			gVehicleSpawns[i][0],
			gVehicleSpawns[i][1],
			gVehicleSpawns[i][2],
			gVehicleSpawns[i][3],
			1 + i,
			0,
			300,
			false
		);

		if (gVehicleIds[i] != INVALID_VEHICLE_ID)
		{
			LinkVehicleToInterior(gVehicleIds[i], 0);
			SetVehicleVirtualWorld(gVehicleIds[i], 0);
			printf("[bare-vtest] vehicle[%d] id=%d model=%d pos=%.3f %.3f %.3f", i, gVehicleIds[i],
				gVehicleModels[i], gVehicleSpawns[i][0], gVehicleSpawns[i][1], gVehicleSpawns[i][2]);
		}
		else
		{
			printf("[bare-vtest] vehicle[%d] create failed model=%d", i, gVehicleModels[i]);
		}
	}
}

CreateSpawnCustomObjectTest()
{
	// INFERRED: IDs come from the local SA-MP SAMP.ide fixture and are used as a loader smoke test.
	for (new i = 0; i < SPAWN_CUSTOM_OBJECT_COUNT; i++)
	{
		gSpawnCustomObjects[i] = CreateObject(
			gSpawnCustomModelIds[i],
			gSpawnCustomObjectSpawns[i][0],
			gSpawnCustomObjectSpawns[i][1],
			gSpawnCustomObjectSpawns[i][2],
			gSpawnCustomObjectSpawns[i][3],
			gSpawnCustomObjectSpawns[i][4],
			gSpawnCustomObjectSpawns[i][5],
			300.0
		);

		printf("[bare-vtest] spawn custom object[%d] id=%d model=%d pos=%.3f %.3f %.3f",
			i,
			gSpawnCustomObjects[i],
			gSpawnCustomModelIds[i],
			gSpawnCustomObjectSpawns[i][0],
			gSpawnCustomObjectSpawns[i][1],
			gSpawnCustomObjectSpawns[i][2]);
	}
}

CreateArea51ObjectTest()
{
	gA51LandObject = CreateObject(11692, 199.3440, 1943.7900, 18.2031, 0.0, 0.0, 0.0);
	gA51FenceObject = CreateObject(19312, 191.1410, 1870.0400, 21.4766, 0.0, 0.0, 0.0);

	gA51BuildingObjects[0] = CreateObject(19905, 206.798950, 1931.643432, 16.450595, 0.0, 0.0, 0.0);
	gA51BuildingObjects[1] = CreateObject(19905, 188.208908, 1835.033569, 16.450595, 0.0, 0.0, 0.0);
	gA51BuildingObjects[2] = CreateObject(19905, 230.378875, 1835.033569, 16.450595, 0.0, 0.0, 0.0);
	gA51BuildingObjects[3] = CreateObject(19907, 142.013977, 1902.538085, 17.633581, 0.0, 0.0, 270.0);
	gA51BuildingObjects[4] = CreateObject(19907, 146.854003, 1846.008056, 16.533580, 0.0, 0.0, 0.0);
	gA51BuildingObjects[5] = CreateObject(19909, 137.900390, 1875.024291, 16.836734, 0.0, 0.0, 270.0);
	gA51BuildingObjects[6] = CreateObject(19909, 118.170387, 1875.184326, 16.846735, 0.0, 0.0, 0.0);

	gA51NorthernGate = CreateObject(19313, 134.545074, 1941.527709, 21.691408, 0.0, 0.0, 180.0);
	gA51EasternGate = CreateObject(19313, 286.008666, 1822.744628, 20.010623, 0.0, 0.0, 90.0);

	gA51GateLabels[0] = Create3DTextLabel("{CCCCCC}[Northern Gate]\n{CCCCCC}/a51ngate toggles object movement",
		0xCCCCCCAA, 135.09, 1942.37, 19.82, 10.5, 0, false);
	gA51GateLabels[1] = Create3DTextLabel("{CCCCCC}[Eastern Gate]\n{CCCCCC}/a51egate toggles object movement",
		0xCCCCCCAA, 287.12, 1821.51, 18.14, 10.5, 0, false);

	printf("[bare-vtest] area51 objects land=%d fence=%d north_gate=%d east_gate=%d",
		gA51LandObject, gA51FenceObject, gA51NorthernGate, gA51EasternGate);
}

RemoveArea51BaseBuildings(playerid)
{
	RemoveBuildingForPlayer(playerid, 16203, 199.3440, 1943.7900, 18.2031, 250.0);
	RemoveBuildingForPlayer(playerid, 16590, 199.3440, 1943.7900, 18.2031, 250.0);
	RemoveBuildingForPlayer(playerid, 16323, 199.3360, 1943.8800, 18.2031, 250.0);
	RemoveBuildingForPlayer(playerid, 16619, 199.3360, 1943.8800, 18.2031, 250.0);
	RemoveBuildingForPlayer(playerid, 1697, 228.7970, 1835.3400, 23.2344, 250.0);
	RemoveBuildingForPlayer(playerid, 16094, 191.1410, 1870.0400, 21.4766, 250.0);
	printf("[bare-vtest] area51 remove-building RPCs queued for player=%d", playerid);
}

PlacePlayerAtArea51ObjectTest(playerid)
{
	SetPlayerInterior(playerid, 0);
	SetPlayerVirtualWorld(playerid, 0);
	SetPlayerPos(playerid, 135.2000, 1948.5100, 19.7400);
	SetPlayerFacingAngle(playerid, 180.0);
	SetCameraBehindPlayer(playerid);
	GameTextForPlayer(playerid, "~b~~h~Area 51 Object Test", 3000, 3);
	SendClientMessage(playerid, 0x66CCFFFF, "[bare-vtest] Area51 object test. Use /a51ngate or /a51egate.");
}

ToggleArea51NorthernGate(playerid)
{
	if (gA51NorthernGate == INVALID_OBJECT_ID)
	{
		SendClientMessage(playerid, 0xFF6666FF, "[bare-vtest] northern gate object is invalid.");
		return 0;
	}

	gA51NorthernGateOpen = !gA51NorthernGateOpen;
	if (gA51NorthernGateOpen)
	{
		MoveObject(gA51NorthernGate, 121.545074, 1941.527709, 21.691408, 1.3, 0.0, 0.0, 180.0);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-vtest] northern gate opening.");
	}
	else
	{
		MoveObject(gA51NorthernGate, 134.545074, 1941.527709, 21.691408, 1.3, 0.0, 0.0, 180.0);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-vtest] northern gate closing.");
	}
	return 1;
}

ToggleArea51EasternGate(playerid)
{
	if (gA51EasternGate == INVALID_OBJECT_ID)
	{
		SendClientMessage(playerid, 0xFF6666FF, "[bare-vtest] eastern gate object is invalid.");
		return 0;
	}

	gA51EasternGateOpen = !gA51EasternGateOpen;
	if (gA51EasternGateOpen)
	{
		MoveObject(gA51EasternGate, 286.008666, 1833.744628, 20.010623, 1.1, 0.0, 0.0, 90.0);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-vtest] eastern gate opening.");
	}
	else
	{
		MoveObject(gA51EasternGate, 286.008666, 1822.744628, 20.010623, 1.1, 0.0, 0.0, 90.0);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-vtest] eastern gate closing.");
	}
	return 1;
}

/// Places the player next to the vehicle test fleet.
/// Reference: https://open.mp/docs/scripting/functions/SetPlayerPos
PlacePlayerAtVehicleTest(playerid)
{
	SetPlayerInterior(playerid, 0);
	SetPlayerVirtualWorld(playerid, 0);
	SetPlayerPos(playerid, SPAWN_X, SPAWN_Y, SPAWN_Z);
	SetPlayerFacingAngle(playerid, SPAWN_ANGLE);
	SetCameraBehindPlayer(playerid);
	SendClientMessage(playerid, 0x66FF66FF, "[bare-vtest] Spawned near static vehicles and SA-MP custom objects. Use /vtest or /vput.");
}

GivePlayerTestWeapons(playerid)
{
	ResetPlayerWeapons(playerid);
	GivePlayerWeapon(playerid, TEST_PRIMARY_WEAPON, TEST_PRIMARY_AMMO);
	GivePlayerWeapon(playerid, TEST_SECONDARY_WEAPON, TEST_SECONDARY_AMMO);
	GivePlayerWeapon(playerid, TEST_MELEE_WEAPON, TEST_MELEE_AMMO);
	SetPlayerArmedWeapon(playerid, TEST_PRIMARY_WEAPON);
	SendClientMessage(playerid, 0x66CCFFFF, "[bare-vtest] Test weapons: M4, Deagle, Knife.");
}

/// Initializes the gamemode and registers the class-selection skins.
/// Reference: https://open.mp/docs/scripting/callbacks/OnGameModeInit
public OnGameModeInit()
{
	SetGameModeText(GAME_MODE_NAME);
	ShowPlayerMarkers(PLAYER_MARKERS_MODE_GLOBAL);
	ShowNameTags(true);

	for (new i = 0; i < sizeof(gPlayerSkins); i++)
	{
		AddPlayerClassEx(
			PLAYER_TEAM,
			gPlayerSkins[i],
			SPAWN_X,
			SPAWN_Y,
			SPAWN_Z,
			SPAWN_ANGLE,
			TEST_PRIMARY_WEAPON,
			TEST_PRIMARY_AMMO,
			TEST_SECONDARY_WEAPON,
			TEST_SECONDARY_AMMO,
			TEST_MELEE_WEAPON,
			TEST_MELEE_AMMO
		);
	}
	CreateVehicleStreamTestFleet();
	CreateSpawnCustomObjectTest();
	CreateArea51ObjectTest();

	print("\n----------------------------------");
	print("  Bare open.mp Vehicle/Object Test Script");
	print("----------------------------------\n");
	return 1;
}

/// Shows a short welcome game text when a player connects.
/// Reference: https://open.mp/docs/scripting/callbacks/OnPlayerConnect
public OnPlayerConnect(playerid)
{
	new name[MAX_PLAYER_NAME + 1];
	GetPlayerName(playerid, name, sizeof(name));
	printf("[bare-rpctest] RPC137 ServerJoin player=%d name=%s npc=%d", playerid, name, IsPlayerNPC(playerid));

	gRpcShopEnabled[playerid] = false;
	gRpcColorAlternate[playerid] = false;
	RemoveArea51BaseBuildings(playerid);
	GameTextForPlayer(playerid, WELCOME_GAME_TEXT, 5000, 5);
	return 1;
}

/// Handles minimal debug commands for vehicle stream testing.
/// Reference: https://open.mp/docs/scripting/callbacks/OnPlayerCommandText
public OnPlayerCommandText(playerid, cmdtext[])
{
	if (!strcmp(cmdtext, "/rpcbatch", true))
	{
		SendPlayerAudioChatRpcHelp(playerid);
		return 1;
	}

	if (!strcmp(cmdtext, "/rpcmapicons", true))
	{
		return StartRpcMapIconBatch(playerid);
	}

	if (!strcmp(cmdtext, "/rpcmapiconsoff", true))
	{
		return StopRpcMapIconBatch(playerid, false);
	}

	if (!strcmp(cmdtext, "/rpcedit", true))
	{
		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		if (gRpcEditObject != INVALID_OBJECT_ID)
		{
			DestroyObject(gRpcEditObject);
		}
		gRpcEditObject = CreateObject(1239, x + 3.0, y, z + 1.0, 0.0, 0.0, 0.0, 200.0);
		if (gRpcEditObject == INVALID_OBJECT_ID || !BeginObjectEditing(playerid, gRpcEditObject))
		{
			SendClientMessage(playerid, 0xFF6666FF, "[bare-rpctest] Could not start the RPC28 edit setup.");
			return 1;
		}
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Object editor active; use /rpccanceledit to exercise RPC28.");
		printf("[bare-rpctest] RPC28 setup player=%d object=%d", playerid, gRpcEditObject);
		return 1;
	}

	if (!strcmp(cmdtext, "/rpccanceledit", true))
	{
		CancelEdit(playerid);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] RPC28 CancelEdit sent.");
		printf("[bare-rpctest] RPC28 CancelEdit player=%d", playerid);
		return 1;
	}

	if (!strcmp(cmdtext, "/rpcshop", true))
	{
		gRpcShopEnabled[playerid] = !gRpcShopEnabled[playerid];
		SetPlayerShopName(playerid, gRpcShopEnabled[playerid] ? "FDPIZA" : "");
		SendClientMessage(playerid, 0x66FF66FF, gRpcShopEnabled[playerid] ?
			"[bare-rpctest] RPC33 shop FDPIZA loaded." : "[bare-rpctest] RPC33 shop unloaded.");
		printf("[bare-rpctest] RPC33 player=%d enabled=%d shop=FDPIZA", playerid, gRpcShopEnabled[playerid]);
		return 1;
	}

	if (!strcmp(cmdtext, "/rpcaudiotest", true))
	{
		return StartRpcAudioTest(playerid, false);
	}

	if (!strcmp(cmdtext, "/rpcaudiotestpos", true))
	{
		return StartRpcAudioTest(playerid, true);
	}

	if ((!strcmp(cmdtext, "/rpcaudio", true, 9) && (cmdtext[9] == ' ' || cmdtext[9] == '\t')) ||
		(!strcmp(cmdtext, "/rpcaudiopos", true, 12) && (cmdtext[12] == ' ' || cmdtext[12] == '\t')))
	{
		new bool:positional = !strcmp(cmdtext, "/rpcaudiopos", true, 12);
		new token = SkipCommandSpaces(cmdtext, positional ? 12 : 9);
		new urlLength = strlen(cmdtext[token]);
		if (urlLength < 1 || urlLength > 127)
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Usage: /rpcaudio[pos] <url up to 127 characters>");
			return 1;
		}

		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		PlayAudioStreamForPlayer(playerid, cmdtext[token], x + 5.0, y, z, 30.0, positional);
		SendClientMessage(playerid, 0x66FF66FF, positional ?
			"[bare-rpctest] RPC41 positional stream started five metres east." :
			"[bare-rpctest] RPC41 non-positional stream started.");
		printf("[bare-rpctest] RPC41 player=%d positional=%d url_length=%d", playerid, positional, urlLength);
		return 1;
	}

	if (!strcmp(cmdtext, "/rpcstopaudio", true))
	{
		StopAudioStreamForPlayer(playerid);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] RPC42 audio stream stopped.");
		printf("[bare-rpctest] RPC42 player=%d", playerid);
		return 1;
	}

	if (!strcmp(cmdtext, "/rpcbubble", true, 10) &&
		(cmdtext[10] == ' ' || cmdtext[10] == '\t'))
	{
		new token = SkipCommandSpaces(cmdtext, 10);
		new targetid = strval(cmdtext[token]);
		if (cmdtext[token] == '\0' || targetid == playerid || !IsPlayerConnected(targetid))
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Usage: /rpcbubble <connected other player/NPC id>");
			return 1;
		}
		SetPlayerChatBubble(targetid, "[RPC59] visible for eight seconds", 0x66CCFFFF, 100.0, 8000);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] RPC59 bubble set on the other player/NPC for eight seconds.");
		printf("[bare-rpctest] RPC59 observer=%d target=%d", playerid, targetid);
		return 1;
	}

	if (!strcmp(cmdtext, "/rpcteam", true, 8) &&
		(cmdtext[8] == ' ' || cmdtext[8] == '\t'))
	{
		new token = SkipCommandSpaces(cmdtext, 8);
		new team = strval(cmdtext[token]);
		if (cmdtext[token] == '\0' || team < 0 || team > 255)
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Usage: /rpcteam <0-255; 255 means no team>");
			return 1;
		}
		SetPlayerTeam(playerid, team);
		new message[96];
		format(message, sizeof(message), "[bare-rpctest] RPC69 team set to %d.", team);
		SendClientMessage(playerid, 0x66FF66FF, message);
		printf("[bare-rpctest] RPC69 player=%d team=%d", playerid, team);
		return 1;
	}

	if (!strcmp(cmdtext, "/rpccolor", true))
	{
		gRpcColorAlternate[playerid] = !gRpcColorAlternate[playerid];
		new colour = gRpcColorAlternate[playerid] ? 0xFF6666FF : 0x66CCFFFF;
		SetPlayerColor(playerid, colour);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] RPC72 player colour toggled; check scoreboard/name tag from client two.");
		printf("[bare-rpctest] RPC72 player=%d colour=%x", playerid, colour);
		return 1;
	}

	if (!strcmp(cmdtext, "/rpcattach", true))
	{
		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		DestroyRpcAttachedObject("replace");
		gRpcAttachedObject = CreateObject(1239, x, y, z + 1.5, 0.0, 0.0, 0.0, 200.0);
		if (gRpcAttachedObject == INVALID_OBJECT_ID)
		{
			SendClientMessage(playerid, 0xFF6666FF, "[bare-rpctest] Could not create the RPC75 test object.");
			return 1;
		}
		gRpcAttachedObjectPlayer = playerid;
		// OPENMP_REF: the legacy native is documented to return false even when the attach is sent.
		AttachObjectToPlayer(gRpcAttachedObject, playerid, 0.0, 0.0, 1.5, 0.0, 0.0, 0.0);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] RPC75 info-icon object attached above the player; /rpcattachoff removes it.");
		printf("[bare-rpctest] RPC75 object=%d player=%d", gRpcAttachedObject, playerid);
		return 1;
	}

	if (!strcmp(cmdtext, "/rpcattachoff", true))
	{
		DestroyRpcAttachedObject("command");
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] RPC75 test object destroyed.");
		return 1;
	}

	if (!strcmp(cmdtext, "/yadayada", true))
	{
		return 1;
	}

	if (!strcmp(cmdtext, "/vtest", true))
	{
		PlacePlayerAtVehicleTest(playerid);
		return 1;
	}

	if (!strcmp(cmdtext, "/a51", true))
	{
		RemoveArea51BaseBuildings(playerid);
		PlacePlayerAtArea51ObjectTest(playerid);
		return 1;
	}

	if (!strcmp(cmdtext, "/a51ngate", true))
	{
		ToggleArea51NorthernGate(playerid);
		return 1;
	}

	if (!strcmp(cmdtext, "/a51egate", true))
	{
		ToggleArea51EasternGate(playerid);
		return 1;
	}

	if (!strcmp(cmdtext, "/sampobjscan", true, 12) && (cmdtext[12] == '\0' || cmdtext[12] == ' ' || cmdtext[12] == '\t'))
	{
		StartSampObjectScanCommand(playerid, cmdtext);
		return 1;
	}

	if (!strcmp(cmdtext, "/sampobjstop", true))
	{
		StopSampObjectScan(playerid, false);
		return 1;
	}

	if (!strcmp(cmdtext, "/vput", true))
	{
		if (gVehicleIds[0] != INVALID_VEHICLE_ID)
		{
			PutPlayerInVehicle(playerid, gVehicleIds[0], 0);
			SendClientMessage(playerid, 0x66FF66FF, "[bare-vtest] PutPlayerInVehicle issued for first test vehicle.");
		}
		return 1;
	}

	if (!strcmp(cmdtext, "/guns", true))
	{
		GivePlayerTestWeapons(playerid);
		return 1;
	}

	if (!strcmp(cmdtext, "/smallrpc", true))
	{
		ApplySmallRpcTest(playerid);
		return 1;
	}

	if (!strcmp(cmdtext, "/smallrpcreset", true))
	{
		ResetSmallRpcTest(playerid);
		return 1;
	}

	if (!strcmp(cmdtext, "/wanted", true, 7) &&
		(cmdtext[7] == '\0' || cmdtext[7] == ' ' || cmdtext[7] == '\t'))
	{
		new token = SkipCommandSpaces(cmdtext, 7);
		new level = strval(cmdtext[token]);
		new message[96];
		if (cmdtext[token] == '\0' || level < 0 || level > 6)
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Usage: /wanted <0-6>");
			return 1;
		}
		SetPlayerWantedLevel(playerid, level);
		format(message, sizeof(message), "[bare-rpctest] Wanted level %d applied.", level);
		SendClientMessage(playerid, 0x66FF66FF, message);
		printf("[bare-rpctest] player=%d wanted=%d", playerid, level);
		return 1;
	}

	if (!strcmp(cmdtext, "/money", true, 6) &&
		(cmdtext[6] == '\0' || cmdtext[6] == ' ' || cmdtext[6] == '\t'))
	{
		new token = SkipCommandSpaces(cmdtext, 6);
		new amount = strval(cmdtext[token]);
		new message[112];
		if (cmdtext[token] == '\0')
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Usage: /money <signed amount>");
			return 1;
		}
		GivePlayerMoney(playerid, amount);
		format(message, sizeof(message), "[bare-rpctest] Money delta %d applied.", amount);
		SendClientMessage(playerid, 0x66FF66FF, message);
		printf("[bare-rpctest] player=%d money_delta=%d", playerid, amount);
		return 1;
	}

	if (!strcmp(cmdtext, "/ammo", true, 5) &&
		(cmdtext[5] == '\0' || cmdtext[5] == ' ' || cmdtext[5] == '\t'))
	{
		new token = SkipCommandSpaces(cmdtext, 5);
		new weapon = strval(cmdtext[token]);
		token = NextCommandToken(cmdtext, token);
		new ammo = strval(cmdtext[token]);
		new message[112];
		if (cmdtext[token] == '\0' || weapon < 0 || weapon > 46 || ammo < 0 || ammo > 65535)
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Usage: /ammo <weapon 0-46> <ammo 0-65535>");
			return 1;
		}
		// SetPlayerAmmo only updates an existing weapon slot in the 0.3.7 client.
		// Seed the requested slot first so this command exercises RPC 145 reliably.
		GivePlayerWeapon(playerid, WEAPON:weapon, 1);
		SetPlayerAmmo(playerid, WEAPON:weapon, ammo);
		format(message, sizeof(message), "[bare-rpctest] Weapon %d ammo set to %d.", weapon, ammo);
		SendClientMessage(playerid, 0x66FF66FF, message);
		printf("[bare-rpctest] player=%d weapon=%d ammo=%d", playerid, weapon, ammo);
		return 1;
	}

	if (!strcmp(cmdtext, "/skin", true, 5) &&
		(cmdtext[5] == '\0' || cmdtext[5] == ' ' || cmdtext[5] == '\t'))
	{
		new token = SkipCommandSpaces(cmdtext, 5);
		new skin = strval(cmdtext[token]);
		new message[96];
		if (cmdtext[token] == '\0' || skin < 0 || skin > 311 || skin == 74)
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Usage: /skin <0-311, except 74>");
			return 1;
		}
		SetPlayerSkin(playerid, skin);
		format(message, sizeof(message), "[bare-rpctest] Skin %d applied.", skin);
		SendClientMessage(playerid, 0x66FF66FF, message);
		printf("[bare-rpctest] player=%d skin=%d", playerid, skin);
		return 1;
	}

	if (!strcmp(cmdtext, "/skill", true, 6) &&
		(cmdtext[6] == '\0' || cmdtext[6] == ' ' || cmdtext[6] == '\t'))
	{
		new token = SkipCommandSpaces(cmdtext, 6);
		new skill = strval(cmdtext[token]);
		token = NextCommandToken(cmdtext, token);
		new level = strval(cmdtext[token]);
		new message[112];
		if (cmdtext[token] == '\0' || skill < 0 || skill > 10 || level < 0 || level > 999)
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Usage: /skill <0-10> <0-999>");
			return 1;
		}
		SetPlayerSkillLevel(playerid, WEAPONSKILL:skill, level);
		format(message, sizeof(message), "[bare-rpctest] Weapon skill %d set to %d.", skill, level);
		SendClientMessage(playerid, 0x66FF66FF, message);
		printf("[bare-rpctest] player=%d skill=%d level=%d", playerid, skill, level);
		return 1;
	}

	if (!strcmp(cmdtext, "/drunk", true, 6) &&
		(cmdtext[6] == '\0' || cmdtext[6] == ' ' || cmdtext[6] == '\t'))
	{
		new token = SkipCommandSpaces(cmdtext, 6);
		new level = strval(cmdtext[token]);
		new message[104];
		if (cmdtext[token] == '\0' || level < 0 || level > 50000)
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Usage: /drunk <0-50000>");
			return 1;
		}
		SetPlayerDrunkLevel(playerid, level);
		format(message, sizeof(message), "[bare-rpctest] Drunk level set to %d.", level);
		SendClientMessage(playerid, 0x66FF66FF, message);
		printf("[bare-rpctest] player=%d drunk=%d", playerid, level);
		return 1;
	}

	if (!strcmp(cmdtext, "/fight", true, 6) &&
		(cmdtext[6] == '\0' || cmdtext[6] == ' ' || cmdtext[6] == '\t'))
	{
		new token = SkipCommandSpaces(cmdtext, 6);
		new style = strval(cmdtext[token]);
		new message[104];
		if (cmdtext[token] == '\0' ||
			!(style == 4 || style == 5 || style == 6 || style == 7 || style == 15 || style == 16))
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Usage: /fight <4,5,6,7,15,16>");
			return 1;
		}
		SetPlayerFightingStyle(playerid, FIGHT_STYLE:style);
		format(message, sizeof(message), "[bare-rpctest] Fighting style set to %d.", style);
		SendClientMessage(playerid, 0x66FF66FF, message);
		printf("[bare-rpctest] player=%d fighting_style=%d", playerid, style);
		return 1;
	}

	if (!strcmp(cmdtext, "/findz", true))
	{
		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		SetPlayerPosFindZ(playerid, x + 5.0, y, z + 50.0);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] SetPlayerPosFindZ sent five metres east.");
		printf("[bare-rpctest] player=%d pos_find_z=%.3f,%.3f,%.3f", playerid, x + 5.0, y, z + 50.0);
		return 1;
	}

	if (!strcmp(cmdtext, "/velocity", true, 9) &&
		(cmdtext[9] == '\0' || cmdtext[9] == ' ' || cmdtext[9] == '\t'))
	{
		new token = SkipCommandSpaces(cmdtext, 9);
		new Float:x = floatstr(cmdtext[token]);
		token = NextCommandToken(cmdtext, token);
		new Float:y = floatstr(cmdtext[token]);
		token = NextCommandToken(cmdtext, token);
		new Float:z = floatstr(cmdtext[token]);
		new message[128];
		if (cmdtext[token] == '\0' || x < -20.0 || x > 20.0 || y < -20.0 || y > 20.0 || z < -20.0 || z > 20.0)
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Usage: /velocity <x -20..20> <y -20..20> <z -20..20>");
			return 1;
		}
		SetPlayerVelocity(playerid, x, y, z);
		format(message, sizeof(message), "[bare-rpctest] Velocity set to %.3f %.3f %.3f.", x, y, z);
		SendClientMessage(playerid, 0x66FF66FF, message);
		printf("[bare-rpctest] player=%d velocity=%.3f,%.3f,%.3f", playerid, x, y, z);
		return 1;
	}

	if (!strcmp(cmdtext, "/eject", true))
	{
		RemovePlayerFromVehicle(playerid);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] RemovePlayerFromVehicle sent.");
		printf("[bare-rpctest] player=%d remove_from_vehicle=1", playerid);
		return 1;
	}

	if (!strcmp(cmdtext, "/animtest", true))
	{
		ApplyAnimation(playerid, "DANCING", "DAN_Down_A", 4.1, true, false, false, true, 0);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Looping animation applied; use /animclear.");
		return 1;
	}

	if (!strcmp(cmdtext, "/animclear", true))
	{
		ClearAnimations(playerid, SYNC_ALL);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] ClearAnimations sent.");
		printf("[bare-rpctest] player=%d clear_animations=1", playerid);
		return 1;
	}

	if (!strcmp(cmdtext, "/vvelocity", true, 10) &&
		(cmdtext[10] == '\0' || cmdtext[10] == ' ' || cmdtext[10] == '\t'))
	{
		new token = SkipCommandSpaces(cmdtext, 10);
		new Float:x = floatstr(cmdtext[token]);
		token = NextCommandToken(cmdtext, token);
		new Float:y = floatstr(cmdtext[token]);
		token = NextCommandToken(cmdtext, token);
		new Float:z = floatstr(cmdtext[token]);
		new message[128];
		if (cmdtext[token] == '\0' || !IsPlayerInAnyVehicle(playerid) ||
			x < -20.0 || x > 20.0 || y < -20.0 || y > 20.0 || z < -20.0 || z > 20.0)
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Usage in vehicle: /vvelocity <x -20..20> <y -20..20> <z -20..20>");
			return 1;
		}
		SetVehicleVelocity(GetPlayerVehicleID(playerid), x, y, z);
		format(message, sizeof(message), "[bare-rpctest] Vehicle velocity set to %.3f %.3f %.3f.", x, y, z);
		SendClientMessage(playerid, 0x66FF66FF, message);
		printf("[bare-rpctest] player=%d vehicle_velocity=%.3f,%.3f,%.3f", playerid, x, y, z);
		return 1;
	}

	if (!strcmp(cmdtext, "/stunt 0", true) || !strcmp(cmdtext, "/stunt 1", true))
	{
		new bool:enabled = cmdtext[7] == '1';
		EnableStuntBonusForPlayer(playerid, enabled);
		SendClientMessage(playerid, 0x66FF66FF,
			enabled ? "[bare-rpctest] Stunt bonus enabled." : "[bare-rpctest] Stunt bonus disabled.");
		printf("[bare-rpctest] player=%d stunt_bonus=%d", playerid, enabled);
		return 1;
	}

	if (!strcmp(cmdtext, "/cp", true))
	{
		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		SetPlayerCheckpoint(playerid, x, y + 10.0, z, 3.0);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Checkpoint set ten metres north.");
		return 1;
	}

	if (!strcmp(cmdtext, "/cpoff", true))
	{
		DisablePlayerCheckpoint(playerid);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Checkpoint disabled.");
		return 1;
	}

	if (!strcmp(cmdtext, "/racecp", true))
	{
		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		SetPlayerRaceCheckpoint(playerid, CP_TYPE_GROUND_NORMAL,
			x, y + 10.0, z, x, y + 30.0, z, 4.0);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Race checkpoint set ten metres north.");
		return 1;
	}

	if (!strcmp(cmdtext, "/racecpoff", true))
	{
		DisablePlayerRaceCheckpoint(playerid);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Race checkpoint disabled.");
		return 1;
	}

	if (!strcmp(cmdtext, "/pickup", true))
	{
		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		if (gRpcTestPickup != -1)
		{
			DestroyPickup(gRpcTestPickup);
		}
		gRpcTestPickup = CreatePickup(1240, 2, x, y + 4.0, z, GetPlayerVirtualWorld(playerid));
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Health pickup created four metres north.");
		printf("[bare-rpctest] player=%d pickup=%d", playerid, gRpcTestPickup);
		return 1;
	}

	if (!strcmp(cmdtext, "/pickupoff", true))
	{
		if (gRpcTestPickup != -1)
		{
			DestroyPickup(gRpcTestPickup);
			gRpcTestPickup = -1;
		}
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Test pickup destroyed.");
		return 1;
	}

	if (!strcmp(cmdtext, "/explode", true))
	{
		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		CreateExplosionForPlayer(playerid, x, y + 8.0, z, 2, 10.0);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Explosion created eight metres north.");
		return 1;
	}

	if (!strcmp(cmdtext, "/clock 0", true) || !strcmp(cmdtext, "/clock 1", true))
	{
		new bool:enabled = cmdtext[7] == '1';
		TogglePlayerClock(playerid, enabled);
		SendClientMessage(playerid, 0x66FF66FF,
			enabled ? "[bare-rpctest] Player clock enabled." : "[bare-rpctest] Player clock disabled.");
		return 1;
	}

	if (!strcmp(cmdtext, "/vpos", true))
	{
		if (!IsPlayerInAnyVehicle(playerid))
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Enter a vehicle first.");
			return 1;
		}
		new Float:x, Float:y, Float:z;
		new vehicleid = GetPlayerVehicleID(playerid);
		GetVehiclePos(vehicleid, x, y, z);
		SetVehiclePos(vehicleid, x + 10.0, y, z);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Vehicle moved ten metres east.");
		return 1;
	}

	if (!strcmp(cmdtext, "/vzangle", true))
	{
		if (!IsPlayerInAnyVehicle(playerid))
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Enter a vehicle first.");
			return 1;
		}
		new vehicleid = GetPlayerVehicleID(playerid);
		new Float:angle;
		GetVehicleZAngle(vehicleid, angle);
		SetVehicleZAngle(vehicleid, angle + 90.0);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Vehicle rotated 90 degrees.");
		return 1;
	}

	if (!strcmp(cmdtext, "/trailer", true))
	{
		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		if (gRpcTestCab == INVALID_VEHICLE_ID)
		{
			gRpcTestCab = CreateVehicle(515, x + 5.0, y, z, 0.0, 1, 1, 600);
		}
		if (gRpcTestTrailer == INVALID_VEHICLE_ID)
		{
			gRpcTestTrailer = CreateVehicle(435, x + 5.0, y - 8.0, z, 0.0, 1, 1, 600);
		}
		AttachTrailerToVehicle(gRpcTestTrailer, gRpcTestCab);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Trailer attached to the nearby Roadtrain.");
		return 1;
	}

	if (!strcmp(cmdtext, "/traileroff", true))
	{
		if (gRpcTestCab != INVALID_VEHICLE_ID && gRpcTestTrailer != INVALID_VEHICLE_ID)
		{
			if (!IsTrailerAttachedToVehicle(gRpcTestCab))
			{
				AttachTrailerToVehicle(gRpcTestTrailer, gRpcTestCab);
			}
			DetachTrailerFromVehicle(gRpcTestCab);
		}
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Trailer detached.");
		return 1;
	}

	if (!strcmp(cmdtext, "/vparams", true))
	{
		if (!IsPlayerInAnyVehicle(playerid))
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Enter a vehicle first.");
			return 1;
		}
		gRpcVehicleParamsEnabled = !gRpcVehicleParamsEnabled;
		new vehicleid = GetPlayerVehicleID(playerid);
		SetVehicleParamsEx(vehicleid, gRpcVehicleParamsEnabled, gRpcVehicleParamsEnabled, false,
			gRpcVehicleParamsEnabled, false, false, false);
		SendClientMessage(playerid, 0x66FF66FF, gRpcVehicleParamsEnabled ?
			"[bare-rpctest] Engine/lights/doors enabled and locked." :
			"[bare-rpctest] Engine/lights/doors disabled and unlocked.");
		return 1;
	}

	if (!strcmp(cmdtext, "/vcomponent", true))
	{
		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		if (gRpcTestModVehicle == INVALID_VEHICLE_ID)
		{
			gRpcTestModVehicle = CreateVehicle(560, x + 5.0, y, z, 0.0, 1, 1, 600);
		}
		AddVehicleComponent(gRpcTestModVehicle, 1138);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Sultan spoiler added; use /vcomponentoff.");
		return 1;
	}

	if (!strcmp(cmdtext, "/vcomponentoff", true))
	{
		if (gRpcTestModVehicle != INVALID_VEHICLE_ID)
		{
			RemoveVehicleComponent(gRpcTestModVehicle, 1138);
		}
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Sultan spoiler removal sent.");
		return 1;
	}

	if (!strcmp(cmdtext, "/vinterior", true))
	{
		if (!IsPlayerInAnyVehicle(playerid))
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Enter a vehicle first.");
			return 1;
		}
		new vehicleid = GetPlayerVehicleID(playerid);
		new interior = GetVehicleInterior(vehicleid) == 0 ? 1 : 0;
		LinkVehicleToInterior(vehicleid, interior);
		SendClientMessage(playerid, 0x66FF66FF, interior ?
			"[bare-rpctest] Vehicle linked to interior 1." :
			"[bare-rpctest] Vehicle linked back to interior 0.");
		return 1;
	}

	if (!strcmp(cmdtext, "/vplate", true))
	{
		if (!IsPlayerInAnyVehicle(playerid))
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Enter a vehicle first.");
			return 1;
		}
		new vehicleid = GetPlayerVehicleID(playerid);
		SetVehicleNumberPlate(vehicleid, "RPC123");
		SetVehicleToRespawn(vehicleid);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] Plate RPC123 set; vehicle respawning.");
		return 1;
	}

	if (!strcmp(cmdtext, "/vlock", true))
	{
		if (!IsPlayerInAnyVehicle(playerid))
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Enter a vehicle first.");
			return 1;
		}
		gRpcVehicleObjectiveEnabled = !gRpcVehicleObjectiveEnabled;
		SetVehicleParamsForPlayer(GetPlayerVehicleID(playerid), playerid,
			gRpcVehicleObjectiveEnabled, gRpcVehicleObjectiveEnabled);
		SendClientMessage(playerid, 0x66FF66FF, gRpcVehicleObjectiveEnabled ?
			"[bare-rpctest] Objective marker and player-specific lock enabled." :
			"[bare-rpctest] Objective marker and player-specific lock disabled.");
		return 1;
	}

	if (!strcmp(cmdtext, "/gravity", true, 8) &&
		(cmdtext[8] == '\0' || cmdtext[8] == ' ' || cmdtext[8] == '\t'))
	{
		new token = SkipCommandSpaces(cmdtext, 8);
		new Float:gravity = floatstr(cmdtext[token]);
		new message[112];
		if (cmdtext[token] == '\0' || gravity < -1.0 || gravity > 1.0)
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Usage: /gravity <-1.0..1.0> (default 0.008)");
			return 1;
		}
		SetGravity(gravity);
		format(message, sizeof(message), "[bare-rpctest] Global gravity %.6f applied.", gravity);
		SendClientMessage(playerid, 0x66FF66FF, message);
		printf("[bare-rpctest] player=%d gravity=%.6f", playerid, gravity);
		return 1;
	}

	if (!strcmp(cmdtext, "/vhealth", true, 8))
	{
		new token = SkipCommandSpaces(cmdtext, 8);
		if (cmdtext[token] == '\0')
		{
			SendVehicleHealthSummary(playerid);
			return 1;
		}

		new slot = strval(cmdtext[token]);
		token = NextCommandToken(cmdtext, token);
		if (cmdtext[token] == '\0')
		{
			SendVehicleHealthLine(playerid, slot);
			return 1;
		}

		SetTestVehicleHealth(playerid, slot, floatstr(cmdtext[token]), "set");
		return 1;
	}

	if (!strcmp(cmdtext, "/vhdmg", true, 6))
	{
		new token = SkipCommandSpaces(cmdtext, 6);
		new slot = strval(cmdtext[token]);
		new Float:health = 0.0;

		token = NextCommandToken(cmdtext, token);
		if (cmdtext[token] == '\0' || !IsTestVehicleSlot(slot) || !GetVehicleHealth(gVehicleIds[slot], health))
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-vtest] Usage: /vhdmg <slot> <amount>");
			return 1;
		}

		health -= floatstr(cmdtext[token]);
		if (health < 0.0)
		{
			health = 0.0;
		}
		SetTestVehicleHealth(playerid, slot, health, "damage");
		return 1;
	}

	if (!strcmp(cmdtext, "/vhfix", true, 6))
	{
		new token = SkipCommandSpaces(cmdtext, 6);
		new slot = strval(cmdtext[token]);
		if (!IsTestVehicleSlot(slot))
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-vtest] Usage: /vhfix <slot>");
			return 1;
		}

		RepairVehicle(gVehicleIds[slot]);
		SetTestVehicleHealth(playerid, slot, 1000.0, "repair");
		return 1;
	}

	if (!strcmp(cmdtext, "/vhkill", true, 7))
	{
		new token = SkipCommandSpaces(cmdtext, 7);
		SetTestVehicleHealth(playerid, strval(cmdtext[token]), 0.0, "kill");
		return 1;
	}

	if (!strcmp(cmdtext, "/rpcname", true))
	{
		gRpcRenameAlternate = !gRpcRenameAlternate;
		SetPlayerName(playerid, gRpcRenameAlternate ? "RPC_Name_A" : "RPC_Name_B");
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] RPC11 name toggled; check chat/scoreboard/name tag.");
		return 1;
	}

	if (!strcmp(cmdtext, "/bounds", true))
	{
		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		gRpcBoundsEnabled = !gRpcBoundsEnabled;
		if (gRpcBoundsEnabled)
		{
			SetPlayerWorldBounds(playerid, x + 20.0, x - 20.0, y + 20.0, y - 20.0);
			SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] RPC17 bounds set to 20 metres around current position.");
		}
		else
		{
			SetPlayerWorldBounds(playerid, 20000.0, -20000.0, 20000.0, -20000.0);
			SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] RPC17 bounds reset.");
		}
		return 1;
	}

	if (!strcmp(cmdtext, "/gmrestart", true))
	{
		SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Triggering GMX/RPC40 now.");
		SendRconCommand("gmx");
		return 1;
	}

	if (!strcmp(cmdtext, "/forceclass", true))
	{
		ForceClassSelection(playerid);
		SetPlayerHealth(playerid, 0.0);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] RPC74 latched and player killed; class selection should follow.");
		return 1;
	}

	if (!strcmp(cmdtext, "/camobject", true))
	{
		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		if (gRpcCameraObject != INVALID_OBJECT_ID) DestroyObject(gRpcCameraObject);
		gRpcCameraObject = CreateObject(19300, x + 8.0, y, z + 6.0, 0.0, 0.0, 0.0, 200.0);
		AttachCameraToObject(playerid, gRpcCameraObject);
		MoveObject(gRpcCameraObject, x + 8.0, y + 20.0, z + 6.0, 4.0);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] RPC81 camera attached to a moving object; /specoff restores it.");
		return 1;
	}

	if (!strcmp(cmdtext, "/caminterp", true))
	{
		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		SetPlayerCameraLookAt(playerid, x, y, z + 1.0, CAMERA_CUT);
		InterpolateCameraPos(playerid, x + 4.0, y - 12.0, z + 5.0, x + 4.0, y + 12.0, z + 8.0, 3000, CAMERA_MOVE);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] RPC82 camera position should interpolate for three seconds.");
		return 1;
	}

	if (!strcmp(cmdtext, "/special", true))
	{
		gRpcSpecialEnabled = !gRpcSpecialEnabled;
		SetPlayerSpecialAction(playerid, gRpcSpecialEnabled ? SPECIAL_ACTION_HANDSUP : SPECIAL_ACTION_NONE);
		SendClientMessage(playerid, 0x66FF66FF, gRpcSpecialEnabled ?
			"[bare-rpctest] RPC88 hands-up enabled." : "[bare-rpctest] RPC88 special action cleared.");
		return 1;
	}

	if (!strcmp(cmdtext, "/specoff", true))
	{
		TogglePlayerSpectating(playerid, false);
		SetCameraBehindPlayer(playerid);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] RPC124 spectating disabled and camera restored.");
		return 1;
	}

	if (!strcmp(cmdtext, "/specplayer", true, 11))
	{
		new token = SkipCommandSpaces(cmdtext, 11);
		new targetid = strval(cmdtext[token]);
		if (cmdtext[token] == '\0' || !IsPlayerConnected(targetid) || targetid == playerid)
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Usage: /specplayer <other player id>");
			return 1;
		}
		TogglePlayerSpectating(playerid, true);
		PlayerSpectatePlayer(playerid, targetid, SPECTATE_MODE_NORMAL);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] RPC126 spectating player; /specoff restores.");
		return 1;
	}

	if (!strcmp(cmdtext, "/spectrain", true, 10) &&
		(cmdtext[10] == '\0' || cmdtext[10] == ' ' || cmdtext[10] == '\t'))
	{
		new token = SkipCommandSpaces(cmdtext, 10);
		new wanted = cmdtext[token] == '\0' ? 0 : strval(cmdtext[token]);
		new found = 0;
		new targetid = INVALID_PLAYER_ID;
		new targetname[MAX_PLAYER_NAME + 1];
		new message[144];

		if (wanted < 0 || wanted >= 5)
		{
			SendClientMessage(playerid, 0xFFCC66FF, "[bare-rpctest] Usage: /spectrain [0-4]");
			return 1;
		}
		for (new candidate = 0; candidate < MAX_PLAYERS; candidate++)
		{
			if (!IsPlayerConnected(candidate) || !IsPlayerNPC(candidate)) continue;
			GetPlayerName(candidate, targetname, sizeof(targetname));
			if (strcmp(targetname, "TrainDriver", true, 11) != 0) continue;
			if (found++ == wanted)
			{
				targetid = candidate;
				break;
			}
		}
		if (targetid == INVALID_PLAYER_ID)
		{
			SendClientMessage(playerid, 0xFF6666FF, "[bare-rpctest] Train NPC not available yet; check server log/startup.");
			return 1;
		}
		TogglePlayerSpectating(playerid, true);
		PlayerSpectatePlayer(playerid, targetid, SPECTATE_MODE_NORMAL);
		format(message, sizeof(message), "[bare-rpctest] RPC126 spectating train NPC %s (player ID %d); /specoff restores.",
			targetname, targetid);
		SendClientMessage(playerid, 0x66FF66FF, message);
		return 1;
	}

	if (!strcmp(cmdtext, "/specveh", true))
	{
		TogglePlayerSpectating(playerid, true);
		PlayerSpectateVehicle(playerid, gVehicleIds[0], SPECTATE_MODE_NORMAL);
		SendClientMessage(playerid, 0x66FF66FF, "[bare-rpctest] RPC127 spectating vehicle 0; /specoff restores.");
		return 1;
	}

	return 0;
}

/// Applies spawn-time player settings.
/// Reference: https://open.mp/docs/scripting/callbacks/OnPlayerSpawn
public OnPlayerSpawn(playerid)
{
	TogglePlayerClock(playerid, false);
	RemoveArea51BaseBuildings(playerid);
	PlacePlayerAtVehicleTest(playerid);
	GivePlayerTestWeapons(playerid);
	return 1;
}

public OnPlayerDisconnect(playerid, reason)
{
	new name[MAX_PLAYER_NAME + 1];
	GetPlayerName(playerid, name, sizeof(name));
	printf("[bare-rpctest] RPC138 ServerQuit player=%d name=%s reason=%d", playerid, name, reason);

	if (gRpcMapIconBatchActive && gRpcMapIconBatchSource == playerid)
	{
		StopRpcMapIconBatch(playerid, true);
	}

	if (gSampObjectScanActive && gSampObjectScanPlayer == playerid)
	{
		StopSampObjectScan(playerid, true);
	}
	if (gRpcAttachedObjectPlayer == playerid)
	{
		DestroyRpcAttachedObject("disconnect");
	}
	gRpcShopEnabled[playerid] = false;
	gRpcColorAlternate[playerid] = false;
	return 1;
}

public OnGameModeExit()
{
	StopRpcMapIconBatch(INVALID_PLAYER_ID, true);
	StopSampObjectScan(INVALID_PLAYER_ID, true);
	DestroyRpcAttachedObject("gamemode_exit");
	if (gRpcEditObject != INVALID_OBJECT_ID)
	{
		DestroyObject(gRpcEditObject);
		gRpcEditObject = INVALID_OBJECT_ID;
	}
	return 1;
}

/// Places the player camera for class selection.
/// Reference: https://open.mp/docs/scripting/callbacks/OnPlayerRequestClass
public OnPlayerRequestClass(playerid, classid)
{
	#pragma unused classid

	SetupPlayerForClassSelection(playerid);
	return 1;
}

/// Logs damage callback traffic for sync-path verification.
/// Reference: https://open.mp/docs/scripting/callbacks/OnPlayerGiveDamage
public OnPlayerGiveDamage(playerid, damagedid, Float:amount, WEAPON:weaponid, bodypart)
{
	printf("[bare-vtest] OnPlayerGiveDamage player=%d damaged=%d amount=%.3f weapon=%d bodypart=%d",
		playerid, damagedid, amount, _:weaponid, bodypart);
	return 1;
}

/// Logs bullet callback traffic for sync-path verification.
/// Reference: https://open.mp/docs/scripting/callbacks/OnPlayerWeaponShot
public OnPlayerWeaponShot(playerid, WEAPON:weaponid, BULLET_HIT_TYPE:hittype, hitid, Float:fX, Float:fY, Float:fZ)
{
	printf("[bare-vtest] OnPlayerWeaponShot player=%d weapon=%d hittype=%d hitid=%d pos=%.3f %.3f %.3f",
		playerid, _:weaponid, _:hittype, hitid, fX, fY, fZ);
	return 1;
}

/// Logs completion of the object movement RPC path.
/// Reference: https://open.mp/docs/scripting/callbacks/OnObjectMoved
public OnObjectMoved(objectid)
{
	if (objectid == gA51NorthernGate)
	{
		printf("[bare-vtest] OnObjectMoved northern_gate=%d open=%d", objectid, gA51NorthernGateOpen);
		return 1;
	}
	if (objectid == gA51EasternGate)
	{
		printf("[bare-vtest] OnObjectMoved eastern_gate=%d open=%d", objectid, gA51EasternGateOpen);
		return 1;
	}
	return 1;
}

public SampObjectScanTick()
{
	gSampObjectScanTimer = 0;

	if (!gSampObjectScanActive)
	{
		return 0;
	}

	if (gSampObjectScanPhase == SAMP_OBJECT_SCAN_PHASE_HOLD)
	{
		DestroySampObjectScanCurrent("hold_elapsed");
		gSampObjectScanIndex++;
		if (gSampObjectScanIndex >= SAMP_OBJECT_SCAN_TOTAL)
		{
			FinishSampObjectScan();
			return 1;
		}

		gSampObjectScanPhase = SAMP_OBJECT_SCAN_PHASE_GAP;
		ScheduleSampObjectScanTick(gSampObjectScanGapMs);
		return 1;
	}

	gSampObjectScanPhase = SAMP_OBJECT_SCAN_PHASE_HOLD;
	CreateSampObjectScanCurrent();
	if (gSampObjectScanActive)
	{
		ScheduleSampObjectScanTick(gSampObjectScanHoldMs);
	}
	return 1;
}
