#pragma once

#include <string>

#include "mugenassignment.h"
#include "playerdefinition.h"

void setupDreamAssignmentEvaluator();
void setupDreamStoryAssignmentEvaluator();
void setupDreamGlobalAssignmentEvaluator();
void shutdownDreamAssignmentEvaluator();

int evaluateDreamAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer);
double evaluateDreamAssignmentAndReturnAsFloat(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer);
int evaluateDreamAssignmentAndReturnAsInteger(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer);
void evaluateDreamAssignmentAndReturnAsString(std::string& oString, DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer);
Vector2D evaluateDreamAssignmentAndReturnAsVector2D(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer);
Vector3D evaluateDreamAssignmentAndReturnAsVector3D(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer);
Vector2DI evaluateDreamAssignmentAndReturnAsVector2DI(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer);
Vector3DI evaluateDreamAssignmentAndReturnAsVector3DI(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer);

void evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, double* v1, double tDefault1);
void evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, double* v1, double* v2, double tDefault1, double tDefault2);
void evaluateDreamAssignmentAndReturnAsThreeFloatsWithDefaultValues(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, double* v1, double* v2, double* v3, double tDefault1, double tDefault2, double tDefault3);
void evaluateDreamAssignmentAndReturnAsFourFloatsWithDefaultValues(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, double* v1, double* v2, double* v3, double* v4, double tDefault1, double tDefault2, double tDefault3, double tDefault4);

void evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* v1, int tDefault1);
void evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* v1, int* v2, int tDefault1, int tDefault2);
void evaluateDreamAssignmentAndReturnAsThreeIntegersWithDefaultValues(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* v1, int* v2, int* v3, int tDefault1, int tDefault2, int tDefault3);
void evaluateDreamAssignmentAndReturnAsFourIntegersWithDefaultValues(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* v1, int* v2, int* v3, int* v4, int tDefault1, int tDefault2, int tDefault3, int tDefault4);

std::string evaluateMugenDefStringOrDefaultAsGroup(MugenDefScriptGroup* tGroup, const char* tVariableName, const std::string& tDefault);
double evaluateMugenDefFloatOrDefaultAsGroup(MugenDefScriptGroup* tGroup, const char* tVariableName, double tDefault);
int evaluateMugenDefIntegerOrDefaultAsGroup(MugenDefScriptGroup* tGroup, const char* tVariableName, int tDefault);
int evaluateMugenDefIntegerOrDefault(MugenDefScript* tScript, const char* tGroupName, const char* tVariableName, int tDefault);