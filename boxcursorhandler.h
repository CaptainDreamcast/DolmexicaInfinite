#pragma once

#include <prism/actorhandler.h>
#include <prism/geometry.h>

ActorBlueprint getBoxCursorHandler();

int addBoxCursor(Position tStartPosition, Position tOffset, GeoRectangle tRectangle);
void removeBoxCursor(int tID);
void setBoxCursorPosition(int tID, Position tPosition);
void pauseBoxCursor(int tID);
void resumeBoxCursor(int tID);