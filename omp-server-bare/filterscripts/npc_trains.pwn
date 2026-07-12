/*
    ================================================================================
    Example Filterscript for Train NPC System using open.mp NPC component
    ================================================================================
	Made by itsneufox @2025 for open.mp team

    Description:
        - Handles 5 passenger train NPCs that drive in continuous loops across San Andreas.
        - Each NPC follows a 5-segment circular route using recorded playback files.

    Requirements:
        - Recording files must be present in "npcmodes/recordings/" folder:
          * TRAIN_UNITY_TO_MARKET.rec (Los Santos Unity to Market)
          * TRAIN_MARKET_TO_CRANBERRY.rec (Market to San Fierro Cranberry)
          * TRAIN_CRANBERRY_TO_YELLOW_BELL.rec (Cranberry to LV Yellow Bell)
          * TRAIN_YELLOW_BELL_TO_LINDEN.rec (Yellow Bell to LV Linden)
          * TRAIN_LINDEN_TO_UNITY.rec (Linden to Los Santos Unity)

    Configuration:
        - TRAIN_DEBUG: Set to true to enable debug messages in server log
        - ENABLE_NPC_MARKER: Set to true to show colored markers on train NPCs
        - Vehicle spawn coordinates and recording files are stored in arrays
        - Train model ID: 538 (Streak)

	Warning:
		- This script uses a total of 5 NPCs and 20 vehicles (each train has 4 carriages)
*/

//==============================================================================================================================================================
//
// Includes
//
//==============================================================================================================================================================

#define FILTERSCRIPT

#include <open.mp>

//==============================================================================================================================================================
//
// Configuration
//
//==============================================================================================================================================================

/**
 * Debug toggle - set to true to enable debug messages
 *
 * When enabled, the following debug messages will be printed:
 * - NPC creation/spawn notifications
 * - Route change notifications
 * - Playback end notifications
 *
 * Default: false (disabled)
 */
#define TRAIN_DEBUG true

/**
 * NPC Marker toggle - set to true to show markers on train NPCs
 *
 * When enabled, train NPCs will have a colored marker on the minimap
 * When disabled, markers will be transparent (invisible)
 *
 * Default: false (disabled)
 */
#define ENABLE_NPC_MARKER true

/**
 * Constants
 */
#define NUM_PLAYBACK_FILES 5    					// Total number of recording files per route cycle
#define MAX_TRAINS 5								// Total number of trains
#define STREAK_TRAIN_ID 538     					// Vehicle model ID for the train
#define TRAIN_NPC_SKIN 7        					// Skin ID for train driver NPCs
#define TRAIN_DRIVER_SEAT 0     					// Driver seat in vehicle

/**
 * NPC Marker Colors
 */
#if ENABLE_NPC_MARKER
	#define TRAIN_NPC_MARKER_COLOR 0xFF6347FF		// red color when markers enabled
#else
	#define TRAIN_NPC_MARKER_COLOR 0x00000000		// Transparent when markers disabled
#endif

/**
 * Train Station Enum
 */
enum
{
	TRAIN_UNITY,        // Unity Station
	TRAIN_MARKET,       // Market Station
	TRAIN_CRANBERRY,    // Cranberry Station
	TRAIN_YELLOWBELL,   // Yellow Bell Station
	TRAIN_LINDEN        // Linden Station
};


//==============================================================================================================================================================
//
// Variables
//
//==============================================================================================================================================================

/**
 * Train Data Arrays
 */
new TrainNPC[MAX_TRAINS];							// NPC IDs indexed by station
new TrainVehicle[MAX_TRAINS];						// Vehicle IDs indexed by station
new gPlaybackCycle[MAX_TRAINS];						// Current playback cycle (0-4) for each train
new bool:gPlaybackStarted[MAX_TRAINS];				// Has initial playback started for each train

/**
 * Train Configuration Data
 */
new const TrainNPCNames[][] = {
	"TrainDriverUnity",
	"TrainDriverMarket",
	"TrainDriverCranberry",
	"TrainDriverYellowBell",
	"TrainDriverLinden"
};

new const Float:TrainSpawnData[][4] = {
	// X, Y, Z, Angle
	{1700.7551, -1953.6531, 14.8756, 190.0},		// Unity
	{814.6358, -1361.7816, -0.2226, 190.0},			// Market
	{-1942.7950, 168.4164, 27.0006, 190.0},			// Cranberry
	{1462.0745, 2630.8787, 10.8203, 190.0},			// Yellow Bell
	{2853.2910, 1348.4590, 11.1272, 190.0}			// Linden
};

#if TRAIN_DEBUG
new const TrainStationNames[][] = {
	"Unity",
	"Market",
	"Cranberry",
	"YellowBell",
	"Linden"
};
#endif

// Recording files for each segment (5 total in circular order)
new const TrainRecordings[][] = {
	"TRAIN_UNITY_TO_MARKET",
	"TRAIN_MARKET_TO_CRANBERRY",
	"TRAIN_CRANBERRY_TO_YELLOW_BELL",
	"TRAIN_YELLOW_BELL_TO_LINDEN",
	"TRAIN_LINDEN_TO_UNITY"
};


//==============================================================================================================================================================
//
// Helper Functions
//
//==============================================================================================================================================================

/**
 * GetTrainIndexByNPC
 *
 * Finds the train index for a given NPC ID
 *
 * @param npcid - The NPC ID to search for
 * @return Train index (0-4) or -1 if not found
 */
stock GetTrainIndexByNPC(npcid)
{
	for (new i = 0; i < MAX_TRAINS; i++)
	{
		if (TrainNPC[i] == npcid)
			return i;
	}
	return -1;
}

//==============================================================================================================================================================
//
// Filterscript Callbacks
//
//==============================================================================================================================================================

/**
 * OnFilterScriptInit
 *
 * Called when the filterscript is loaded
 *
 * Actions:
 * - Prints system load message
 * - Creates all train vehicles at their respective spawn points
 * - Creates all train NPCs
 * - Initializes playback tracking variables
 *
 * Train Spawn Locations are defined in TrainSpawnData array
 *
 * @return 1 on success
 */
public OnFilterScriptInit()
{
	print("+---------------------------------------------+");
	print("|  Example Filterscript for Train NPC System  |");
	print("|     using open.mp NPC component loaded!     |");
	print("+---------------------------------------------+");

	// Create all train vehicles and NPCs
	for (new i = 0; i < MAX_TRAINS; i++)
	{
		// Create train vehicle
		TrainVehicle[i] = AddStaticVehicleEx(
			STREAK_TRAIN_ID,
			TrainSpawnData[i][0],  // X
			TrainSpawnData[i][1],  // Y
			TrainSpawnData[i][2],  // Z
			TrainSpawnData[i][3],  // Angle
			-1, -1, -1
		);

		// Create train NPC
		TrainNPC[i] = NPC_Create(TrainNPCNames[i]);

		// Initialize playback tracking
		gPlaybackCycle[i] = 0;
		gPlaybackStarted[i] = false;

		#if TRAIN_DEBUG
		if (TrainNPC[i] != INVALID_NPC_ID)
		{
			printf("[TRAIN NPC]: Driver of train %s created (NPC ID: %d, Vehicle: %d)",
				TrainStationNames[i], TrainNPC[i], TrainVehicle[i]);
		}
		#endif
	}

	#if TRAIN_DEBUG
	printf("[TRAIN NPC]: All %d passenger train vehicles and NPCs created", MAX_TRAINS);
	#endif

	return 1;
}

/**
 * OnFilterScriptExit
 *
 * Called when the filterscript is unloaded
 *
 * Actions:
 * - Destroys all created train NPCs
 * - Destroys all created train vehicles
 * - Prints system unload message
 * - Cleans up resources
 *
 * @return 1 on success
 */
public OnFilterScriptExit()
{
	// Destroy all train NPCs and vehicles
	for (new i = 0; i < MAX_TRAINS; i++)
	{
		if (NPC_IsValid(TrainNPC[i]))
			NPC_Destroy(TrainNPC[i]);

		if (TrainVehicle[i] != INVALID_VEHICLE_ID)
			DestroyVehicle(TrainVehicle[i]);
	}

	print("+---------------------------------------------+");
	print("|  Example Filterscript for Train NPC System  |");
	print("|    using open.mp NPC component unloaded!    |");
	print("+---------------------------------------------+");

	return 1;
}

/**
 * OnNPCCreate
 *
 * Called when an NPC is created and connects to the server
 *
 * Actions:
 * - Schedules NPC spawn using a 100ms timer for proper initialization
 *
 * @param npcid - The NPC ID that was created
 * @return 1 on success
 */
public OnNPCCreate(npcid)
{
	#if TRAIN_DEBUG
	printf("[TRAIN NPC]: OnNPCCreate called for NPC ID %d", npcid);
	#endif

	// Timer to spawn the NPC after creation
	SetTimerEx("Train_SpawnNPC", 100, false, "i", npcid);

	return 1;
}

/**
 * Train_SpawnNPC
 *
 * Timer function to spawn a train NPC after creation
 *
 * @param npcid - The NPC ID to spawn
 * @return void
 */
forward Train_SpawnNPC(npcid);
public Train_SpawnNPC(npcid)
{
	// Find which train this NPC belongs to
	new trainIdx = GetTrainIndexByNPC(npcid);

	if (trainIdx != -1)
	{
		// Set spawn position before spawning
		NPC_SetPos(npcid, TrainSpawnData[trainIdx][0], TrainSpawnData[trainIdx][1], TrainSpawnData[trainIdx][2]);
		NPC_SetFacingAngle(npcid, TrainSpawnData[trainIdx][3]);
		NPC_SetSkin(npcid, TRAIN_NPC_SKIN);

		// Spawn the NPC so OnNPCSpawn gets called
		NPC_Spawn(npcid);

		#if TRAIN_DEBUG
		printf("[TRAIN NPC]: Spawning train %s (ID: %d)", TrainStationNames[trainIdx], npcid);
		#endif
	}
}

/**
 * OnNPCSpawn
 *
 * Called when an NPC spawns
 *
 * Actions:
 * - Sets the NPC skin
 * - Puts the NPC in their assigned vehicle
 * - Sets the NPC marker color (based on ENABLE_NPC_MARKER config)
 * - Schedules initial playback to start after 1 second delay
 *
 * @param npcid - The NPC ID that spawned
 * @return 1 on success
 */
public OnNPCSpawn(npcid)
{
	new trainIdx = GetTrainIndexByNPC(npcid);

	if (trainIdx != -1 && TrainVehicle[trainIdx] != INVALID_VEHICLE_ID)
	{
		NPC_SetSkin(npcid, TRAIN_NPC_SKIN);
		NPC_PutInVehicle(npcid, TrainVehicle[trainIdx], TRAIN_DRIVER_SEAT);
		SetPlayerColor(npcid, TRAIN_NPC_MARKER_COLOR);

		// Timer of 1 second before starting playback
		SetTimerEx("Train_StartInitialPlayback", 1000, false, "i", npcid);

		#if TRAIN_DEBUG
		printf("[TRAIN NPC]: Driver of train %s spawned, putting in vehicle", TrainStationNames[trainIdx]);
		#endif
	}

	return 1;
}

/**
 * Train_StartInitialPlayback
 *
 * Timer function to start initial playback for train NPCs
 * Called after a short delay when NPC is spawned and put in vehicle
 *
 * @param npcid - The NPC ID to start playback for
 * @return void
 */
forward Train_StartInitialPlayback(npcid);
public Train_StartInitialPlayback(npcid)
{
	// Find which train this NPC belongs to
	new trainIdx = GetTrainIndexByNPC(npcid);

	if (trainIdx != -1 && !gPlaybackStarted[trainIdx])
	{
		// Each train starts with its own first recording
		NPC_StartPlayback(npcid, TrainRecordings[trainIdx], false, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
		gPlaybackCycle[trainIdx] = 1;
		gPlaybackStarted[trainIdx] = true;

		#if TRAIN_DEBUG
		printf("[TRAIN NPC]: Train %s started initial playback", TrainStationNames[trainIdx]);
		#endif
	}
}

//==============================================================================================================================================================
//
// Core Functions
//
//==============================================================================================================================================================

/**
 * TrainNPC_StartNextPlayback
 *
 * Determines and starts the next playback route for a train NPC
 *
 * Route Logic:
 * - Each NPC follows a 5-segment cycle (0, 1, 2, 3, 4)
 * - Cycle resets to 0 after reaching 4
 * - Each segment corresponds to a different recording file
 *
 * Route Segments (circular):
 * Unity -> Market -> Cranberry -> Yellow Bell -> Linden -> Unity
 *
 * Starting Positions:
 * - Train Unity: Unity (0=Unity->Market, 1=Market->Cranberry, 2=Cranberry->YellowBell, 3=YellowBell->Linden, 4=Linden->Unity)
 * - Train Market: Market (0=Market->Cranberry, 1=Cranberry->YellowBell, 2=YellowBell->Linden, 3=Linden->Unity, 4=Unity->Market)
 * - Train Cranberry: Cranberry (0=Cranberry->YellowBell, 1=YellowBell->Linden, 2=Linden->Unity, 3=Unity->Market, 4=Market->Cranberry)
 * - Train YellowBell: Yellow Bell (0=YellowBell->Linden, 1=Linden->Unity, 2=Unity->Market, 3=Market->Cranberry, 4=Cranberry->YellowBell)
 * - Train Linden: Linden (0=Linden->Unity, 1=Unity->Market, 2=Market->Cranberry, 3=Cranberry->YellowBell, 4=YellowBell->Linden)
 *
 * @param npcid - The NPC ID of the train NPC
 * @return void
 */
stock TrainNPC_StartNextPlayback(npcid)
{
	new trainIdx = GetTrainIndexByNPC(npcid);
	if (trainIdx == -1) return;

	if (gPlaybackCycle[trainIdx] >= NUM_PLAYBACK_FILES)
		gPlaybackCycle[trainIdx] = 0;

	// trainIdx offset to help us get the right recording for this train's position in the route
	new recordingIdx = (trainIdx + gPlaybackCycle[trainIdx]) % NUM_PLAYBACK_FILES;

	NPC_StartPlayback(npcid, TrainRecordings[recordingIdx], false, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

	#if TRAIN_DEBUG
	printf("[TRAIN NPC]: Train %s -> next segment (recording: %s)",
		TrainStationNames[trainIdx], TrainRecordings[recordingIdx]);
	#endif

	gPlaybackCycle[trainIdx]++;
}

/**
 * OnNPCPlaybackEnd
 *
 * Called when an NPC finishes playing a recording
 *
 * Actions:
 * - Identifies train NPCs using GetTrainIndexByNPC helper function
 * - Automatically starts the next route segment for continuous operation
 *
 * This callback ensures continuous train operation by automatically
 * cycling through route segments when one completes.
 *
 * @param npcid - The NPC ID of the NPC that finished playback
 * @param recordid - The ID of the recording that finished (unused)
 * @return 1 on success
 */
public OnNPCPlaybackEnd(npcid, recordid)
{
	// Check if this NPC is a train
	if (GetTrainIndexByNPC(npcid) != -1)
	{
		#if TRAIN_DEBUG
		printf("[TRAIN NPC]: NPC %d finished route, starting next...", npcid);
		#endif

		// Automatically start the next route segment
		TrainNPC_StartNextPlayback(npcid);
	}

	return 1;
}
