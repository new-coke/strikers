#ifndef _MOVIE_H_
#define _MOVIE_H_

#include "NL/nlTask.h"
#include "Game/GameInfo.h"

bool IsMoviePlayingInStrikers101();
bool MoviePlay();
bool MovieStop();
bool MovieStart(const char* szFilename, bool bSound, bool bLoopMovie);

#endif // _MOVIE_H_
