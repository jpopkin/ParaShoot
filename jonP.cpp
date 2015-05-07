#include "jonP.h"
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <stdio.h>
/*
#define USE_SOUND

#ifdef USE_SOUND
#include <FMOD/fmod_errors.h>
#include <FMOD/fmod.h>
#include <FMOD/wincompat.h>
#include "fmod.h"
#endif
*/

void create_sounds() {
#ifdef USE_SOUND
    if(fmod_init()) {
	printf("ERROR");
	return;
    }
    if(fmod_createsound((char *)"./sounds/Highly_Suspicious.mp3", 0)) {
	printf("ERROR");
	return;
    }
    fmod_setmode(0, FMOD_LOOP_NORMAL);
#endif
}
void play() {
    fmod_playsound(0);
}
