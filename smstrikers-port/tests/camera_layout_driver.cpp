#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "Game/Camera/TopDownCamera.h"
#include "Game/Field.h"
#include "NL/gl/glMatrix.h"
extern "C" void* fixture_allocate_top_down();
class cBall;
cBall* g_pBall = nullptr;
static size_t requested = 0;
static void* allocated = nullptr;
// These satisfy the real class's vtable; this constructor test never invokes them.
float cField::GetGoalLineX(unsigned int) { std::abort(); }
void glMatrixLookAt(nlMatrix4&,const nlVector3&,const nlVector3&,const nlVector3&) { std::abort(); }
void* nlMalloc(size_t size,unsigned int alignment,bool atEnd) {
 assert(alignment==8 && !atEnd && !allocated);
 requested=size; allocated=std::malloc(size); assert(allocated);
 std::printf("requested=%zu canonical=%zu\n",size,sizeof(TopDownCamera)); std::fflush(stdout);
 return allocated;
}
int main() {
 auto* camera=static_cast<TopDownCamera*>(fixture_allocate_top_down());
 assert(requested==sizeof(TopDownCamera));
 assert(camera==allocated && camera->GetType()==eCameraType_TopDown);
 assert(camera->m_pFilter==nullptr);
 assert(camera->mUpVector.x==0 && camera->mUpVector.y==0 && camera->mUpVector.z==1);
 assert(camera->m_Fov==90 && camera->GetFOV()==90);
 assert(camera->m_Position.x==0 && camera->m_Position.y==-1 && camera->m_Position.z==21);
 assert(camera->m_Target.x==0 && camera->m_Target.y==0 && camera->m_Target.z==0);
 camera->~TopDownCamera(); std::free(allocated); allocated=nullptr;
 std::puts("PASS: exact factory expression, real TopDownCamera constructor/base initializer/destructor, initialized fields and allocator contract");
}
