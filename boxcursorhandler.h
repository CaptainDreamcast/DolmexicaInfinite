#pragma once

#include <prism/actorhandler.h>
#include <prism/geometry.h>

ActorBlueprint getBoxCursorHandler();

int addBoxCursor(const Position& tStartPosition, const Position& tOffset, const GeoRectangle2D& tRectangle);
void removeBoxCursor(int tID);
void setBoxCursorPosition(int tID, const Position& tPosition);
void pauseBoxCursor(int tID);
void resumeBoxCursor(int tID);