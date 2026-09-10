// Duplicating front-end scene content: every options screen is authored to exactly the rows it
// uses, so a new setting means copying an instance that works.

// The copies are not inside the package's single allocation, so PortFEReleaseClones must run before
// the package is freed.

#ifndef _PORT_FE_CLONE_H_
#define _PORT_FE_CLONE_H_

class TLInstance;

#ifdef __cplusplus
extern "C" {
#endif

// Deep-copy `src` and its subtree, unlinked. `newName` rehashes it for FEFinder; NULL leaves a
// duplicate under the source's name.
TLInstance* PortFEClone(const TLInstance* src, const char* newName);

// Non-zero if the last clone overran the instance or slide tables, leaving animation targets partly
// remapped.
int PortFECloneOverflowed(void);

void PortFEInsertAfter(TLInstance* after, TLInstance* node);

void PortFESetPosition(TLInstance* node, float x, float y, float z);

// Depth-first by name hash. A menu row's label is an 'R JUST' text instance.
TLInstance* PortFEFindChild(TLInstance* parent, const char* name);

unsigned long PortFECloneCount(void);
void PortFEReleaseClones(void);

#ifdef __cplusplus
}
#endif

#endif // _PORT_FE_CLONE_H_
