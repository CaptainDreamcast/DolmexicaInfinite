#pragma once

#include <string>

void initDolmexicaNetplay();

void startDolmexicaNetplayHost(void(tJoinedCB)(void*));
void stopDolmexicaNetplayHost();
bool tryDolmexicaNetplayJoin(const std::string& tHostIP, void(tJoinedCB)(void*));

void updateDolmexicaNetplay();
bool isDolmexicaNetplayHost();