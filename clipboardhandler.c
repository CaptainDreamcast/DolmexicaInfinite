#include "clipboardhandler.h"

#include <string.h>

#include <prism/mugentexthandler.h>
#include <prism/log.h>
#include <prism/system.h>

#define CLIPBOARD_LINE_AMOUNT 10

static struct {
	int mLineAmount;
	char mLines[CLIPBOARD_LINE_AMOUNT][1024];

	int mTextIDs[CLIPBOARD_LINE_AMOUNT];
} gData;

void initClipboardForGame() {
	memset(gData.mLines, 0, sizeof gData.mLines);
	gData.mLineAmount = 0;
}

static void initClipboardLines() {
	

	int font = isOnDreamcast() ? 1 : -1;
	double deltaY = getMugenFontSizeY(font) + getMugenFontSpacingY(font);
	Position pos = makePosition(20, 20, 90);
	int i;
	for (i = 0; i < CLIPBOARD_LINE_AMOUNT; i++) {
		gData.mTextIDs[i] = addMugenText("", pos, font);
		pos.y += deltaY;
	}
}

static void setClipboardLineTexts() {
	int i;
	for (i = 0; i < gData.mLineAmount; i++) {
		changeMugenText(gData.mTextIDs[i], gData.mLines[i]);
	}
}

static void loadClipboardHandler(void* tData) {
	initClipboardLines();
	setClipboardLineTexts();
}

ActorBlueprint ClipboardHandler = {
	.mLoad = loadClipboardHandler,
};

static void moveClipboardLinesDown() {
	int i;
	for (i = 1; i < gData.mLineAmount; i++) {
		strcpy(gData.mLines[i - 1], gData.mLines[i]);
	}

	gData.mLineAmount--;
}

void addClipboardLine(char* tLine) {
	if (gData.mLineAmount == CLIPBOARD_LINE_AMOUNT) {
		moveClipboardLinesDown();
	}

	strcpy(gData.mLines[gData.mLineAmount], tLine);
	gData.mLineAmount++;

	setClipboardLineTexts();
}

static void getArgumentTextAndAdvanceParams(char* tArgumentText, char** tParams) {
	if (*tParams == NULL) {
		sprintf(tArgumentText, "");
		return;
	}

	char* nextComma = strchr(*tParams, ',');
	if (nextComma) *nextComma = '\0';

	sprintf(tArgumentText, "%s", *tParams);

	*tParams = nextComma + 1;
}

static void parseParameterInput(char* tFormatString, int* i, char** tDst, char** tParams) {

	(*i)++;
	char identifier = tFormatString[*i];

	char argumentText[100];
	char parsedValue[100];

	if (identifier == '%') {
		**tDst = '%';
		(*tDst)++;
		return;
	}
	else if(identifier == 'd' || identifier == 'i') {
		getArgumentTextAndAdvanceParams(argumentText, tParams);
		int val = atoi(argumentText); // TODO: fix
		sprintf(parsedValue, "%d", val);
		int len = strlen(parsedValue);
		memcpy(*tDst, parsedValue, len);
		*tDst += len;
	}
	else if (identifier == 'f' || identifier == 'F') {
		getArgumentTextAndAdvanceParams(argumentText, tParams);
		double val = atof(argumentText); // TODO: fix
		sprintf(parsedValue, "%f", val);
		int len = strlen(parsedValue);
		memcpy(*tDst, parsedValue, len);
		*tDst += len;
	}
	else if (identifier == 'e' || identifier == 'E') { // TODO: uppercase
		getArgumentTextAndAdvanceParams(argumentText, tParams);
		double val = atof(argumentText); // TODO: fix
		sprintf(parsedValue, "%e", val);
		int len = strlen(parsedValue);
		memcpy(*tDst, parsedValue, len);
		*tDst += len;
	}
	else if (identifier == 'g' || identifier == 'G') { // TODO: uppercase
		getArgumentTextAndAdvanceParams(argumentText, tParams);
		double val = atof(argumentText); // TODO: fix
		sprintf(parsedValue, "%g", val);
		int len = strlen(parsedValue);
		memcpy(*tDst, parsedValue, len);
		*tDst += len;
	}
	else {
		logError("Unrecognized parsing parameter");
		logErrorString(&tFormatString[*i - 1]);
		abortSystem();
	}



}

static void parseLinebreak(char** tDst, char* tTextBuffer) {
	**tDst = '\0';
	addClipboardLine(tTextBuffer);
	*tDst = tTextBuffer;
}

static void parseTab(char** tDst) {
	int j;
	for (j = 0; j < 4; j++) {
		**tDst = ' ';
		(*tDst)++;
	}
}

static void parseFormatInput(char* tFormatString, int* i, char** tDst, char* tTextBuffer) {
	(*i)++;
	char identifier = tFormatString[*i];

	if (identifier == '\\') {
		**tDst = '\\';
		(*tDst)++;
	}
	else if (identifier == 't') {
		parseTab(tDst);
	}
	else if (identifier == 'n') {
		parseLinebreak(tDst, tTextBuffer);
	}
	else {
		logError("Unrecognized format parameter");
		logErrorString(&tFormatString[*i - 1]);
		abortSystem();
	}
}

void addClipboardLineFormatString(char * tFormatString, char * tParameterString)
{
	// TODO: parse properly
	char text[1024];
	char* dst;
	char paramBuffer[200];

	strcpy(paramBuffer, tParameterString);
	char* param = paramBuffer;

	dst = text;
	int len = strlen(tFormatString);
	int i;
	for (i = 0; i < len; i++) {
		if (tFormatString[i] == '%') {
			parseParameterInput(tFormatString, &i, &dst, &param);
		} else if(tFormatString[i] == '\\') {
			parseFormatInput(tFormatString, &i, &dst, text);
		}
		else if (tFormatString[i] == '\n') {
			parseLinebreak(&dst, text);
		}
		else if (tFormatString[i] == '\t') {
			parseTab(&dst);
		}
		else {
			*dst = tFormatString[i];
			dst++;
		}
	}

	*dst = '\0';
	if (dst != text) {
		addClipboardLine(text);
	}
}
