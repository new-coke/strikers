static uint64_t profileClockCalls = 0;
static uint64_t profile_clock() { return ++profileClockCalls * 1000; }
static void present_profile() {
    CHECK(!profileClockCalls); // All old lifetime scenarios left profiling OFF.
    reset();
    vglSetPresentProfile(nullptr, nullptr);
    vglSwapBuffers(false);
    CHECK(!profileClockCalls && producerCalls == 2);
    PortVglPresentProfile p{};
    vglSetPresentProfile(&p, profile_clock);
    const auto oldCalls = producerCalls;
    vglSwapBuffers(false);
    vglSetPresentProfile(nullptr, nullptr);
    CHECK(p.addEntryCalls == 1 && p.addEntryNs == 1000 && !p.addEntryErrors && !p.lastAddEntryResult);
    CHECK(p.gcCalls == 1 && p.gcNs == 1000 && !p.gcErrors && !p.lastGcResult);
    CHECK(producerCalls == oldCalls + 2 && profileClockCalls == 4);
    const auto clocks = profileClockCalls;
    vglSwapBuffers(false);
    CHECK(profileClockCalls == clocks && p.gcCalls == 1 && p.addEntryCalls == 1);
    for (unsigned failure = 0; failure < 3; ++failure) {
        reset();p={};
        addEntryResult = failure == 0 ? -9 : 0;
        signalResult = failure == 1 ? -7 : 0;
        waitResult = failure == 2 ? -11 : 0;
        vglSetPresentProfile(&p, profile_clock);
        vglSwapBuffers(false);
        vglSetPresentProfile(nullptr, nullptr);
        CHECK(p.addEntryCalls == 1 && p.gcCalls == 1 && p.addEntryNs == 1000 && p.gcNs == 1000);
        CHECK(p.addEntryErrors == unsigned(failure == 0) && p.lastAddEntryResult == addEntryResult);
        CHECK(p.gcErrors == unsigned(failure != 0));
        CHECK(p.lastGcResult == (failure == 1 ? -7 : failure == 2 ? -11 : 0));
        CHECK(producerCalls == (failure == 1 ? 1u : 2u)); // Original short-circuit is unchanged.
        CHECK(vgl_error == (failure == 0 ? GL_NO_ERROR : GL_INVALID_OPERATION));
        signalResult = waitResult = addEntryResult = 0;
        if (failure == 2) run_gc(); // Complete the intentionally failed producer wait.
    }
    // The real system-app branch does not call DisplayQueueAddEntry.
    reset();p={};system_app_mode=true;
    vglSetPresentProfile(&p, profile_clock);vglSwapBuffers(false);vglSetPresentProfile(nullptr,nullptr);
    CHECK(!p.addEntryCalls && !p.addEntryNs && p.gcCalls==1 && p.gcNs==1000);
    system_app_mode=false;
    const auto end=profileClockCalls;
    reset();vglSwapBuffers(false);CHECK(profileClockCalls==end);
    std::puts("Present profile: actual swap/GC bodies, common clock, raw failures, preserved short circuit and quiet null hook passed");
}
