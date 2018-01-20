#include "mugentexthandler.h"

#include <assert.h>

#include <tari/mugendefreader.h>
#include <tari/mugenanimationhandler.h>
#include <tari/log.h>
#include <tari/system.h>
#include <tari/drawing.h>
#include <tari/texture.h>
#include <tari/math.h>

typedef enum {
	MUGEN_FONT_TYPE_BITMAP,
	MUGEN_FONT_TYPE_TRUETYPE,
	MUGEN_FONT_TYPE_ELECBYTE,
} MugenFontType;

typedef enum {
	MUGEN_BITMAP_FONT_BANK_TYPE_PALETTE,

} MugenBitmapFontBankType;

typedef enum {
	MUGEN_ELECBYTE_FONT_TYPE_VARIABLE,
	MUGEN_ELECBYTE_FONT_TYPE_FIXED,

} MugenElecbyteFontType;

typedef struct {
	int mStartX;
	int mWidth;

} MugenElecbyteFontMapEntry;

typedef struct {
	IntMap mMap;
	MugenSpriteFileSprite* mSprite;

	MugenElecbyteFontType mType;
} MugenElecbyteFont;

typedef struct {
	TruetypeFont mFont;
} MugenTruetypeFont;

typedef struct {
	MugenSpriteFile mSprites;
	MugenBitmapFontBankType mBankType;
} MugenBitmapFont;

typedef struct {
	MugenFontType mType;
	void* mData;

	Vector3DI mSize;
	Vector3DI mSpacing;
	Vector3DI mOffset;
} MugenFont;

static struct {
	IntMap mFonts;
} gData;

static void loadBitmapFont(MugenDefScript* tScript, MugenFont* tFont) {
	MugenBitmapFont* e = allocMemory(sizeof(MugenBitmapFont));

	char* path = getAllocatedMugenDefStringVariable(tScript, "Def", "file");
	e->mSprites = loadMugenSpriteFileWithoutPalette(path);
	freeMemory(path);

	// TODO: banktype

	tFont->mData = e;
}

static void loadMugenTruetypeFont(MugenDefScript* tScript, MugenFont* tFont) {
	MugenTruetypeFont* e = allocMemory(sizeof(MugenTruetypeFont));
	char* name = getAllocatedMugenDefStringVariable(tScript, "Def", "file");
	e->mFont = loadTruetypeFont(name, tFont->mSize.y);
	freeMemory(name);
	tFont->mData = e;
}

typedef struct {
	MugenFont* mFont;
	MugenElecbyteFont* mElecbyteFont;
	int i;
} ElecbyteMapParseCaller;

static void parseSingleMapElement(void* tCaller, void* tData) {
	ElecbyteMapParseCaller* caller = tCaller;
	MugenDefScriptGroupElement* element = tData;
	assert(element->mType == MUGEN_DEF_SCRIPT_GROUP_STRING_ELEMENT);
	MugenDefScriptStringElement* e = element->mData;

	MugenElecbyteFontMapEntry* entry = allocMemory(sizeof(MugenElecbyteFontMapEntry));

	char key[100];
	if (caller->mElecbyteFont->mType == MUGEN_ELECBYTE_FONT_TYPE_VARIABLE) {
		int items = sscanf(e->mString, "%s %d %d", key, &entry->mStartX, &entry->mWidth);
		assert(items == 3);
	}
	else if (caller->mElecbyteFont->mType == MUGEN_ELECBYTE_FONT_TYPE_FIXED) {
		int items = sscanf(e->mString, "%s", key);
		assert(items == 1);
		entry->mStartX = caller->i * caller->mFont->mSize.x;
		entry->mWidth = caller->mFont->mSize.x;
	}

	int keyValue;
	if (strlen(key) == 1) {
		keyValue = key[0];
	}
	else if (key[0] == '0' && key[1] == 'x') {
		sscanf(e->mString, "%i", &keyValue);
	}
	else {
		logError("Unrecognized map key.");
		logErrorString(key);
		abortSystem();
		keyValue = -1;
	}


	int_map_push_owned(&caller->mElecbyteFont->mMap, keyValue, entry);

	caller->i++;
}

static MugenElecbyteFontType getMugenElecbyteFontType(MugenDefScript* tScript) {
	MugenElecbyteFontType ret;

	char* text = getAllocatedMugenDefStringVariable(tScript, "Def", "type");
	turnStringLowercase(text);

	if (!strcmp("fixed", text)) {
		ret = MUGEN_ELECBYTE_FONT_TYPE_FIXED;
	}
	else if (!strcmp("variable", text)) {
		ret = MUGEN_ELECBYTE_FONT_TYPE_VARIABLE;
	}
	else {
		logError("Unable to determine font type.");
		logErrorString(text);
		abortSystem();
		ret = MUGEN_ELECBYTE_FONT_TYPE_VARIABLE;
	}

	freeMemory(text);

	return ret;
}

static void loadMugenElecbyteFont(MugenDefScript* tScript, Buffer tTextureBuffer, MugenFont* tFont) {
	MugenElecbyteFont* e = allocMemory(sizeof(MugenElecbyteFont));

	e->mType = getMugenElecbyteFontType(tScript);

	e->mMap = new_int_map();
	MugenDefScriptGroup* group = string_map_get(&tScript->mGroups, "Map");

	ElecbyteMapParseCaller caller;
	caller.mFont = tFont;
	caller.mElecbyteFont = e;
	caller.i = 0;
	list_map(&group->mOrderedElementList, parseSingleMapElement, &caller);

	e->mSprite = loadSingleTextureFromPCXBuffer(tTextureBuffer);

	tFont->mData = e;
}

static MugenFontType getMugenFontTypeFromScript(MugenDefScript* tScript) {
	char* text = getAllocatedMugenDefStringVariable(tScript, "Def", "type");
	turnStringLowercase(text);

	MugenFontType ret;
	if (!strcmp("bitmap", text)) {
		ret = MUGEN_FONT_TYPE_BITMAP;
	}
	else if (!strcmp("truetype", text)) {
		ret = MUGEN_FONT_TYPE_TRUETYPE;
	}
	else {
		ret = MUGEN_FONT_TYPE_BITMAP;
		logError("Unable to determine font type");
		logErrorString(text);
		abortSystem();
	}

	freeMemory(text);

	return ret;
}

static void setMugenFontDirectory() {

	setWorkingDirectory("/"); // TODO
}

static void setMugenFontDirectory2(char* tPath) {
	char path[1024];
	getPathToFile(path, tPath);
	setWorkingDirectory(path);
}

static void resetMugenFontDirectory() {
	setWorkingDirectory("/");
}

typedef struct {
	uint8_t mMagic[12];
	uint16_t mVersionHigh;
	uint16_t mVersionLow;

	uint32_t mTextureOffset;
	uint32_t mTextureLength;

	uint32_t mTextOffset;
	uint32_t mTextLength;

	uint8_t mComment[40];
} MugenFontHeader;

void addMugenFont(int tKey, char* tPath) {
	setMugenFontDirectory(tPath);

	MugenFontHeader header;

	Buffer b = fileToBuffer(tPath);
	BufferPointer p = getBufferPointer(b);
	readFromBufferPointer(&header, &p, sizeof(MugenFontHeader));

	Buffer textureBuffer = makeBuffer((void*)(((uint32_t)b.mData) + header.mTextureOffset), header.mTextureLength);
	Buffer textBuffer = makeBuffer((void*)(((uint32_t)b.mData) + header.mTextOffset), header.mTextLength);

	MugenDefScript script = loadMugenDefScriptFromBuffer(textBuffer);

	MugenFont* e = allocMemory(sizeof(MugenFont));
	e->mSize = getMugenDefVectorIOrDefault(&script, "Def", "size", makeVector3DI(1, 1, 0));
	e->mSpacing = getMugenDefVectorIOrDefault(&script, "Def", "spacing", makeVector3DI(0, 0, 0));
	e->mOffset = getMugenDefVectorIOrDefault(&script, "Def", "offset", makeVector3DI(0, 0, 0));

	e->mType = MUGEN_FONT_TYPE_ELECBYTE;
	loadMugenElecbyteFont(&script, textureBuffer, e);

	unloadMugenDefScript(script);

	freeBuffer(b);
	resetMugenFontDirectory();

	int_map_push_owned(&gData.mFonts, tKey, e);
}

void addMugenFont2(int tKey, char* tPath) {
	MugenDefScript script = loadMugenDefScript(tPath);

	setMugenFontDirectory2(tPath);

	MugenFont* e = allocMemory(sizeof(MugenFont));
	e->mType = getMugenFontTypeFromScript(&script);

	e->mSize = getMugenDefVectorIOrDefault(&script, "Def", "size", makeVector3DI(1, 1, 0));
	e->mSpacing = getMugenDefVectorIOrDefault(&script, "Def", "spacing", makeVector3DI(0, 0, 0));
	e->mOffset = getMugenDefVectorIOrDefault(&script, "Def", "offset", makeVector3DI(0, 0, 0));

	if (e->mType == MUGEN_FONT_TYPE_BITMAP) {
		loadBitmapFont(&script, e);
	}
	else if (e->mType == MUGEN_FONT_TYPE_TRUETYPE) {
		loadMugenTruetypeFont(&script, e);
	}
	else {
		logError("Unimplemented font type.");
		logErrorInteger(e->mType);
		abortSystem();
	}

	unloadMugenDefScript(script);

	resetMugenFontDirectory();

	int_map_push_owned(&gData.mFonts, tKey, e);
}

static void addMugenSystemFont(int tKey, char* tPath) {
	char path[1024];

	sprintf(path, "assets/%s", tPath); // TODO: properly

	char* ending = getFileExtension(path);

	if (!strcmp("def", ending)) {
		addMugenFont2(tKey, path);
	}
	else if (!strcmp("fnt", ending)) {
		addMugenFont(tKey, path);
	}
	else {
		logError("Unrecognized font file type.");
		logErrorString(tPath);
		abortSystem();
	}

}

static void loadMugenFonts(MugenDefScript* tScript) {
	int i;
	for (i = 0; i < 100; i++) {
		char name[100];
		sprintf(name, "font%d", i);
		if (isMugenDefStringVariable(tScript, "Files", name)) {
			char* path = getAllocatedMugenDefStringVariable(tScript, "Files", name);
			addMugenSystemFont(i, path);
			freeMemory(path);
		}
		else if (i != 0) {
			break;
		}

	}
}

void loadMugenTextHandler()
{

	MugenDefScript script = loadMugenDefScript("assets/data/system.def");

	gData.mFonts = new_int_map();
	loadMugenFonts(&script);

	unloadMugenDefScript(script);
}


typedef struct {
	char mText[1024];
	Position mPosition;
	MugenFont* mFont;

	double mR;
	double mG;
	double mB;

	MugenTextAlignment mAlignment;
	GeoRectangle mRectangle;
} MugenText;

static struct {
	IntMap mHandledTexts;

} gHandler;

static void loadMugenTextHandlerActor(void* tData) {
	gHandler.mHandledTexts = new_int_map();
}

static void updateMugenTextHandler(void* tData) {
	// TODO: buildup text
}

typedef struct {
	MugenSpriteFileSprite* mSprite;
	Position mBasePosition;
} BitmapDrawCaller;

// TODO: rectangle + color
static void drawSingleBitmapSubSprite(void* tCaller, void* tData) {
	BitmapDrawCaller* caller = tCaller;
	MugenSpriteFileSubSprite* subSprite = tData;

	double factor = 0.5; // TODO: 640p
	Position p = vecAdd(caller->mBasePosition, vecScale(makePosition(subSprite->mOffset.x, subSprite->mOffset.y, subSprite->mOffset.z), factor));
	scaleDrawing(factor, p);
	drawSprite(subSprite->mTexture, p, makeRectangleFromTexture(subSprite->mTexture));
	setDrawingParametersToIdentity();
}

static void drawSingleBitmapText(MugenText* e) {
	MugenFont* font = e->mFont;
	MugenBitmapFont* bitmapFont = font->mData;
	int textLength = strlen(e->mText);
	double factor = 0.5; // TODO: 640p

	MugenSpriteFileGroup* spriteGroup = int_map_get(&bitmapFont->mSprites.mGroups, 0);

	int i;
	Position p = vecAdd(e->mPosition, vecScale(makePosition(font->mOffset.x, font->mOffset.y, 0), factor));
	for (i = 0; i < textLength; i++) {

		if (int_map_contains(&spriteGroup->mSprites, (int)e->mText[i])) {
			BitmapDrawCaller caller;
			caller.mSprite = int_map_get(&spriteGroup->mSprites, (int)e->mText[i]);
			caller.mBasePosition = p;
			list_map(&caller.mSprite->mTextures, drawSingleBitmapSubSprite, &caller);

			p = vecAdd(p, vecScale(makePosition(caller.mSprite->mOriginalTextureSize.x, 0, 0), factor));
			p = vecAdd(p, vecScale(makePosition(font->mSpacing.x, 0, 0), factor));
		}
		else {
			p = vecAdd(p, vecScale(makePosition(font->mSize.x, 0, 0), factor));
			p = vecAdd(p, vecScale(makePosition(font->mSpacing.x, 0, 0), factor));
		}
	}
}

// TODO: color + rectangle
static void drawSingleTruetypeText(MugenText* e) {
	MugenFont* font = e->mFont;
	MugenTruetypeFont* truetypeFont = font->mData;

	drawTruetypeText(e->mText, truetypeFont->mFont, e->mPosition, font->mSize, COLOR_WHITE);
}

typedef struct {
	MugenElecbyteFontMapEntry* mMapEntry;
	Position mBasePosition;
	MugenText* mText;
	MugenFont* mFont;
} ElecbyteDrawCaller;

static void drawSingleElecbyteSubSprite(void* tCaller, void* tData) {
	ElecbyteDrawCaller* caller = tCaller;
	MugenSpriteFileSubSprite* subSprite = tData;

	int minWidth = 0;
	int maxWidth = subSprite->mTexture.mTextureSize.x - 1;
	int leftX = max(minWidth, min(maxWidth, caller->mMapEntry->mStartX - subSprite->mOffset.x));
	int rightX = max(minWidth, min(maxWidth, (caller->mMapEntry->mStartX + caller->mMapEntry->mWidth - 1) - subSprite->mOffset.x));

	if (leftX == rightX) return;

	Position p = vecAdd(caller->mBasePosition, makePosition(0, 0, 0));

	int minHeight = 0;
	int maxHeight = subSprite->mTexture.mTextureSize.y - 1;
	int upY = max(minHeight, min(maxHeight, (int)(caller->mText->mRectangle.mTopLeft.y - p.y)));
	int downY = max(minHeight, min(maxHeight, upY + (int)(caller->mText->mRectangle.mBottomRight.y - (p.y + caller->mFont->mSize.y))));
	if (upY == downY) return;

	p.y = max(p.y, caller->mText->mRectangle.mTopLeft.y);

	setDrawingBaseColorAdvanced(caller->mText->mR, caller->mText->mG, caller->mText->mB);
	drawSprite(subSprite->mTexture, p, makeRectangle(leftX, upY, rightX - leftX, downY - upY));
}

static void drawSingleElecbyteText(MugenText* e) {
	MugenFont* font = e->mFont;
	MugenElecbyteFont* elecbyteFont = font->mData;
	int textLength = strlen(e->mText);

	int i;
	Position p = vecAdd(e->mPosition, makePosition(font->mOffset.x, font->mOffset.y, 0));
	for (i = 0; i < textLength; i++) {

		if (int_map_contains(&elecbyteFont->mMap, (int)e->mText[i])) {
			ElecbyteDrawCaller caller;
			caller.mMapEntry = int_map_get(&elecbyteFont->mMap, (int)e->mText[i]);
			caller.mBasePosition = p;
			caller.mText = e;
			caller.mFont = font;
			list_map(&elecbyteFont->mSprite->mTextures, drawSingleElecbyteSubSprite, &caller);

			p = vecAdd(p, makePosition(caller.mMapEntry->mWidth, 0, 0));
			p = vecAdd(p, makePosition(font->mSpacing.x, 0, 0));
		}
		else {
			p = vecAdd(p, makePosition(font->mSize.x, 0, 0));
			p = vecAdd(p, makePosition(font->mSpacing.x, 0, 0));
		}
	}
}

static void drawSingleText(void* tCaller, void* tData) {
	(void)tCaller;

	MugenText* e = tData;
	if (e->mFont->mType == MUGEN_FONT_TYPE_BITMAP) {
		drawSingleBitmapText(e);
	}
	else if (e->mFont->mType == MUGEN_FONT_TYPE_TRUETYPE) {
		drawSingleTruetypeText(e);
	}
	else if (e->mFont->mType == MUGEN_FONT_TYPE_ELECBYTE) {
		drawSingleElecbyteText(e);
	}
	else {
		logError("Unimplemented font type.");
		logErrorInteger(e->mFont->mType);
		abortSystem();
	}
}

static void drawMugenTextHandler(void* tData) {

	int_map_map(&gHandler.mHandledTexts, drawSingleText, NULL);
}

ActorBlueprint MugenTextHandler = {
	.mLoad = loadMugenTextHandlerActor,
	.mUpdate = updateMugenTextHandler,
	.mDraw = drawMugenTextHandler,
};


int addMugenText(char * tText, Position tPosition, int tFont)
{
	MugenText* e = allocMemory(sizeof(MugenText));
	strcpy(e->mText, tText);

	if (int_map_contains(&gData.mFonts, tFont)) {
		e->mFont = int_map_get(&gData.mFonts, tFont);
	}
	else {
		e->mFont = int_map_get(&gData.mFonts, 1);
	}
	e->mPosition = vecSub(tPosition, makePosition(0, e->mFont->mSize.y / 2, 0));
	e->mR = e->mG = e->mB = 1;
	e->mAlignment = MUGEN_TEXT_ALIGNMENT_LEFT;
	e->mRectangle = makeGeoRectangle(-INF / 2, -INF / 2, INF, INF);

	return int_map_push_back_owned(&gHandler.mHandledTexts, e);
}

void setMugenTextFont(int tID, int tFont)
{
	MugenText* e = int_map_get(&gHandler.mHandledTexts, tID);

	e->mPosition = vecAdd(e->mPosition, makePosition(0, e->mFont->mSize.y / 2, 0));
	if (int_map_contains(&gData.mFonts, tFont)) {
		e->mFont = int_map_get(&gData.mFonts, tFont);
	}
	else {
		e->mFont = int_map_get(&gData.mFonts, 1);
	}

	e->mPosition = vecSub(e->mPosition, makePosition(0, e->mFont->mSize.y / 2, 0));
}

static double getBitmapTextSize(MugenText* e) {
	MugenFont* font = e->mFont;
	MugenBitmapFont* bitmapFont = font->mData;
	int textLength = strlen(e->mText);
	double factor = 0.5; // TODO: 640p

	MugenSpriteFileGroup* spriteGroup = int_map_get(&bitmapFont->mSprites.mGroups, 0);

	double sizeX = 0;
	int i;
	for (i = 0; i < textLength; i++) {
		if (int_map_contains(&spriteGroup->mSprites, (int)e->mText[i])) {
			MugenSpriteFileSprite* sprite = int_map_get(&spriteGroup->mSprites, (int)e->mText[i]);
			sizeX += sprite->mOriginalTextureSize.x * factor;
			sizeX += font->mSpacing.x * factor;
		}
		else {
			sizeX += font->mSize.x * factor;
			sizeX += font->mSpacing.x * factor;
		}
	}

	return sizeX;
}

static double getTruetypeTextSize(MugenText* e) {
	int textLength = strlen(e->mText);
	return e->mFont->mSize.x*textLength;
}

static double getElecbyteTextSize(MugenText* e) {
	MugenFont* font = e->mFont;
	MugenElecbyteFont* elecbyteFont = font->mData;
	int textLength = strlen(e->mText);
	double factor = 1; // TODO: 640p

	int i;
	double sizeX = 0;
	for (i = 0; i < textLength; i++) {
		if (int_map_contains(&elecbyteFont->mMap, (int)e->mText[i])) {
			MugenElecbyteFontMapEntry* mapEntry = int_map_get(&elecbyteFont->mMap, (int)e->mText[i]);
			sizeX += mapEntry->mWidth*factor;
			sizeX += font->mSpacing.x*factor;
		}
		else {
			sizeX += font->mSize.x*factor;
			sizeX += font->mSpacing.x*factor;
		}
	}

	return sizeX;
}

static double getMugenTextSizeX(MugenText* e) {

	if (e->mFont->mType == MUGEN_FONT_TYPE_BITMAP) {
		return getBitmapTextSize(e);
	}
	else if (e->mFont->mType == MUGEN_FONT_TYPE_TRUETYPE) {
		return getTruetypeTextSize(e);
	}
	else if (e->mFont->mType == MUGEN_FONT_TYPE_ELECBYTE) {
		return getElecbyteTextSize(e);
	}
	else {
		logError("Unimplemented font type.");
		logErrorInteger(e->mFont->mType);
		abortSystem();
	}
}

void setMugenTextAlignment(int tID, MugenTextAlignment tAlignment)
{
	MugenText* e = int_map_get(&gHandler.mHandledTexts, tID);

	double sizeX = getMugenTextSizeX(e);

	if (e->mAlignment == MUGEN_TEXT_ALIGNMENT_CENTER) {
		e->mPosition.x += sizeX / 2;
	} else if (e->mAlignment == MUGEN_TEXT_ALIGNMENT_RIGHT) {
		e->mPosition.x += sizeX;
	}

	if (tAlignment == MUGEN_TEXT_ALIGNMENT_CENTER) {
		e->mPosition.x -= sizeX / 2;
	}
	else if (tAlignment == MUGEN_TEXT_ALIGNMENT_RIGHT) {
		e->mPosition.x -= sizeX;
	}
	e->mAlignment = tAlignment;
}

void setMugenTextColor(int tID, Color tColor)
{
	MugenText* e = int_map_get(&gHandler.mHandledTexts, tID);
	getRGBFromColor(tColor, &e->mR,&e->mG, &e->mB);
}

void setMugenTextRectangle(int tID, GeoRectangle tRectangle)
{
	MugenText* e = int_map_get(&gHandler.mHandledTexts, tID);
	e->mRectangle = tRectangle;
		
}

void setMugenTextPosition(int tID, Position tPosition)
{
	MugenText* e = int_map_get(&gHandler.mHandledTexts, tID);
	e->mPosition = tPosition;
	MugenTextAlignment alignment = e->mAlignment;
	e->mAlignment = MUGEN_TEXT_ALIGNMENT_LEFT;
	setMugenTextAlignment(tID, alignment);
}

MugenTextAlignment getMugenTextAlignmentFromMugenAlignmentIndex(int tIndex)
{
	if (tIndex > 0) return MUGEN_TEXT_ALIGNMENT_LEFT;
	else if (tIndex < 0) return MUGEN_TEXT_ALIGNMENT_RIGHT;
	else return MUGEN_TEXT_ALIGNMENT_CENTER;
}

Color getMugenTextColorFromMugenTextColorIndex(int tIndex)
{
	if (tIndex == 0) {
		return COLOR_WHITE;
	}
	else if (tIndex == 5) {
		return COLOR_YELLOW;
	}
	else {
		logError("Unrecognized Mugen text color.");
		logErrorInteger(tIndex);
		abortSystem();
		return COLOR_WHITE;
	}
}
