// Which disc is loaded, decided at runtime, once, from the data itself.

#ifndef PORT_REGION_H
#define PORT_REGION_H

#ifdef __cplusplus
extern "C" {
#endif

enum PortRegion
{
    PORT_REGION_USA = 0,     // G4QE01
    PORT_REGION_EUROPE = 1,  // G4QP01
    PORT_REGION_JAPAN = 2,   // G4QJ01
};

// The loaded disc's region. Valid from DVDInit onwards, which is before main(); an operator new
// from any static initialiser reaches nlMalloc, which calls nlInitMemory, which calls DVDInit.
// With no sys/boot.bin to read, `region` in strikers.ini names the disc, and failing that USA.
int port_region(void);

// The four-character game code and two-character maker code as they appear in the disc header
// ("G4QP", "01").
const char* port_disc_game_code(void);
const char* port_disc_maker_code(void);

// Tell Aurora which disc is loaded, so the memory card lands in the right region directory.
void PortSetDiscGameName(const char* code4);

#ifdef __cplusplus
}
#endif

#endif // PORT_REGION_H
