#include "storyhelper.h"

#include "mugenstatehandler.h"
#include "playerdefinition.h"
#include "dolmexicastoryscreen.h"
#include "config.h"

static struct {
	std::string mPath;
} gStoryHelperData;

void resetStoryHelper()
{
	gStoryHelperData.mPath = "";
}

const std::string& getStoryHelperPath()
{
	return gStoryHelperData.mPath;
}

void setStoryHelperPath(const std::string& tPath)
{
	gStoryHelperData.mPath = tPath;
}

int hasStoryHelper()
{
	return !gStoryHelperData.mPath.empty();
}
