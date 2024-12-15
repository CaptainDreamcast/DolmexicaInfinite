#include "netplaylogic.h"

#include <prism/netplay.h>

using namespace prism;

void initDolmexicaNetplay()
{
	initNetplay();
}

void startDolmexicaNetplayHost(void(tJoinedCB)(void*))
{
	setNetplayConnectCB(tJoinedCB, NULL);
	startNetplayHosting();
}

void stopDolmexicaNetplayHost()
{
	stopNetplayHosting();
}

bool tryDolmexicaNetplayJoin(const std::string& tHostIP, void(tJoinedCB)(void*))
{
	setNetplayConnectCB(tJoinedCB, NULL);
	return joinNetplayHost(tHostIP, 1234);
}

void updateDolmexicaNetplay()
{
}

bool isDolmexicaNetplayHost() {
	return isNetplayHost();
}