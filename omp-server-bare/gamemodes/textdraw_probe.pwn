#define SAMP_COMPAT
#include <open.mp>

#define TD_MODE_NAME ("TextDraw 0.3.7 probe")
#define TD_GLOBAL_COUNT (24)
#define TD_PLAYER_COUNT (10)
#define TD_SELECT_COLOR (0x66CCFFFF)

static Text:gGlobalTextDraws[TD_GLOBAL_COUNT];
static PlayerText:gPlayerTextDraws[MAX_PLAYERS][TD_PLAYER_COUNT];
static bool:gGlobalTextDrawsBuilt;
static bool:gPlayerTextDrawsBuilt[MAX_PLAYERS];
static gTextDrawVersion[MAX_PLAYERS];

static const Float:SPAWN_X = 1958.3783;
static const Float:SPAWN_Y = 1343.1572;
static const Float:SPAWN_Z = 15.3746;
static const Float:SPAWN_A = 270.0;

main()
{
}

stock ResetGlobalTextDrawSlots()
{
	for (new i = 0; i < TD_GLOBAL_COUNT; i++)
	{
		gGlobalTextDraws[i] = Text:INVALID_TEXT_DRAW;
	}
	gGlobalTextDrawsBuilt = false;
	return 1;
}

stock ResetPlayerTextDrawSlots(playerid)
{
	for (new i = 0; i < TD_PLAYER_COUNT; i++)
	{
		gPlayerTextDraws[playerid][i] = PlayerText:INVALID_PLAYER_TEXT_DRAW;
	}
	gPlayerTextDrawsBuilt[playerid] = false;
	gTextDrawVersion[playerid] = 0;
	return 1;
}

stock Text:CreateGlobalProbeTextDraw(Float:x, Float:y, const text[], TEXT_DRAW_FONT:font, TEXT_DRAW_ALIGN:alignment,
	Float:letterX, Float:letterY, Float:sizeX, Float:sizeY, color, boxColor, backgroundColor,
	bool:box, shadow, outline, bool:proportional, bool:selectable)
{
	new Text:textid = TextDrawCreate(x, y, text);
	if (textid == Text:INVALID_TEXT_DRAW)
	{
		return Text:INVALID_TEXT_DRAW;
	}

	TextDrawFont(textid, font);
	TextDrawAlignment(textid, alignment);
	TextDrawLetterSize(textid, letterX, letterY);
	TextDrawTextSize(textid, sizeX, sizeY);
	TextDrawColor(textid, color);
	TextDrawUseBox(textid, box);
	TextDrawBoxColor(textid, boxColor);
	TextDrawBackgroundColor(textid, backgroundColor);
	TextDrawSetShadow(textid, shadow);
	TextDrawSetOutline(textid, outline);
	TextDrawSetProportional(textid, proportional);
	TextDrawSetSelectable(textid, selectable);
	return textid;
}

stock PlayerText:CreatePlayerProbeTextDraw(playerid, Float:x, Float:y, const text[], TEXT_DRAW_FONT:font,
	TEXT_DRAW_ALIGN:alignment, Float:letterX, Float:letterY, Float:sizeX, Float:sizeY, color,
	boxColor, backgroundColor, bool:box, shadow, outline, bool:proportional, bool:selectable)
{
	new PlayerText:textid = CreatePlayerTextDraw(playerid, x, y, text);
	if (textid == PlayerText:INVALID_PLAYER_TEXT_DRAW)
	{
		return PlayerText:INVALID_PLAYER_TEXT_DRAW;
	}

	PlayerTextDrawFont(playerid, textid, font);
	PlayerTextDrawAlignment(playerid, textid, alignment);
	PlayerTextDrawLetterSize(playerid, textid, letterX, letterY);
	PlayerTextDrawTextSize(playerid, textid, sizeX, sizeY);
	PlayerTextDrawColor(playerid, textid, color);
	PlayerTextDrawUseBox(playerid, textid, box);
	PlayerTextDrawBoxColor(playerid, textid, boxColor);
	PlayerTextDrawBackgroundColor(playerid, textid, backgroundColor);
	PlayerTextDrawSetShadow(playerid, textid, shadow);
	PlayerTextDrawSetOutline(playerid, textid, outline);
	PlayerTextDrawSetProportional(playerid, textid, proportional);
	PlayerTextDrawSetSelectable(playerid, textid, selectable);
	return textid;
}

stock BuildGlobalTextDrawProbe()
{
	ResetGlobalTextDrawSlots();

	gGlobalTextDraws[0] = CreateGlobalProbeTextDraw(14.0, 24.0, "TD00 header box shadow",
		TEXT_DRAW_FONT_1, TEXT_DRAW_ALIGN_LEFT, 0.32, 1.15, 276.0, 22.0,
		0xFFFFFFFF, 0x00000099, 0x222222FF, true, 1, 0, true, false);
	gGlobalTextDraws[1] = CreateGlobalProbeTextDraw(22.0, 58.0, "TD01 font0 left",
		TEXT_DRAW_FONT_0, TEXT_DRAW_ALIGN_LEFT, 0.40, 1.45, 230.0, 20.0,
		0xFFCC66FF, 0x00000000, 0x111111FF, false, 1, 0, true, false);
	gGlobalTextDraws[2] = CreateGlobalProbeTextDraw(320.0, 58.0, "TD02 font1 centre",
		TEXT_DRAW_FONT_1, TEXT_DRAW_ALIGN_CENTER, 0.36, 1.35, 210.0, 20.0,
		0x66CCFFFF, 0x00000066, 0x000000FF, true, 1, 0, true, false);
	gGlobalTextDraws[3] = CreateGlobalProbeTextDraw(618.0, 58.0, "TD03 font2 right",
		TEXT_DRAW_FONT_2, TEXT_DRAW_ALIGN_RIGHT, 0.34, 1.25, 410.0, 20.0,
		0x99FF99FF, 0x33111188, 0x000000FF, true, 1, 0, true, false);
	gGlobalTextDraws[4] = CreateGlobalProbeTextDraw(22.0, 92.0, "TD04 font3 outline",
		TEXT_DRAW_FONT_3, TEXT_DRAW_ALIGN_LEFT, 0.46, 1.60, 300.0, 24.0,
		0xFFFFFFFF, 0x00000000, 0xAA2222FF, false, 0, 2, true, false);
	gGlobalTextDraws[5] = CreateGlobalProbeTextDraw(22.0, 124.0, "TD05 non proportional 012345",
		TEXT_DRAW_FONT_1, TEXT_DRAW_ALIGN_LEFT, 0.28, 1.00, 320.0, 18.0,
		0xCCCCFFFF, 0x00000000, 0x000000FF, false, 1, 0, false, false);
	gGlobalTextDraws[6] = CreateGlobalProbeTextDraw(22.0, 154.0, "TD06 alpha box/background",
		TEXT_DRAW_FONT_1, TEXT_DRAW_ALIGN_LEFT, 0.30, 1.05, 284.0, 20.0,
		0xFFFFFFFF, 0x33669966, 0xFF0000AA, true, 0, 1, true, false);
	gGlobalTextDraws[7] = CreateGlobalProbeTextDraw(22.0, 186.0, "~r~TD07 red ~g~green ~b~blue~n~~w~second line",
		TEXT_DRAW_FONT_1, TEXT_DRAW_ALIGN_LEFT, 0.27, 1.00, 330.0, 36.0,
		0xFFFFFFFF, 0x00000088, 0x000000FF, true, 1, 0, true, false);
	gGlobalTextDraws[8] = CreateGlobalProbeTextDraw(22.0, 236.0, "TD08 under_score and ~k~~VEHICLE_ENTER_EXIT~",
		TEXT_DRAW_FONT_1, TEXT_DRAW_ALIGN_LEFT, 0.28, 1.00, 390.0, 20.0,
		0xFFDDAAFF, 0x00000000, 0x000000FF, false, 1, 0, true, false);
	gGlobalTextDraws[9] = CreateGlobalProbeTextDraw(22.0, 268.0, "TD09 selectable left",
		TEXT_DRAW_FONT_1, TEXT_DRAW_ALIGN_LEFT, 0.32, 1.10, 210.0, 22.0,
		0xFFFFFFFF, 0x11448899, 0x000000FF, true, 1, 0, true, true);
	gGlobalTextDraws[10] = CreateGlobalProbeTextDraw(320.0, 268.0, "TD10 selectable centre",
		TEXT_DRAW_FONT_1, TEXT_DRAW_ALIGN_CENTER, 0.32, 1.10, 140.0, 22.0,
		0xFFFFFFFF, 0x66114499, 0x000000FF, true, 1, 0, true, true);
	gGlobalTextDraws[11] = CreateGlobalProbeTextDraw(618.0, 268.0, "TD11 selectable right",
		TEXT_DRAW_FONT_1, TEXT_DRAW_ALIGN_RIGHT, 0.32, 1.10, 450.0, 22.0,
		0xFFFFFFFF, 0x44661199, 0x000000FF, true, 1, 0, true, true);
	gGlobalTextDraws[12] = CreateGlobalProbeTextDraw(420.0, 92.0, "LD_SPAC:white",
		TEXT_DRAW_FONT_SPRITE_DRAW, TEXT_DRAW_ALIGN_LEFT, 0.0, 0.0, 84.0, 42.0,
		0x88CCFFFF, 0x00000000, 0xFFFFFFFF, false, 0, 0, true, true);
	gGlobalTextDraws[13] = CreateGlobalProbeTextDraw(520.0, 92.0, "_",
		TEXT_DRAW_FONT_MODEL_PREVIEW, TEXT_DRAW_ALIGN_LEFT, 0.0, 0.0, 70.0, 70.0,
		0xFFFFFFFF, 0x00000000, 0x202020FF, false, 0, 0, true, true);
	gGlobalTextDraws[14] = CreateGlobalProbeTextDraw(420.0, 174.0, "_",
		TEXT_DRAW_FONT_MODEL_PREVIEW, TEXT_DRAW_ALIGN_LEFT, 0.0, 0.0, 82.0, 58.0,
		0xFFFFFFFF, 0x00000000, 0x202040FF, false, 0, 0, true, true);
	gGlobalTextDraws[15] = CreateGlobalProbeTextDraw(22.0, 306.0, "TD15 set-string target",
		TEXT_DRAW_FONT_1, TEXT_DRAW_ALIGN_LEFT, 0.32, 1.10, 286.0, 22.0,
		0xFFFFFFFF, 0x22446699, 0x000000FF, true, 1, 0, true, true);
	gGlobalTextDraws[16] = CreateGlobalProbeTextDraw(22.0, 338.0, "TD16 per-player set-string target",
		TEXT_DRAW_FONT_1, TEXT_DRAW_ALIGN_LEFT, 0.30, 1.05, 330.0, 22.0,
		0xFFFFCCFF, 0x44221199, 0x000000FF, true, 1, 0, true, true);
	gGlobalTextDraws[17] = CreateGlobalProbeTextDraw(22.0, 370.0, "TD17 hide/show target",
		TEXT_DRAW_FONT_1, TEXT_DRAW_ALIGN_LEFT, 0.32, 1.10, 260.0, 22.0,
		0xCCFFFFFF, 0x222222AA, 0x000000FF, true, 1, 0, true, true);
	gGlobalTextDraws[18] = CreateGlobalProbeTextDraw(420.0, 132.0, "LD_SPAC:black",
		TEXT_DRAW_FONT_SPRITE_DRAW, TEXT_DRAW_ALIGN_LEFT, 0.0, 0.0, 84.0, 42.0,
		0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, false, 0, 0, true, true);
	gGlobalTextDraws[19] = CreateGlobalProbeTextDraw(520.0, 132.0, "LD_BEAT:chit",
		TEXT_DRAW_FONT_SPRITE_DRAW, TEXT_DRAW_ALIGN_LEFT, 0.0, 0.0, 44.0, 44.0,
		0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, false, 0, 0, true, true);
	gGlobalTextDraws[20] = CreateGlobalProbeTextDraw(520.0, 174.0, "_",
		TEXT_DRAW_FONT_MODEL_PREVIEW, TEXT_DRAW_ALIGN_LEFT, 0.0, 0.0, 70.0, 62.0,
		0xFFFFFFFF, 0x00000000, 0x202020FF, false, 0, 0, true, true);
	gGlobalTextDraws[21] = CreateGlobalProbeTextDraw(420.0, 242.0, "_",
		TEXT_DRAW_FONT_MODEL_PREVIEW, TEXT_DRAW_ALIGN_LEFT, 0.0, 0.0, 82.0, 62.0,
		0xFFFFFFFF, 0x00000000, 0x202020FF, false, 0, 0, true, true);
	gGlobalTextDraws[22] = CreateGlobalProbeTextDraw(520.0, 242.0, "_",
		TEXT_DRAW_FONT_MODEL_PREVIEW, TEXT_DRAW_ALIGN_LEFT, 0.0, 0.0, 82.0, 62.0,
		0xFFFFFFFF, 0x00000000, 0x202020FF, false, 0, 0, true, true);
	gGlobalTextDraws[23] = CreateGlobalProbeTextDraw(420.0, 312.0, "_",
		TEXT_DRAW_FONT_MODEL_PREVIEW, TEXT_DRAW_ALIGN_LEFT, 0.0, 0.0, 82.0, 58.0,
		0xFFFFFFFF, 0x00000000, 0x202040FF, false, 0, 0, true, true);

	TextDrawSetPreviewModel(gGlobalTextDraws[13], 0);
	TextDrawSetPreviewRot(gGlobalTextDraws[13], -12.0, 0.0, -22.0, 1.00);
	TextDrawSetPreviewModel(gGlobalTextDraws[14], 411);
	TextDrawSetPreviewRot(gGlobalTextDraws[14], -8.0, 0.0, -36.0, 0.85);
	TextDrawSetPreviewVehCol(gGlobalTextDraws[14], 3, 1);
	TextDrawSetPreviewModel(gGlobalTextDraws[20], 7);
	TextDrawSetPreviewRot(gGlobalTextDraws[20], -12.0, 0.0, -18.0, 1.00);
	TextDrawSetPreviewModel(gGlobalTextDraws[21], 19316);
	TextDrawSetPreviewRot(gGlobalTextDraws[21], -18.0, 0.0, 24.0, 0.80);
	TextDrawSetPreviewModel(gGlobalTextDraws[22], 19894);
	TextDrawSetPreviewRot(gGlobalTextDraws[22], -18.0, 0.0, -36.0, 0.80);
	TextDrawSetPreviewModel(gGlobalTextDraws[23], 522);
	TextDrawSetPreviewRot(gGlobalTextDraws[23], -8.0, 0.0, -28.0, 0.78);
	TextDrawSetPreviewVehCol(gGlobalTextDraws[23], 6, 6);

	gGlobalTextDrawsBuilt = true;
	print("[tdprobe] global textdraw probe built");
	return 1;
}

stock DestroyPlayerTextDrawProbe(playerid)
{
	if (!gPlayerTextDrawsBuilt[playerid])
	{
		ResetPlayerTextDrawSlots(playerid);
		return 1;
	}

	for (new i = 0; i < TD_PLAYER_COUNT; i++)
	{
		if (gPlayerTextDraws[playerid][i] != PlayerText:INVALID_PLAYER_TEXT_DRAW)
		{
			PlayerTextDrawDestroy(playerid, gPlayerTextDraws[playerid][i]);
		}
	}
	ResetPlayerTextDrawSlots(playerid);
	printf("[tdprobe] player textdraw probe destroyed player=%d", playerid);
	return 1;
}

stock BuildPlayerTextDrawProbe(playerid)
{
	DestroyPlayerTextDrawProbe(playerid);

	gPlayerTextDraws[playerid][0] = CreatePlayerProbeTextDraw(playerid, 420.0, 246.0, "PTD00 player textdraw",
		TEXT_DRAW_FONT_1, TEXT_DRAW_ALIGN_LEFT, 0.30, 1.00, 610.0, 20.0,
		0xFFFFFFFF, 0x00000088, 0x000000FF, true, 1, 0, true, false);
	gPlayerTextDraws[playerid][1] = CreatePlayerProbeTextDraw(playerid, 420.0, 278.0, "PTD01 selectable",
		TEXT_DRAW_FONT_1, TEXT_DRAW_ALIGN_LEFT, 0.30, 1.00, 610.0, 20.0,
		0xFFFFFFFF, 0x66330099, 0x000000FF, true, 1, 0, true, true);
	gPlayerTextDraws[playerid][2] = CreatePlayerProbeTextDraw(playerid, 520.0, 310.0, "PTD02 centre",
		TEXT_DRAW_FONT_2, TEXT_DRAW_ALIGN_CENTER, 0.30, 1.00, 160.0, 20.0,
		0xCCFFCCFF, 0x00336699, 0x000000FF, true, 1, 0, true, true);
	gPlayerTextDraws[playerid][3] = CreatePlayerProbeTextDraw(playerid, 420.0, 342.0, "LD_BEAT:chit",
		TEXT_DRAW_FONT_SPRITE_DRAW, TEXT_DRAW_ALIGN_LEFT, 0.0, 0.0, 48.0, 48.0,
		0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, false, 0, 0, true, true);
	gPlayerTextDraws[playerid][4] = CreatePlayerProbeTextDraw(playerid, 480.0, 342.0, "_",
		TEXT_DRAW_FONT_MODEL_PREVIEW, TEXT_DRAW_ALIGN_LEFT, 0.0, 0.0, 70.0, 54.0,
		0xFFFFFFFF, 0x00000000, 0x202020FF, false, 0, 0, true, true);
	gPlayerTextDraws[playerid][5] = CreatePlayerProbeTextDraw(playerid, 420.0, 410.0, "PTD05 set-string target",
		TEXT_DRAW_FONT_1, TEXT_DRAW_ALIGN_LEFT, 0.28, 1.00, 620.0, 20.0,
		0xFFFFAAFF, 0x00000066, 0x000000FF, true, 1, 0, true, true);
	gPlayerTextDraws[playerid][6] = CreatePlayerProbeTextDraw(playerid, 520.0, 410.0, "PTD06 outline",
		TEXT_DRAW_FONT_3, TEXT_DRAW_ALIGN_LEFT, 0.34, 1.20, 620.0, 22.0,
		0xFFFFFFFF, 0x00000000, 0x2244FFFF, false, 0, 1, true, false);
	gPlayerTextDraws[playerid][7] = CreatePlayerProbeTextDraw(playerid, 560.0, 342.0, "_",
		TEXT_DRAW_FONT_MODEL_PREVIEW, TEXT_DRAW_ALIGN_LEFT, 0.0, 0.0, 62.0, 54.0,
		0xFFFFFFFF, 0x00000000, 0x202020FF, false, 0, 0, true, true);
	gPlayerTextDraws[playerid][8] = CreatePlayerProbeTextDraw(playerid, 560.0, 282.0, "_",
		TEXT_DRAW_FONT_MODEL_PREVIEW, TEXT_DRAW_ALIGN_LEFT, 0.0, 0.0, 62.0, 54.0,
		0xFFFFFFFF, 0x00000000, 0x202020FF, false, 0, 0, true, true);
	gPlayerTextDraws[playerid][9] = CreatePlayerProbeTextDraw(playerid, 560.0, 222.0, "LD_SPAC:black",
		TEXT_DRAW_FONT_SPRITE_DRAW, TEXT_DRAW_ALIGN_LEFT, 0.0, 0.0, 58.0, 32.0,
		0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, false, 0, 0, true, true);

	PlayerTextDrawSetPreviewModel(playerid, gPlayerTextDraws[playerid][4], 522);
	PlayerTextDrawSetPreviewRot(playerid, gPlayerTextDraws[playerid][4], -10.0, 0.0, -28.0, 0.80);
	PlayerTextDrawSetPreviewVehCol(playerid, gPlayerTextDraws[playerid][4], 6, 6);
	PlayerTextDrawSetPreviewModel(playerid, gPlayerTextDraws[playerid][7], 19316);
	PlayerTextDrawSetPreviewRot(playerid, gPlayerTextDraws[playerid][7], -16.0, 0.0, 18.0, 0.78);
	PlayerTextDrawSetPreviewModel(playerid, gPlayerTextDraws[playerid][8], 19894);
	PlayerTextDrawSetPreviewRot(playerid, gPlayerTextDraws[playerid][8], -16.0, 0.0, -34.0, 0.78);

	gPlayerTextDrawsBuilt[playerid] = true;
	printf("[tdprobe] player textdraw probe built player=%d", playerid);
	return 1;
}

stock ShowTextDrawProbe(playerid)
{
	if (!gGlobalTextDrawsBuilt)
	{
		BuildGlobalTextDrawProbe();
	}
	if (!gPlayerTextDrawsBuilt[playerid])
	{
		BuildPlayerTextDrawProbe(playerid);
	}

	for (new i = 0; i < TD_GLOBAL_COUNT; i++)
	{
		if (gGlobalTextDraws[i] != Text:INVALID_TEXT_DRAW)
		{
			TextDrawShowForPlayer(playerid, gGlobalTextDraws[i]);
		}
	}
	for (new i = 0; i < TD_PLAYER_COUNT; i++)
	{
		if (gPlayerTextDraws[playerid][i] != PlayerText:INVALID_PLAYER_TEXT_DRAW)
		{
			PlayerTextDrawShow(playerid, gPlayerTextDraws[playerid][i]);
		}
	}
	SendClientMessage(playerid, 0x66FF66FF, "[tdprobe] shown. Use /tdselect, /tdstrings, /tdhide, /tdshow, /tdrebuild.");
	printf("[tdprobe] show player=%d", playerid);
	return 1;
}

stock HideTextDrawProbe(playerid)
{
	for (new i = 0; i < TD_GLOBAL_COUNT; i++)
	{
		if (gGlobalTextDraws[i] != Text:INVALID_TEXT_DRAW)
		{
			TextDrawHideForPlayer(playerid, gGlobalTextDraws[i]);
		}
	}
	for (new i = 0; i < TD_PLAYER_COUNT; i++)
	{
		if (gPlayerTextDraws[playerid][i] != PlayerText:INVALID_PLAYER_TEXT_DRAW)
		{
			PlayerTextDrawHide(playerid, gPlayerTextDraws[playerid][i]);
		}
	}
	SendClientMessage(playerid, 0xFFCC66FF, "[tdprobe] hidden.");
	printf("[tdprobe] hide player=%d", playerid);
	return 1;
}

stock UpdateTextDrawProbeStrings(playerid)
{
	gTextDrawVersion[playerid]++;

	new line[96];
	format(line, sizeof(line), "TD15 TextDrawSetString v%d", gTextDrawVersion[playerid]);
	TextDrawSetString(gGlobalTextDraws[15], line);

	format(line, sizeof(line), "TD16 TextDrawSetStringForPlayer p%d v%d", playerid, gTextDrawVersion[playerid]);
	TextDrawSetStringForPlayer(gGlobalTextDraws[16], playerid, line);

	format(line, sizeof(line), "PTD05 PlayerTextDrawSetString p%d v%d", playerid, gTextDrawVersion[playerid]);
	PlayerTextDrawSetString(playerid, gPlayerTextDraws[playerid][5], line);

	SendClientMessage(playerid, 0x66CCFFFF, "[tdprobe] string updates sent.");
	printf("[tdprobe] string update player=%d version=%d", playerid, gTextDrawVersion[playerid]);
	return 1;
}

stock CycleTextDrawProbePreview(playerid)
{
	new color = (gTextDrawVersion[playerid] % 8) + 1;
	new Float:angle = float(gTextDrawVersion[playerid] * 23);

	TextDrawSetPreviewRot(gGlobalTextDraws[13], -12.0, 0.0, angle, 1.00);
	TextDrawSetPreviewRot(gGlobalTextDraws[14], -8.0, 0.0, angle, 0.85);
	TextDrawSetPreviewVehCol(gGlobalTextDraws[14], color, color + 1);
	TextDrawSetPreviewRot(gGlobalTextDraws[20], -12.0, 0.0, angle, 1.00);
	TextDrawSetPreviewRot(gGlobalTextDraws[21], -18.0, 0.0, angle, 0.80);
	TextDrawSetPreviewRot(gGlobalTextDraws[22], -18.0, 0.0, angle, 0.80);
	TextDrawSetPreviewRot(gGlobalTextDraws[23], -8.0, 0.0, angle, 0.78);
	TextDrawSetPreviewVehCol(gGlobalTextDraws[23], color + 4, color + 5);
	TextDrawShowForPlayer(playerid, gGlobalTextDraws[13]);
	TextDrawShowForPlayer(playerid, gGlobalTextDraws[14]);
	TextDrawShowForPlayer(playerid, gGlobalTextDraws[20]);
	TextDrawShowForPlayer(playerid, gGlobalTextDraws[21]);
	TextDrawShowForPlayer(playerid, gGlobalTextDraws[22]);
	TextDrawShowForPlayer(playerid, gGlobalTextDraws[23]);

	PlayerTextDrawSetPreviewRot(playerid, gPlayerTextDraws[playerid][4], -10.0, 0.0, angle, 0.80);
	PlayerTextDrawSetPreviewVehCol(playerid, gPlayerTextDraws[playerid][4], color + 2, color + 3);
	PlayerTextDrawSetPreviewRot(playerid, gPlayerTextDraws[playerid][7], -16.0, 0.0, angle, 0.78);
	PlayerTextDrawSetPreviewRot(playerid, gPlayerTextDraws[playerid][8], -16.0, 0.0, angle, 0.78);
	PlayerTextDrawShow(playerid, gPlayerTextDraws[playerid][4]);
	PlayerTextDrawShow(playerid, gPlayerTextDraws[playerid][7]);
	PlayerTextDrawShow(playerid, gPlayerTextDraws[playerid][8]);

	SendClientMessage(playerid, 0x66CCFFFF, "[tdprobe] preview rotation/colours updated and re-shown.");
	printf("[tdprobe] preview cycle player=%d angle=%.3f color=%d", playerid, angle, color);
	return 1;
}

stock SendTextDrawProbeHelp(playerid)
{
	SendClientMessage(playerid, 0xFFFFFFFF, "[tdprobe] /tdshow /tdhide /tdselect /tdcancel /tdstrings /tdpreview /tdrebuild");
	SendClientMessage(playerid, 0xFFFFFFFF, "[tdprobe] Click selectable global and player textdraws; ESC should report INVALID_TEXT_DRAW.");
	return 1;
}

stock SetupTextDrawProbePlayer(playerid)
{
	SetPlayerInterior(playerid, 0);
	SetPlayerVirtualWorld(playerid, 0);
	SetPlayerPos(playerid, SPAWN_X, SPAWN_Y, SPAWN_Z);
	SetPlayerFacingAngle(playerid, SPAWN_A);
	SetCameraBehindPlayer(playerid);
	TogglePlayerClock(playerid, false);
	ShowTextDrawProbe(playerid);
	return 1;
}

/// Initializes the focused TextDraw probe mode.
/// Reference: https://open.mp/docs/scripting/functions/TextDrawCreate
public OnGameModeInit()
{
	SetGameModeText(TD_MODE_NAME);
	ShowPlayerMarkers(PLAYER_MARKERS_MODE_GLOBAL);
	ShowNameTags(true);
	AddPlayerClass(0, SPAWN_X, SPAWN_Y, SPAWN_Z, SPAWN_A, 0, 0, 0, 0, 0, 0);
	BuildGlobalTextDrawProbe();
	print("\n----------------------------------");
	print("  TextDraw 0.3.7 probe");
	print("----------------------------------\n");
	return 1;
}

/// Cleans up global TextDraws on gamemode shutdown.
/// Reference: https://open.mp/docs/scripting/functions/TextDrawDestroy
public OnGameModeExit()
{
	if (gGlobalTextDrawsBuilt)
	{
		for (new i = 0; i < TD_GLOBAL_COUNT; i++)
		{
			if (gGlobalTextDraws[i] != Text:INVALID_TEXT_DRAW)
			{
				TextDrawDestroy(gGlobalTextDraws[i]);
			}
		}
	}
	ResetGlobalTextDrawSlots();
	return 1;
}

/// Builds per-player TextDraws when the player connects.
/// Reference: https://open.mp/docs/scripting/functions/CreatePlayerTextDraw
public OnPlayerConnect(playerid)
{
	ResetPlayerTextDrawSlots(playerid);
	BuildPlayerTextDrawProbe(playerid);
	SendTextDrawProbeHelp(playerid);
	return 1;
}

/// Destroys per-player TextDraws on disconnect.
/// Reference: https://open.mp/docs/scripting/functions/PlayerTextDrawDestroy
public OnPlayerDisconnect(playerid, reason)
{
	#pragma unused reason

	DestroyPlayerTextDrawProbe(playerid);
	return 1;
}

/// Places the player and shows the TextDraw matrix after spawn.
/// Reference: https://open.mp/docs/scripting/callbacks/OnPlayerSpawn
public OnPlayerSpawn(playerid)
{
	SetupTextDrawProbePlayer(playerid);
	return 1;
}

/// Provides minimal commands for deterministic TextDraw RPC traces.
/// Reference: https://open.mp/docs/scripting/functions/SelectTextDraw
public OnPlayerCommandText(playerid, cmdtext[])
{
	if (!strcmp(cmdtext, "/tdhelp", true))
	{
		SendTextDrawProbeHelp(playerid);
		return 1;
	}
	if (!strcmp(cmdtext, "/tdshow", true))
	{
		ShowTextDrawProbe(playerid);
		return 1;
	}
	if (!strcmp(cmdtext, "/tdhide", true))
	{
		HideTextDrawProbe(playerid);
		return 1;
	}
	if (!strcmp(cmdtext, "/tdselect", true))
	{
		SelectTextDraw(playerid, TD_SELECT_COLOR);
		SendClientMessage(playerid, 0x66CCFFFF, "[tdprobe] select mode enabled.");
		printf("[tdprobe] select enabled player=%d color=0x%08x", playerid, TD_SELECT_COLOR);
		return 1;
	}
	if (!strcmp(cmdtext, "/tdcancel", true))
	{
		CancelSelectTextDraw(playerid);
		printf("[tdprobe] select cancel requested player=%d", playerid);
		return 1;
	}
	if (!strcmp(cmdtext, "/tdstrings", true))
	{
		UpdateTextDrawProbeStrings(playerid);
		return 1;
	}
	if (!strcmp(cmdtext, "/tdpreview", true))
	{
		CycleTextDrawProbePreview(playerid);
		return 1;
	}
	if (!strcmp(cmdtext, "/tdrebuild", true))
	{
		BuildGlobalTextDrawProbe();
		BuildPlayerTextDrawProbe(playerid);
		ShowTextDrawProbe(playerid);
		printf("[tdprobe] rebuild player=%d", playerid);
		return 1;
	}
	return 0;
}

/// Logs global TextDraw click/cancel callbacks.
/// Reference: https://open.mp/docs/scripting/callbacks/OnPlayerClickTextDraw
public OnPlayerClickTextDraw(playerid, Text:clickedid)
{
	if (clickedid == Text:INVALID_TEXT_DRAW)
	{
		printf("[tdprobe] click global cancel player=%d clicked=INVALID_TEXT_DRAW", playerid);
		SendClientMessage(playerid, 0xFFCC66FF, "[tdprobe] global select cancelled.");
		return 1;
	}

	printf("[tdprobe] click global player=%d textdraw=%d", playerid, _:clickedid);
	SendClientMessage(playerid, 0x66FF66FF, "[tdprobe] global textdraw clicked.");
	return 1;
}

/// Logs per-player TextDraw click callbacks.
/// Reference: https://open.mp/docs/scripting/callbacks/OnPlayerClickPlayerTextDraw
public OnPlayerClickPlayerTextDraw(playerid, PlayerText:playertextid)
{
	printf("[tdprobe] click player player=%d playertextdraw=%d", playerid, _:playertextid);
	SendClientMessage(playerid, 0x66FF66FF, "[tdprobe] player textdraw clicked.");
	return 1;
}
