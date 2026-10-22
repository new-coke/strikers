#include "port/benchmark.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);exit(1);}}while(0)
static unsigned long long ns=1000000000ull;
static unsigned calls;
int PortDiagnosticsEnabled(void){return 0;}
int PortFpsOverlayEnabled(void){return 0;}
unsigned long long port_monotonic_ns(void){++calls;return ns;}
static void check_ready(const char* path, const char* expected){
    char ready[160],line[256];snprintf(ready,sizeof ready,"%s.ready",path);
    FILE* f=fopen(ready,"r");CHECK(f);CHECK(fgets(line,sizeof line,f));
    CHECK(strcmp(line,"# strikers-frame-capture-ready-v1\n")==0);
    CHECK(fgets(line,sizeof line,f));CHECK(strstr(line,"requested,warmup_remaining,"));
    CHECK(fgets(line,sizeof line,f));CHECK(strcmp(line,expected)==0);
    CHECK(!fgets(line,sizeof line,f));CHECK(fclose(f)==0);CHECK(unlink(ready)==0);
}
static void check_play_only(void){
    char path[128];snprintf(path,sizeof path,"/tmp/strikers-play-capture-%ld.csv",(long)getpid());
    setenv("STRIKERS_FRAME_CAPTURE",path,1);setenv("STRIKERS_FRAME_CAPTURE_FRAMES","4",1);
    setenv("STRIKERS_FRAME_CAPTURE_MATCH_ONLY","1",1);setenv("STRIKERS_FRAME_CAPTURE_PLAY_ONLY","1",1);
    setenv("STRIKERS_FRAME_CAPTURE_SKIP","2",1);PortBenchInit();PortBenchRendererGameplayStart();
    CHECK(calls==0);check_ready(path,"4,2,1,1,0,0\n");CHECK(access(path,F_OK)!=0);
    for(unsigned i=0;i<12;++i){
        // Intro, kickoff/replay and a renderer epoch reset must not consume
        // live-play samples or reset their simulation-frame identity.
        if(i==6)PortBenchRendererGameplayStart();
        PortBenchSetPlayActive(i==1||i==2||i==4||i==5||i==8||i==9);
        PortBenchFrameBegin();ns+=10000000ull;PortBenchAfterTasks();ns+=2000000ull;PortBenchFrameEnd();
    }
    CHECK(calls==30);PortBenchLive live;PortBenchGetLive(&live);CHECK(live.playFrames==6);
    PortBenchCaptureStatus status;PortBenchGetCaptureStatus(&status);
    CHECK(status.state==PORT_CAPTURE_COMPLETE&&status.samples==4&&status.lastFrame==5);
    FILE* f=fopen(path,"r");CHECK(f);char line[256];CHECK(fgets(line,sizeof line,f));CHECK(fgets(line,sizeof line,f));
    CHECK(strstr(line,"play_only=1 counter=live_play"));CHECK(fgets(line,sizeof line,f));
    for(unsigned i=0;i<4;++i){unsigned sample,match,frame,tasks,present,sleep;unsigned long mf;
        CHECK(fgets(line,sizeof line,f));CHECK(sscanf(line,"%u,%u,%lu,%u,%u,%u,%u",&sample,&match,&mf,&frame,&tasks,&present,&sleep)==7);
        CHECK(sample==i&&match==1&&mf==i+2);CHECK(frame==12000&&tasks==10000&&present==2000&&sleep==0);
    }
    CHECK(!fgets(line,sizeof line,f));fclose(f);unlink(path);
}
int main(void){
    pid_t child=fork();CHECK(child>=0);if(child==0){check_play_only();_exit(0);}
    int status;CHECK(waitpid(child,&status,0)==child);CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==0);
    char path[128];snprintf(path,sizeof path,"/tmp/strikers-capture-test-%ld.csv",(long)getpid());
    setenv("STRIKERS_FRAME_CAPTURE",path,1);setenv("STRIKERS_FRAME_CAPTURE_FRAMES","4",1);
    PortBenchInit();
    CHECK(calls==0);check_ready(path,"4,0,0,0,0,0\n");CHECK(access(path,F_OK)!=0);
    for(unsigned i=0;i<6;++i){
        if(i==1)PortBenchRendererGameplayStart();if(i==3)PortBenchRendererGameplayEnd();
        PortBenchAddPreFrameSleep(2000000ull);PortBenchFrameBegin();PortBenchAddSleep(1000000ull);
        ns+=9000000ull+i*1000000ull;PortBenchAfterTasks();ns+=3000000ull;PortBenchFrameEnd();
    }
    CHECK(calls==12); // Collection stops completely when both regular switches are OFF.
    FILE* f=fopen(path,"r");CHECK(f);char line[256];CHECK(fgets(line,sizeof line,f));CHECK(fgets(line,sizeof line,f));CHECK(fgets(line,sizeof line,f));
    for(unsigned i=0;i<4;++i){unsigned sample,match,frame,tasks,present,sleep;unsigned long mf;
        CHECK(fgets(line,sizeof line,f));CHECK(sscanf(line,"%u,%u,%lu,%u,%u,%u,%u",&sample,&match,&mf,&frame,&tasks,&present,&sleep)==7);
        CHECK(sample==i);CHECK(match==(i==1||i==2));CHECK(mf==(i==2?1ul:0ul));
        CHECK(frame==14000+i*1000);CHECK(tasks==8000+i*1000);CHECK(present==3000);CHECK(sleep==3000);
    }
    CHECK(!fgets(line,sizeof line,f));fclose(f);unlink(path);
    puts("finite frame capture: pacing, phase accounting, gameplay identity, completion, diagnostics OFF");return 0;
}
