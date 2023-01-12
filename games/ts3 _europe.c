//==========================================================================
// Mouse Injector for Dolphin
//==========================================================================
// Copyright (C) 2019-2022 Carnivorous (lightly modified by NervyDestroyer)
// All rights reserved.
//
// Mouse Injector is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, visit http://www.gnu.org/licenses/gpl-2.0.html
//==========================================================================
#include <stdint.h>
#include "../main.h"
#include "../memory.h"
#include "../mouse.h"
#include "game.h"

#define MAPMAKERXMIN 1.f // mapmaker cursor limits
#define MAPMAKERYMIN 0.f
#define MAPMAKERXMAX 98.f
#define MAPMAKERYMAX 96.f
// TS3 ADDRESSES - OFFSET ADDRESSES BELOW (REQUIRES PLAYERBASE TO USE)
#define TS3_camx 0x810D7280 - 0x810D7180
#define TS3_camy 0x810D7284 - 0x810D7180
#define TS3_fov 0x80B7D594 - 0x80B7D210
#define TS3_crosshairx 0x810D7F7C - 0x810D7180
#define TS3_crosshairy 0x810D7F80 - 0x810D7180
#define TS3_gunx 0x810D7F84 - 0x810D7180
#define TS3_guny 0x810D7F88 - 0x810D7180
// STATIC ADDRESSES BELOW
#define TS3_playerbase 0x80611D74 // playable character pointer
#define TS3_fovbase 0x80611D5C // fov base pointer
#define TS3_crosshairsetting 0x80501680 // crosshair settings (1 == on and moving)
#define TS3_yaxislimit 0x80611D7C
#define TS3_mapmakerx 0x8051A82C
#define TS3_mapmakery 0x8051A830

static uint8_t TS3E_Status(void);
static void TS3E_Inject(void);

static const GAMEDRIVER GAMEDRIVER_INTERFACE =
{
	"TimeSplitters: Future Perfect (Europe PAL)",
	TS3E_Status,
	TS3E_Inject,
	1, // 1000 Hz tickrate
	1 // crosshair sway supported for driver
};

const GAMEDRIVER *GAME_TS3_EUROPE = &GAMEDRIVER_INTERFACE;

//==========================================================================
// Purpose: return 1 if game is detected
//==========================================================================
static uint8_t TS3E_Status(void)
{
	return (MEM_ReadUInt(0x80000000) == 0x47334650U && MEM_ReadUInt(0x80000004) == 0x36390000U); // check game header to see if it matches TS3
}
//==========================================================================
// Purpose: calculate mouse look and inject into current game
//==========================================================================
static void TS3E_Inject(void)
{
	if(xmouse == 0 && ymouse == 0) // if mouse is idle
		return;
	if(MEM_ReadInt(TS3_yaxislimit) != 0x42A00000) // overwrite y axis limit from 60 degrees (SP) or 75 degrees (MP)
		MEM_WriteFloat(TS3_yaxislimit, 80.f);
	const float looksensitivity = (float)sensitivity / 40.f;
	const float crosshairsensitivity = ((float)crosshair / 100.f) * looksensitivity;
	float cursorx = MEM_ReadFloat(TS3_mapmakerx), cursory = MEM_ReadFloat(TS3_mapmakery);
	if(cursorx >= MAPMAKERXMIN && cursorx <= MAPMAKERXMAX && cursory >= MAPMAKERYMIN && cursory <= MAPMAKERYMAX) // if mapmaker cursor is safe to inject
	{
		cursorx += (float)xmouse / 7.5f * looksensitivity; // calculate mapmaker cursor movement
		cursory += (float)ymouse / 7.5f * looksensitivity;
		MEM_WriteFloat(TS3_mapmakerx, ClampFloat(cursorx, MAPMAKERXMIN, MAPMAKERXMAX));
		MEM_WriteFloat(TS3_mapmakery, ClampFloat(cursory, MAPMAKERYMIN, MAPMAKERYMAX));
	}
	const uint32_t playerbase = MEM_ReadUInt(TS3_playerbase);
	const uint32_t fovbase = MEM_ReadUInt(TS3_fovbase);
	if(!playerbase || !fovbase) // if playerbase or fovbase are invalid
		return;
	float camx = MEM_ReadFloat(playerbase + TS3_camx);
	float camy = MEM_ReadFloat(playerbase + TS3_camy);
	const float fov = MEM_ReadFloat(fovbase + TS3_fov);
	const float yaxislimit = MEM_ReadFloat(TS3_yaxislimit);
	if(/*camx >= 0 && camx < 360 &&*/camy >= -yaxislimit && camy <= yaxislimit && fov <= 55 && fov > 3)
	{
		camx -= (float)xmouse / 10.f * looksensitivity * (fov / 55.f); // normal calculation method for X
		camy += (float)(!invertpitch ? -ymouse : ymouse) / 10.f * looksensitivity * (fov / 55.f); // normal calculation method for Y
		/*while(camx < 0)
			camx += 360;
		while(camx >= 360)
			camx -= 360;*/
		camy = ClampFloat(camy, -yaxislimit, yaxislimit);
		MEM_WriteFloat(playerbase + TS3_camx, camx);
		MEM_WriteFloat(playerbase + TS3_camy, camy);
		if(crosshair && MEM_ReadInt(TS3_crosshairsetting) <= 1) // if crosshair sway is enabled and turned on in-game
		{
			float crosshairx = MEM_ReadFloat(playerbase + TS3_crosshairx), gunx = MEM_ReadFloat(playerbase + TS3_gunx); // after camera x and y have been calculated and injected, calculate the crosshair/gun sway
			crosshairx += (float)xmouse / 80.f * crosshairsensitivity * (fov / 55.f);
			gunx += (float)xmouse / 80.f * crosshairsensitivity * (fov / 55.f) / 1.5f;
			MEM_WriteFloat(playerbase + TS3_crosshairx, ClampFloat(crosshairx, -1.f, 1.f));
			MEM_WriteFloat(playerbase + TS3_gunx, ClampFloat(gunx, -1.f, 1.f));
			float crosshairy = MEM_ReadFloat(playerbase + TS3_crosshairy), guny = MEM_ReadFloat(playerbase + TS3_guny);
			crosshairy += (float)(!invertpitch ? ymouse : -ymouse) / 80.f * crosshairsensitivity * (fov / 55.f);
			guny += (float)(!invertpitch ? ymouse : -ymouse) / 80.f * crosshairsensitivity * (fov / 55.f) / 1.5f;
			MEM_WriteFloat(playerbase + TS3_crosshairy, ClampFloat(crosshairy, -1.f, 1.f));
			MEM_WriteFloat(playerbase + TS3_guny, ClampFloat(guny, -1.f, 1.f));
		}
	}
}