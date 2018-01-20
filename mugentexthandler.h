#pragma once

#include <tari/actorhandler.h>
#include <tari/geometry.h>
#include <tari/drawing.h>

typedef enum {
	MUGEN_TEXT_ALIGNMENT_LEFT,
	MUGEN_TEXT_ALIGNMENT_CENTER,
	MUGEN_TEXT_ALIGNMENT_RIGHT,
} MugenTextAlignment;

void addMugenFont(int tKey, char* tPath);
void addMugenFont2(int tKey, char* tPath);
void loadMugenTextHandler();

extern ActorBlueprint MugenTextHandler;

int addMugenText(char* tText, Position tPosition, int tFont);
void setMugenTextFont(int tID, int tFont);
void setMugenTextAlignment(int tID, MugenTextAlignment tAlignment);
void setMugenTextColor(int tID, Color tColor);
void setMugenTextRectangle(int tID, GeoRectangle tRectangle);
void setMugenTextPosition(int tID, Position tPosition);

MugenTextAlignment getMugenTextAlignmentFromMugenAlignmentIndex(int tIndex);
Color getMugenTextColorFromMugenTextColorIndex(int tIndex);