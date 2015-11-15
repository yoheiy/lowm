#include <X11/Xlib.h>
#include <X11/keysym.h>
Display *Dpy;
Window   Root;

struct client {
   Window id;
   int z; /* =0 if leftmost window of a line */
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
   int i, x = 0, y = 0, line_height = 0;
   struct client *p;

   for (i = 0; i < nr_clients; i++) {
      p = &clients[i];

      if (!p->z) {
         x = 0;
         y = y + line_height + gap;
         line_height = p->h;
      }
      else {
         if (p->h > line_height)
            line_height = p->h;
      }
      p->x = x;
      p->y = y;

      if (i == cursor) {
         p->x += 32;
         x    += 32;
      }
      x += p->w + gap;
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
   if (wattr.map_state == IsViewable && wattr.override_redirect == False) {
      clients[nr_clients].id = w;
      get_geometry_xywh(&clients[nr_clients]);
      nr_clients++;
   }
}

void
delete_window(Window w)
{
   int i;

   for (i = 0; i < nr_clients; i++)
      if (clients[i].id == w)
         break;
   clients[i + 1].z = 0;
   nr_clients--;
   for (; i < nr_clients; i++)
      clients[i] = clients[i + 1];
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
move_cursor(int n)
{
   int i;

   if (n > 0) {
      for (i = cursor + 1; i < nr_clients; i++)
         if (!clients[i].z) break;
      if (i >= nr_clients) return;
      if (clients[i].z) return;
   } else {
      for (i = cursor; i >= 0 ; i--) /* go to line head */
         if (!clients[i].z) break;
      for (i--; i >= 0 ; i--)
         if (!clients[i].z) break;
      if (i < 0) return;
   }
   cursor = i;

   if (clients[cursor].y + world_y < 0)
      world_y = -clients[cursor].y;

   arrange();
}

void
move_cursor_inline(int n)
{
   if (n > 0) {
      if (cursor + 1 >= nr_clients) return;
      if (!clients[cursor + 1].z) return;
      cursor++;
   } else {
      if (cursor == 0) return;
      if (!clients[cursor].z) return;
      cursor--;
   }
   arrange();
}

void
join(void)
{
   int i;

   for (i = cursor + 1; i < nr_clients; i++)
      if (!clients[i].z)
         break;
   clients[i].z = 1;
}

void
cut(void)
{
   clients[cursor].z = 0;
}

void
mainloop_body(void)
{
   XEvent e;
   Window w;

   XNextEvent(Dpy, &e);
   switch (e.type) {
   case MapRequest:
      w = e.xmaprequest.window;
      XMapRaised(Dpy, w);
      newwindow(w);
      arrange();
      place_world();
      break;
   case UnmapNotify:
      w = e.xunmap.window;
      delete_window(w);
      arrange();
      place_world();
      break;
   case KeyPress:
      if (e.xkey.keycode == XKeysymToKeycode(Dpy, XK_F))
         world_y -= 100;
      else
      if (e.xkey.keycode == XKeysymToKeycode(Dpy, XK_B))
         world_y += 100;
      else
      if (e.xkey.keycode == XKeysymToKeycode(Dpy, XK_G)) {
         world_y = cursor = 0;
         arrange(); }
      else
      if (e.xkey.keycode == XKeysymToKeycode(Dpy, XK_J)) {
         if (e.xkey.state & ShiftMask) {
            join();
            arrange(); }
         else
            move_cursor(1); }
      else
      if (e.xkey.keycode == XKeysymToKeycode(Dpy, XK_K)) {
         if (e.xkey.state & ShiftMask) {
            cut();
            arrange(); }
         else
            move_cursor(-1); }
      else
      if (e.xkey.keycode == XKeysymToKeycode(Dpy, XK_L)) {
         if (e.xkey.state & ShiftMask)
            world_y = -clients[cursor].y;
         else
            move_cursor_inline(1); }
      else
      if (e.xkey.keycode == XKeysymToKeycode(Dpy, XK_H)) {
         if (e.xkey.state & ShiftMask)
            ;
         else
            move_cursor_inline(-1); }
      place_world();
      break;
   }
}

void
mainloop(void)
{
   char *c, s[2] = "X";

   XSelectInput(Dpy, Root, SubstructureRedirectMask |
                           SubstructureNotifyMask);

   for (c = "BFGHJKL"; *c; c++) {
      s[0] = *c;
      XGrabKey(Dpy, XKeysymToKeycode(Dpy, XStringToKeysym(s)), Mod1Mask, Root,
            True, GrabModeAsync, GrabModeAsync);
      XGrabKey(Dpy, XKeysymToKeycode(Dpy, XStringToKeysym(s)),
            Mod1Mask | ShiftMask, Root,
            True, GrabModeAsync, GrabModeAsync);
   }

   for (;;) {
      mainloop_body();
   }
}

int
xerror(Display *dpy, XErrorEvent *e)
{
   return 0;
}

int
main(void)
{
   Dpy = XOpenDisplay(NULL);
   if (!Dpy) return 1;
   Root = DefaultRootWindow(Dpy);
   XSetErrorHandler(xerror);

   init_clients();
   arrange();
   place_world();
   mainloop();
   XCloseDisplay(Dpy);
   return 0;
}
