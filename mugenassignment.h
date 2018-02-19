#pragma once

#include <prism/geometry.h>

#include <prism/mugendefreader.h>

typedef enum {
	MUGEN_ASSIGNMENT_TYPE_FIXED_BOOLEAN,
	MUGEN_ASSIGNMENT_TYPE_AND,
	MUGEN_ASSIGNMENT_TYPE_OR,
	MUGEN_ASSIGNMENT_TYPE_COMPARISON,
	MUGEN_ASSIGNMENT_TYPE_INEQUALITY,
	MUGEN_ASSIGNMENT_TYPE_LESS_OR_EQUAL,
	MUGEN_ASSIGNMENT_TYPE_GREATER_OR_EQUAL,
	MUGEN_ASSIGNMENT_TYPE_VECTOR,
	MUGEN_ASSIGNMENT_TYPE_RANGE,
	MUGEN_ASSIGNMENT_TYPE_NULL,
	MUGEN_ASSIGNMENT_TYPE_NEGATION,
	MUGEN_ASSIGNMENT_TYPE_VARIABLE,
	MUGEN_ASSIGNMENT_TYPE_NUMBER,
	MUGEN_ASSIGNMENT_TYPE_FLOAT,
	MUGEN_ASSIGNMENT_TYPE_STRING,
	MUGEN_ASSIGNMENT_TYPE_ARRAY,
	MUGEN_ASSIGNMENT_TYPE_LESS,
	MUGEN_ASSIGNMENT_TYPE_GREATER,
	MUGEN_ASSIGNMENT_TYPE_ADDITION,
	MUGEN_ASSIGNMENT_TYPE_MULTIPLICATION,
	MUGEN_ASSIGNMENT_TYPE_MODULO,
	MUGEN_ASSIGNMENT_TYPE_SUBTRACTION,
	MUGEN_ASSIGNMENT_TYPE_SET_VARIABLE,
	MUGEN_ASSIGNMENT_TYPE_DIVISION,
	MUGEN_ASSIGNMENT_TYPE_EXPONENTIATION,
	MUGEN_ASSIGNMENT_TYPE_UNARY_MINUS,
	MUGEN_ASSIGNMENT_TYPE_OPERATOR_ARGUMENT,
	MUGEN_ASSIGNMENT_TYPE_BITWISE_AND,
	MUGEN_ASSIGNMENT_TYPE_BITWISE_OR,
} DreamMugenAssignmentType;

typedef struct {
	DreamMugenAssignmentType mType;
	void* mData;

} DreamMugenAssignment;

typedef struct {
	int mValue;
} DreamMugenNumberAssignment;

typedef struct {
	double mValue;
} DreamMugenFloatAssignment;

typedef struct {
	char* mValue;
} DreamMugenStringAssignment;

typedef struct {
	char mName[100];
} DreamMugenVariableAssignment;

typedef struct {
	int mValue;
} DreamMugenFixedBooleanAssignment;

typedef struct {
	int mExcludeLeft;
	int mExcludeRight;
	DreamMugenAssignment* a;
} DreamMugenRangeAssignment;

typedef struct {
	DreamMugenAssignment* a;
	DreamMugenAssignment* b;
} DreamMugenDependOnTwoAssignment;

typedef struct {
	DreamMugenAssignment* a;
} DreamMugenDependOnOneAssignment;

DreamMugenAssignment* makeDreamTrueMugenAssignment();

DreamMugenAssignment* makeDreamFalseMugenAssignment();
void destroyDreamFalseMugenAssignment(DreamMugenAssignment* tAssignment);
void destroyDreamMugenAssignment(DreamMugenAssignment* tAssignment);

DreamMugenAssignment* makeDreamNumberMugenAssignment(int tVal);
DreamMugenAssignment * makeDreamFloatMugenAssignment(double tVal);
DreamMugenAssignment * makeDreamStringMugenAssignment(char* tVal);
DreamMugenAssignment* makeDream2DVectorMugenAssignment(Vector3D tVal);
DreamMugenAssignment* makeDreamAndMugenAssignment(DreamMugenAssignment* a, DreamMugenAssignment* b);
DreamMugenAssignment* makeDreamOrMugenAssignment(DreamMugenAssignment* a, DreamMugenAssignment* b);

DreamMugenAssignment*  parseDreamMugenAssignmentFromString(char* tText);


int fetchDreamAssignmentFromGroupAndReturnWhetherItExists(char* tName, MugenDefScriptGroup* tGroup, DreamMugenAssignment** tOutput);

int doDreamAssignmentStringsBeginsWithPattern(char* tPattern, char* tText);
