#ifndef _GLXSWAP_H_
#define _GLXSWAP_H_

void glxSwapPost(bool bSend);
void glxSwapPre(bool bSend);
void glxInitSwap(void* fb0, void* fb1);
void glxSwapWaitDrawDone();
void glxLoadRestoreState();
void glxLoadSaveState();
void glxSwapLoading(bool bBegin, bool bOtherPosition);
void* glxGetBackBuffer();
void* glxGetDisplayedBuffer();
void glxSwapSetBlack(bool black);

#endif // _GLXSWAP_H_
