#include <X11/Xlib.h>
#include <X11/Xlib.h>
Display *Dpy;
Window   Root;

struct client {
   Window id;
   int x, y;
   unsigned int w, h, bw; /* geometry */
} clients[80];
int nr_clients;
int world_y;
int cursor;

const int gap = 1;

void
get_geometry_xywh(struct client *c)
{
   Window r;
   unsigned int d;

   XGetGeometry(Dpy, c->id, &r,
         &c->x, &c->y, &c->w, &c->h, &c->bw, &d);
}

void
arrange(void)
{
   int i, y = 0;
   struct client *p;

   for (i = 0; i < nr_clients; i++) {
      p = &clients[i];
      p->y = y;
      y += p->h + gap;
   }
}

void
place_world(void)
{
   int i;
   struct client c;

   for (i = 0; i < nr_clients; i++) {
      c = clients[i];
      c.y += world_y;
      XMoveWindow(Dpy, c.id, c.x, c.y);
   }
}

void
newwindow (Window w)
{
   XWindowAttributes wattr;

   XGetWindowAttributes (Dpy, w, &wattr);
   if (wattr.map_state == IsViewable &&
       wattr.override_redirect == False) {
      clients[nr_clients].id = w;
      get_geometry_xywh(&clients[nr_clients]);
      nr_clients++;
   }
}

void
init_clients(void)
{
   Window r_root, r_parent, *r_ch;
   unsigned int n_ch, i;

   nr_clients = 0;
   if (!XQueryTree (Dpy, Root, &r_root, &r_parent, &r_ch, &n_ch))
      return;

   for (i = 0; i < n_ch; i++)
      newwindow (r_ch[i]);

   XFree (r_ch);
}

void
mainloop_body(void)
{
   XEvent e;
   Window w;

   XNextEvent(Dpy, &e);
   switch (e.type) {
   case MapRequest:
      w = e.xany.window;
      XMapRaised(Dpy, w);
      newwindow(w);
      arrange();
      place_world();
      break;
   case UnmapNotify:
      init_clients();
      arrange();
      place_world();
      break;
   case KeyPress:
      if (e.xkey.keycode == XKeysymToKeycode(Dpy, XStringToKeysym("F1")))
         world_y -= 100;
      else
         world_y += 100;
      place_world();
      break;
   }
}

void
mainloop(void)
{
   XSelectInput(Dpy, Root, SubstructureRedirectMask |
                           SubstructureNotifyMask);
   XGrabKey(Dpy, 'f', Mod1Mask, Root, True, GrabModeAsync, GrabModeAsync);
   XGrabKey(Dpy, 'b', Mod1Mask, Root, True, GrabModeAsync, GrabModeAsync);

   XGrabKey(Dpy, XKeysymToKeycode(Dpy, XStringToKeysym("F1")), Mod1Mask, Root,
            True, GrabModeAsync, GrabModeAsync);
   XGrabKey(Dpy, XKeysymToKeycode(Dpy, XStringToKeysym("F2")), Mod1Mask, Root,
            True, GrabModeAsync, GrabModeAsync);

   for (;;) {
      mainloop_body();
   }
}

int
main(void)
{
   Dpy = XOpenDisplay(NULL);
   if (!Dpy) return 1;
   Root = DefaultRootWindow (Dpy);

   init_clients();
   arrange();
   place_world();
   mainloop();
   XCloseDisplay(Dpy);
   return 0;
}
