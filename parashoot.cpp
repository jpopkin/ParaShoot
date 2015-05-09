#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cmath>
#include <string>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <X11/Xutil.h>
#include "log.h"
#include "ppm.h"
extern "C" {
#include "fonts.h"
}
#include "jonP.h"
#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 800
#define STARTING_ALTITUDE 12000
#define MAX_PARTICLES 1
#define GRAVITY 3.0
//#define USE_SOUND

//#ifdef USE_SOUND
//#include <FMOD/fmod.h>
//#include <FMOD/wincompat.h>
//#include "fmod.h"
//#endif

//X Windows variables
Display *dpy;
Window win;
GLXContext glc;

int xres = WINDOW_WIDTH , yres = WINDOW_HEIGHT;
bool size_flag = false;
bool start_flag = true;

//Camera position
GLfloat gCameraY = 0.0f;

Ppmimage *skyImage = NULL;
Ppmimage *characterImage = NULL;
Ppmimage *BlueBirdImage = NULL;

GLuint skyTexture;
GLuint characterTexture;
GLuint silhouetteTexture;
GLuint BlueBirdTexture;
GLuint BsilhouetteTexture;

int sky = 1;
int character = 1;
int keys[65536];

struct Vec {
    float x, y, z;
};

struct Shape {
    float width, height;
    float radius;
    Vec center;
};

struct Character {
    Shape s;
    Vec velocity;
};

struct Game {
    Shape box;
    Character character;
    Character BlueBird;
    int n;
    float altitude;
    Game() {
	altitude = (float)STARTING_ALTITUDE;
	n = 0;
    }
};


//Function prototypes
void initXWindows(void);
void init_opengl(Game *game);
void cleanupXWindows(void);
void check_mouse(XEvent *e, Game *game);
int check_keys(XEvent *e);
void movement(Game *game);
void render(Game *game);
void check_resize(Game *game, XEvent *e);
//void create_sounds();
//void play();
void init_keys();
//-----------------------------------------------------------------------------
//Setup timers
const double physicsRate = 1.0 / 30.0;
const double oobillion = 1.0 / 1e9;
struct timespec timeStart, timeCurrent;
struct timespec timePause;
double physicsCountdown=0.0;
double timeSpan=0.0;
double timeDiff(struct timespec *start, struct timespec *end) {
    return (double)(end->tv_sec - start->tv_sec ) +
	(double)(end->tv_nsec - start->tv_nsec) * oobillion;
}
void timeCopy(struct timespec *dest, struct timespec *source) {
    memcpy(dest, source, sizeof(struct timespec));
}
//-----------------------------------------------------------------------------

int main(void)
{
    int done=0;
    srand(time(NULL));
    initXWindows();
    Game game;
    init_opengl(&game);
    create_sounds();
    play();
    //declare game object
    init_keys();
    clock_gettime(CLOCK_REALTIME, &timePause);
    clock_gettime(CLOCK_REALTIME, &timeStart);

    //start animation
    while(!done) {
	while(XPending(dpy)) {
	    XEvent e;
	    XNextEvent(dpy, &e);
	    check_mouse(&e, &game);
	    check_resize(&game, &e);
	    done = check_keys(&e);
	}
	clock_gettime(CLOCK_REALTIME, &timeCurrent);
	timeSpan = timeDiff(&timeStart, &timeCurrent);
	timeCopy(&timeStart, &timeCurrent);
	physicsCountdown += timeSpan;
	while(physicsCountdown >= physicsRate) {
	    movement(&game);
	    physicsCountdown -= physicsRate;
	}
	render(&game);
	glXSwapBuffers(dpy, win);
    }
    cleanupXWindows();
    cleanup_fonts();
    return 0;
}

void set_title(void)
{
    //Set the window title bar.
    XMapWindow(dpy, win);
    XStoreName(dpy, win, "ParaShoot!");
}

void cleanupXWindows(void) {
    //do not change
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
}

void setup_screen_res(const int w, const int h)
{
    xres=w;
    yres=h;
}

void initXWindows(void) {
    //do not change
    GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
    int w=WINDOW_WIDTH, h=WINDOW_HEIGHT;
    dpy = XOpenDisplay(NULL);
    if (dpy == NULL) {  std::cout << "\n\tcannot connect to X server\n" << std::endl;
	exit(EXIT_FAILURE);
    }
    Window root = DefaultRootWindow(dpy);
    XVisualInfo *vi = glXChooseVisual(dpy, 0, att);
    if(vi == NULL) {
	std::cout << "\n\tno appropriate visual found\n" << std::endl;
	exit(EXIT_FAILURE);
    }
    Colormap cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
	ButtonPress | ButtonReleaseMask |
	PointerMotionMask |
	StructureNotifyMask | SubstructureNotifyMask;
    win = XCreateWindow(dpy, root, 0, 0, w, h, 0, vi->depth,
	    InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
    set_title();
    glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, glc);
}



void reshape_window(Game *game, int width, int height)
{
    Character *p = &game->character;
    size_flag = true;
    setup_screen_res(width, height);
    p->s.center.x = width/2;
    p->s.center.y = (game->altitude - height) / 2;

    glViewport(0,0, (GLint)width, (GLint)height);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    //glOrtho(0, width, 0, height, -1, 1);
    glOrtho(0, width, (game->altitude - height), game->altitude, -1, 1);
    set_title();
}


unsigned char *buildAlphaData(Ppmimage *img)
{
    //add 4th component to RGB stream...
    int a,b,c;
    unsigned char *newdata, *ptr;
    unsigned char *data = (unsigned char *)img->data;
    //newdata = (unsigned char *)malloc(img->width * img->height * 4);
    newdata = new unsigned char[img->width * img->height * 4];
    ptr = newdata;
    for (int i=0; i<img->width * img->height * 3; i+=3) {
	a = *(data+0);
	b = *(data+1);
	c = *(data+2);
	*(ptr+0) = a;
	*(ptr+1) = b;
	*(ptr+2) = c;
	//
	//new code, suggested by Chris Smith, Fall 2013
	*(ptr+3) = (a|b|c);
	//
	ptr += 4;
	data += 3;
    }
    return newdata;
}

void init_opengl(Game *game) 
{
    //OpenGL initialization
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    //Initialize matrices
    glMatrixMode(GL_PROJECTION); 
    glLoadIdentity();
    //Set 2D mode (no perspective)
    glOrtho(0, WINDOW_WIDTH, (game->altitude - WINDOW_HEIGHT), game->altitude, -1, 1);
    //glOrtho(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT, -1, 1);
    glMatrixMode(GL_MODELVIEW); 
    glLoadIdentity();
    //glDisable(GL_LIGHTING);
    //glDisable(GL_DEPTH_TEST);
    //glDisable(GL_FOG);
    //glDisable(GL_CULL_FACE);

    //Save the default modelview matrix
    glPushMatrix();
    //Set the screen background color
    //glClearColor(0.1, 0.5, 1.0, 1.0);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glEnable(GL_TEXTURE_2D);
    initialize_fonts();
    //
    //load the images file into a ppm structure
    //
    skyImage = ppm6GetImage("./images/Sunset.ppm");
    characterImage = ppm6GetImage("./images/character2.ppm");
    BlueBirdImage = ppm6GetImage("./images/BlueBird.ppm");

    //create opengl texture elements
    glGenTextures(1, &skyTexture);
    glGenTextures(1, &BlueBirdTexture);
    glGenTextures(1, &characterTexture);
    glGenTextures(1, &silhouetteTexture);
    glGenTextures(1, &BsilhouetteTexture);
    //
    //sky
    glBindTexture(GL_TEXTURE_2D, skyTexture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, skyImage->width, skyImage->height,
	    0, GL_RGB, GL_UNSIGNED_BYTE, skyImage->data);

    //
    //character
    glBindTexture(GL_TEXTURE_2D, characterTexture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, characterImage->width,
	    characterImage->height, 0, GL_RGB, GL_UNSIGNED_BYTE,
	    characterImage->data);

    //
    //bird
    glBindTexture(GL_TEXTURE_2D, BsilhouetteTexture);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    unsigned char *BsilhouetteData = buildAlphaData(BlueBirdImage);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, BlueBirdImage->width, 
	    BlueBirdImage->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 
	    BsilhouetteData);
    delete [] BsilhouetteData;

    //
    //character silhouette
    glBindTexture(GL_TEXTURE_2D, silhouetteTexture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    unsigned char *silhouetteData = buildAlphaData(characterImage);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, characterImage->width, 
	    characterImage->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, silhouetteData);
    delete [] silhouetteData;
}

void check_resize(Game *game, XEvent *e)
{
    if(e->type != ConfigureNotify)
	return;

    XConfigureEvent xce = e->xconfigure;
    if(xce.width != xres || xce.height != yres) {
	reshape_window(game, xce.width, xce.height);
    }
}

void makeCharacter(Game *game)
{
    Character *p = &game->character;
    Character *b = &game->BlueBird;
    p->s.center.x = xres/2;
    p->s.center.y = (game->altitude- (yres/2));
    p->velocity.y = 0;
    p->velocity.x = 0;

    //	start.centerx = xres/2;
    //	start.centery = game->altitude - 200;
    //	start.bot = game->altitude - 200;
    //	start.width = 500;
    //	start.height = 100;
    //	start.center = xres/2 + 200;
    //	start.left = start.centerx;
    //	start.right = start.centerx;
    //	start.top = game->altitude - 200;


    b->s.center.x = xres/2-200;
    b->s.center.y = (game->altitude- (yres/2)+10);
    b->velocity.x = 0;
    b->velocity.y=0;
    game->n++;
    start_flag = false;
}

void check_mouse(XEvent *e, Game *game)
{
    static int savex = 0;
    static int savey = 0;
    static int n = 0;

    if (e->type == ButtonRelease) {
	return;
    }
    if (e->type == ButtonPress) {
	if (e->xbutton.button==1) {
	    //Left button was pressed
	    if(start_flag)
		makeCharacter(game);
	    return;
	}
	if (e->xbutton.button==3) {
	    //Right button was pressed
	    return;
	}
    }
    //Did the mouse move?
    if (savex != e->xbutton.x || savey != e->xbutton.y) {
	savex = e->xbutton.x;
	savey = e->xbutton.y;
	if (++n < 10)
	    return;
    }
}

void init_keys() {
    memset(keys, 0, 65536);
}

int check_keys(XEvent *e)
{
    //keyboard input?
    static int shift=0;
    int key = XLookupKeysym(&e->xkey, 0);
    //Log("key: %i\n", key);
    if (e->type == KeyRelease) {
	keys[key]=0;
	if (key == XK_Shift_L || key == XK_Shift_R)
	    shift=0;
	return 0;
    }
    if (e->type == KeyPress) {
	keys[key]=1;
	if (key == XK_Shift_L || key == XK_Shift_R) {
	    shift=1;
	    return 0;
	}
    } else {
	return 0;
    }
    if (shift){}
    switch(key) {
	case XK_Escape:
	    return 1;
	case XK_f:
	    break;
	case XK_s:
	    break;
	case XK_Down:
	    break;
	case XK_equal:
	    break;
	case XK_minus:
	    break;
    }
    return 0;
}
/*
   int check_keys(XEvent *e, Game *game)
   {
   Character *p;
//Was there input from the keyboard?
if (e->type == KeyPress) {
int key = XLookupKeysym(&e->xkey, 0);
if (key == XK_Escape) {
return 1;
}
}
p = &game->character;
//You may check other keys here.
if (e->type == KeyPress) {
int key = XLookupKeysym(&e->xkey, 0);
if (key == XK_Left) {
p->s.center.x -= 16.0; 
} else if (key == XK_Right) {
p->s.center.x += 16.0; 
}

}
return 0;
}
*/
/*
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
}
*/
void movement(Game *game)
{
    Character *p;
    Character *b;
    if (game->n <= 0)
	return;

    p = &game->character;
    p->s.center.x += p->velocity.x;
    p->s.center.y += p->velocity.y;
    p->s.center.y -= GRAVITY;
    game->altitude -= GRAVITY;
    gCameraY += (float)GRAVITY;

    b = &game->BlueBird;
    b->s.center.x += b->velocity.x;
    b->s.center.y += b->velocity.y;
    b->s.center.y -= GRAVITY;
    //game->altitude -= GRAVITY;
    //gCameraY += (float)GRAVITY;


    //check for collision with objects here...
    //Shape *s;
    if (keys[XK_Right]) {
	p->velocity.x += 2;
    }
    if (keys[XK_Left]) {
	p->velocity.x += -2;
    }
    if (keys[XK_Up]) {
	p->velocity.y += 2;
    }
    if (keys[XK_Down]) {
	p->velocity.y -= 2;
    }

    //border collision detection
    //
    if (p->s.center.x <= 50) {
	p->velocity.x = 3;
    }
    if (p->s.center.x >= (xres - 50)) {
	p->velocity.x = -3;
    }
    if (p->s.center.y >= (game->altitude - 50)) {
	p->velocity.y = -3;
    }
    if (p->s.center.y <= (game->altitude - (yres - 50))) {
	p->velocity.y = 3;
    }

    //check for off-screen
    /*
       if (p->s.center.y < 0.0) {
       game->n = 0;
       }
       if(size_flag == true){
       if(p->s.center.x < 0.0){
       p->s.center.x += xres;
       }
       else if(p->s.center.x > xres) {
       p->s.center.x -= xres;
       }
       }
       else if(size_flag == false){
       if(p->s.center.x < 0.0) {
       p->s.center.x += WINDOW_WIDTH;
       }
       else if(p->s.center.x > WINDOW_WIDTH) {
       p->s.center.x -= WINDOW_WIDTH;
       }
       }
       */
}

void render(Game *game)
{
    if(!start_flag) {
	float w, h;
	glClear(GL_COLOR_BUFFER_BIT);
	//Pop default matrix onto current matrix
	glMatrixMode(GL_MODELVIEW);
	//glPopMatrix();
	//Save default matrix again
	glPushMatrix();
	glTranslatef(0.f, gCameraY, 0.f);
	Vec *c = &game->character.s.center;
	Vec *b = &game->BlueBird.s.center;
	w = 49;
	h = 79;
	//int wB= 65;
	//int hB= 26;
	int wB=17;
	int hB= 13;

	glColor3f(1.0, 1.0, 1.0);
	if (sky) {
	    glBindTexture(GL_TEXTURE_2D, skyTexture);
	    glBegin(GL_QUADS);
	    int ybottom = game->altitude - yres;
	    glTexCoord2f(0.0f, 1.0f); glVertex2i(0, ybottom);
	    glTexCoord2f(0.0f, 0.0f); glVertex2i(0, game->altitude);
	    glTexCoord2f(1.0f, 0.0f); glVertex2i(xres, game->altitude);
	    glTexCoord2f(1.0f, 1.0f); glVertex2i(xres, ybottom);
	    glEnd();
	}
	//glBindTexture(GL_TEXTURE_2D, characterTexture);
	glBindTexture(GL_TEXTURE_2D, silhouetteTexture);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.0f);
	glColor4ub(255,255,255,255);
	glBegin(GL_QUADS);
	if (game->character.velocity.x < 0) {
	    glTexCoord2f(0.0f, 1.0f); glVertex2i(c->x-w, c->y-h);
	    glTexCoord2f(0.0f, 0.0f); glVertex2i(c->x-w, c->y+h);
	    glTexCoord2f(0.5f, 0.0f); glVertex2i(c->x+w, c->y+h);
	    glTexCoord2f(0.5f, 1.0f); glVertex2i(c->x+w, c->y-h);
	    glEnd();
	}
	if (game->character.velocity.x >= 0) {
	    glTexCoord2f(0.5f, 1.0f); glVertex2i(c->x-w, c->y-h);
	    glTexCoord2f(0.5f, 0.0f); glVertex2i(c->x-w, c->y+h);
	    glTexCoord2f(1.0f, 0.0f); glVertex2i(c->x+w, c->y+h);
	    glTexCoord2f(1.0f, 1.0f); glVertex2i(c->x+w, c->y-h);
	    glEnd();
	}	

	if(game->altitude < 11500 && game->altitude > 10000)
	{

	 glBindTexture(GL_TEXTURE_2D, BsilhouetteTexture);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.0f);
	glColor4ub(255,255,255,255);
	    glBegin(GL_QUADS);
	    glTexCoord2f(0.0f, 1.0f); glVertex2i(b->x-wB, b->y-hB);
	    glTexCoord2f(0.0f, 0.0f); glVertex2i(b->x-wB, b->y+hB);
	    glTexCoord2f(0.5f, 0.0f); glVertex2i(b->x+wB, b->y+hB);
	    glTexCoord2f(0.5f, 1.0f); glVertex2i(b->x+wB, b->y-hB);
	    glEnd();
	   
	}	
	    //  b->s.center.x += b->velocity.x;
	    //	bb->s.center.x += 2;



	glEnd();

	int i = STARTING_ALTITUDE;
	while (i > 0) {
	    if ((game->altitude < (i + 400)) && (game->altitude > (i - 400))) {
		Rect r;
		char cstr[10];
		r.left = xres - 50;
		r.bot = i - yres/2;
		r.center = xres - 50;
		r.width = 500;
		r.height = 100;
		sprintf (cstr, "%d", i);
		strcat (cstr, "ft");
		ggprint16(&r, 16, 0xdd4814, "%s", cstr);
	    }
	    i = i - 100;
	}

	glPopMatrix();
    }

    if(start_flag) {
	glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	//glPopMatrix();
	glPushMatrix();
	glColor3f(1.0, 1.0, 1.0);
	if (sky) {
	    glBindTexture(GL_TEXTURE_2D, skyTexture);
	    glBegin(GL_QUADS);
	    int ybottom = game->altitude - yres;
	    glTexCoord2f(0.0f, 0.0f); glVertex2i(0, ybottom);
	    glTexCoord2f(1.0f, 0.0f); glVertex2i(0, game->altitude);
	    glTexCoord2f(1.0f, 1.0f); glVertex2i(xres, game->altitude);
	    glTexCoord2f(0.0f, 1.0f); glVertex2i(xres, ybottom);
	    glEnd();
	}


	glPopMatrix();

	Rect start;
	Rect click;

	start.centerx = xres/2;
	start.centery = game->altitude - 200;
	start.bot = game->altitude - 200;
	start.width = 500;
	start.height = 100;
	start.center = xres/2 + 200;
	start.left = start.centerx;
	start.right = start.centerx;
	start.top = game->altitude - 200;

	click.centerx = xres/2;
	click.centery = game->altitude - 400;
	click.bot = game->altitude - 400;
	click.width = 500;
	click.height = 100;
	click.center = xres/2;
	click.left = click.centerx;
	click.right = click.centerx;
	click.top = game->altitude - 400;

	ggprint16(&start, 1000, 0x00fff000, "PARASHOOT!");
	ggprint16(&click, 1000, 0x00fff000, "Click to start");
    }
}
