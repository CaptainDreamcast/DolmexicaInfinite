#pragma once

#include <prism/actorhandler.h>
#include <prism/geometry.h>

extern ActorBlueprint BoxCursorHandler;

int addBoxCursor(Position tStartPosition, Position tOffset, GeoRectangle tRectangle);
void removeBoxCursor(int tID);
void setBoxCursorPosition(int tID, Position tPosition);