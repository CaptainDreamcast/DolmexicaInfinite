#pragma once

#include <stdint.h>

#if defined(DREAMCAST) || defined(VITA) || defined(__EMSCRIPTEN__)
#define FIGHT_DETERMINISM_DISABLED 1
#endif

#ifdef FIGHT_DETERMINISM_DISABLED

inline uint32_t getFightStateChecksum() { return 0; }

inline int startFightInputRecording(const char*) { return 0; }
inline void stopFightInputRecording() {}
inline int isFightInputRecordingActive() { return 0; }

inline int startFightInputPlayback(const char*) { return 0; }
inline void stopFightInputPlayback() {}
inline int isFightInputPlaybackActive() { return 0; }
inline int isFightInputPlaybackOver() { return 0; }
inline int getFightInputPlaybackFrame() { return 0; }
inline int getFightInputPlaybackFrameAmount() { return 0; }
inline int getFightInputPlaybackChecksumMismatchAmount() { return 0; }
inline int getFightInputPlaybackFirstChecksumMismatchFrame() { return -1; }
inline void getFightInputPlaybackPlayerDefinitionPath(char* tDst, int) { if (tDst) *tDst = '\0'; }

inline void updateFightDeterminismInputHook(uint32_t*) {}

#else

// excludes gSTLCounter IDs so it is comparable across process restarts and across machines
uint32_t getFightStateChecksum();

int startFightInputRecording(const char* tPath);
void stopFightInputRecording();
int isFightInputRecordingActive();

int startFightInputPlayback(const char* tPath);
void stopFightInputPlayback();
int isFightInputPlaybackActive();
int isFightInputPlaybackOver();
int getFightInputPlaybackFrame();
int getFightInputPlaybackFrameAmount();
int getFightInputPlaybackChecksumMismatchAmount();
int getFightInputPlaybackFirstChecksumMismatchFrame(); // -1 if none so far

void getFightInputPlaybackPlayerDefinitionPath(char* tDst, int i);

void updateFightDeterminismInputHook(uint32_t* tHeldMasks);

#endif
