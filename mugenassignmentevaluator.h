#pragma once

#include "mugenassignment.h"
#include "playerdefinition.h"

void setupDreamAssignmentEvaluator();
void setupDreamStoryAssignmentEvaluator();

int evaluateDreamAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer);
double evaluateDreamAssignmentAndReturnAsFloat(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer);
int evaluateDreamAssignmentAndReturnAsInteger(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer);
char* evaluateDreamAssignmentAndReturnAsAllocatedString(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer);
Vector3D evaluateDreamAssignmentAndReturnAsVector3D(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer);
Vector3DI evaluateDreamAssignmentAndReturnAsVector3DI(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer);
