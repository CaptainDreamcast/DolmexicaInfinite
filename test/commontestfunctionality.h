#pragma once

#include <string>

void setupTestForScreenTestInAssetsFolder();
void tearDownTestForScreenTestInAssetsFolder();

void initForAutomatedFightScreenTest();

// DOLMEXICA_ASSET_FILTER=<substring> narrows a whole-folder test run down to the assets whose name contains it, so a single failing character or stage can be reproduced, empty means everything
int isAssetIncludedInTestRun(const std::string& tAssetName);