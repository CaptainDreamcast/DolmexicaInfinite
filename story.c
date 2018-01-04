#include "story.h"

#include <assert.h>

static struct {
	int mPosition;

} gData;

static char gStoryDefPaths[][1024] = {
"assets/main/fight/story/1.def",
"assets/main/fight/story/2.def",
"assets/main/fight/story/3.def",
"assets/main/fight/story/4.def",
};

void resetDreamStory()
{
	gData.mPosition = -1;
}

void startDreamNextStoryPart()
{
	gData.mPosition++;
}

char * getCurrentDreamStoryDefinitionFile()
{

	assert(gData.mPosition >= 0);
	assert(gData.mPosition < 4);
	return gStoryDefPaths[gData.mPosition];
}
