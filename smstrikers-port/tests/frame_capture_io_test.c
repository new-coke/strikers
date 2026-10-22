/* Exercise real capture code with deterministic stdio faults. No Vita needed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>
#include "port/benchmark.h"
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); exit(1); } } while(0)
static int fault, opens;
static unsigned long long tick;
int PortDiagnosticsEnabled(void) { return 0; }
int PortFpsOverlayEnabled(void) { return 0; }
unsigned long long port_monotonic_ns(void) { return tick += 1000000; }
static FILE* capture_open(const char* path, const char* mode)
{
    ++opens;
    if (fault == 1) { errno=EACCES; return NULL; }
    return fopen(path, mode);
}
static int capture_print(FILE* stream, const char* format, ...)
{
    if (fault == 2) { errno=ENOSPC; return -1; }
    va_list args; va_start(args,format);
    int result=vfprintf(stream,format,args); va_end(args); return result;
}
static int capture_close(FILE* stream)
{
    int result=fclose(stream);
    if (fault == 3) { errno=EIO; return EOF; }
    return result;
}
#define fopen capture_open
#define fprintf capture_print
#define fclose capture_close
#include "../src/platform/benchmark.c"
#undef fopen
#undef fprintf
#undef fclose

static void frame(void)
{
    PortBenchFrameBegin(); PortBenchAfterTasks(); PortBenchFrameEnd();
}
static void check_fault(int failure)
{
    char path[128]; snprintf(path,sizeof path,"/tmp/strikers-capture-io-%ld.csv",(long)getpid());
    setenv("STRIKERS_FRAME_CAPTURE",path,1);
    setenv("STRIKERS_FRAME_CAPTURE_FRAMES","4",1);
    PortBenchCaptureStatus status;
    PortBenchGetCaptureStatus(&status); CHECK(status.state==PORT_CAPTURE_IDLE);
    PortBenchInit();
    PortBenchGetCaptureStatus(&status); CHECK(status.state==PORT_CAPTURE_COLLECTING);
    CHECK(opens==1); // The startup-ready marker is written before measurement.
    CHECK(!PortBenchRetryCapture());
    fault=failure;
    for (int i=0;i<3;++i) frame();
    CHECK(opens==1); // No output while collecting incomplete samples.
    frame();
    PortBenchGetCaptureStatus(&status);
    CHECK(status.state==PORT_CAPTURE_ERROR && status.samples==4 && status.requested==4);
    CHECK(status.errorCode==(failure==1?EACCES:failure==2?ENOSPC:EIO));
    for (int i=0;i<20;++i) frame();
    CHECK(opens==2); // No automatic retry and no out-of-bounds extra sample.
    fault=0;
    CHECK(PortBenchRetryCapture());
    PortBenchGetCaptureStatus(&status);
    CHECK(status.state==PORT_CAPTURE_COMPLETE && status.errorCode==0 && status.samples==4);
    CHECK(!PortBenchRetryCapture() && opens==3);
    FILE* file=fopen(path,"rb"); CHECK(file);
    char line[256]; int rows=0;
    while (fgets(line,sizeof line,file)) {
        unsigned sample,match,frameUs,tasks,present,sleep; unsigned long mf;
        if (sscanf(line,"%u,%u,%lu,%u,%u,%u,%u",&sample,&match,&mf,&frameUs,&tasks,&present,&sleep)==7) {
            CHECK(sample==(unsigned)rows++ && frameUs==2000 && tasks==1000 && present==1000);
        }
    }
    CHECK(rows==4); fclose(file); unlink(path);
    char ready[sizeof path + sizeof ".ready"];
    snprintf(ready,sizeof ready,"%s.ready",path); unlink(ready);
}
int main(void)
{
    for (int failure=1;failure<=3;++failure) {
        pid_t child=fork(); CHECK(child>=0);
        if (!child) { check_fault(failure); _exit(0); }
        int status; CHECK(waitpid(child,&status,0)==child);
        CHECK(WIFEXITED(status) && WEXITSTATUS(status)==0);
    }
    puts("capture open/write/close failures retain exact samples; explicit retry only");
}
