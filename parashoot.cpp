#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cmath>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <X11/Xutil.h>
#include "log.h"
#include "ppm.h"
extern "C" {
#include "fonts.h"
}
#define WINDOW_WIDTH  1250
#define WINDOW_HEIGHT 900
#define MAX_PARTICLES 1
#define GRAVITY 3.0

//X Windows variables
Display *dpy;
Window win;
GLXContext glc;

int xres= WINDOW_WIDTH, yres=WINDOW_HEIGHT;
bool size_flag = false;//Structures
bool start_flag = true;

struct Vec {
    float x, y, z;
};

struct Shape {
    float width, height;
    float radius;
    Vec center;
};

struct Particle {
    Shape s;
    Vec velocity;
};

struct Game {
    Shape box;
    Particle particle;
    int n;
};


//Function prototypes
void initXWindows(void);
void init_opengl(void);
void cleanupXWindows(void);
void check_mouse(XEvent *e, Game *game);
int check_keys(XEvent *e, Game *game);
void movement(Game *game);
void render(Game *game);
void check_resize(Game *game, XEvent *e);

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
    init_opengl();
    //declare game object
    Game game;
    game.n=0;
    clock_gettime(CLOCK_REALTIME, &timePause);
    clock_gettime(CLOCK_REALTIME, &timeStart);

    //start animation
    while(!done) {
	while(XPending(dpy)) {
	    XEvent e;
	    XNextEvent(dpy, &e);
	    check_mouse(&e, &game);
	    check_resize(&game, &e);
	    done = check_keys(&e, &game);
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
    Particle *p = &game->particle;
    size_flag = true;
    setup_screen_res(width, height);
    p->s.center.x = xres/2;
    p->s.center.y = yres/2;
    glViewport(0,0, (GLint)width, (GLint)height);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glOrtho(0, xres, 0, yres, -1, 1);
    set_title();
    p->s.center.x = xres/2;
    p->s.center.y = yres/2;
}

void init_opengl(void) 
{
    //OpenGL initialization
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    //Initialize matrices
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    //Set 2D mode (no perspective)
    glOrtho(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT, -1, 1);
    //Set the screen background color
    glClearColor(0.1, 0.5, 1.0, 1.0);
    glEnable(GL_TEXTURE_2D);
    initialize_fonts();
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

void makeParticle(Game *game)
{
    //position of particle
    Particle *p = &game->particle;
    p->s.center.x = WINDOW_WIDTH/2;
    p->s.center.y = WINDOW_HEIGHT/2;
    p->velocity.y = 0;
    p->velocity.x = 0;
    game->n++;
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
	    start_flag = false;
	    makeParticle(game);
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

int check_keys(XEvent *e, Game *game)
{
    Particle *p;
    //Was there input from the keyboard?
    if (e->type == KeyPress) {
	int key = XLookupKeysym(&e->xkey, 0);
	if (key == XK_Escape) {
	    return 1;
	}
    }
    p = &game->particle;
    //You may check other keys here.
    if (e->type == KeyPress) {
	int key = XLookupKeysym(&e->xkey, 0);
	if (key == XK_Left) {
	    p->s.center.x -= 16.0; }
	else if (key == XK_Right) {
	    p->s.center.x += 16.0; }

    }
    return 0;
}

void movement(Game *game)
{
    Particle *p;

    if (game->n <= 0)
	return;

    p = &game->particle;
    /*p->s.center.x += p->velocity.x;
      p->s.center.y += p->velocity.y;
      p->s.center.y -= GRAVITY;
      */
    //check for collision with shapes...
    //Shape *s;


    //check for off-screen
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
}

void render(Game *game)
{
    float w, h;
    glClear(GL_COLOR_BUFFER_BIT);
    //draw character here
    glPushMatrix();
    glColor3ub(125,0,220);
    Vec *c = &game->particle.s.center;

        w = 12;
    h = 12;
    glBegin(GL_QUADS);
    glVertex2i(c->x-w, c->y-h);
    glVertex2i(c->x-w, c->y+h);
    glVertex2i(c->x+w, c->y+h);
    glVertex2i(c->x+w, c->y-h);
    glEnd();
    glPopMatrix();
    
    if(start_flag)
    {
    Rect start;
    Rect click;

    start.centerx = xres/2;
    start.centery = yres/2 + 400;
    start.bot = yres/2 + 400;
    start.width = 500;
    start.height = 100;
    start.center = xres/2 + 400;
    start.left = start.centerx;
    start.right = start.centerx;
    start.top = yres/2 + 400;

    click.centerx = xres/2;
    click.centery = yres/2 + 375;
    click.bot = yres/2 + 375;
    click.width = 500;
    click.height = 100;
    click.center = xres/2 + 375;
    click.left = click.centerx;
    click.right = click.centerx;
    click.top = yres/2 + 375;
    
    ggprint16(&start, 1000, 0x00fff000, "PARASHOOT!");
    ggprint16(&click, 1000, 0x00fff000, "Click to start");
    }
}
