#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#define MAX(x, y) ((x) > (y) ? (x) : (y))
void exit(int status);

Display *Dpy;
Window   Root;

struct size_hint {
   int base_width, base_height;
   int width_inc,  height_inc;
   int min_width,  min_height;
};
struct client {
   Window id;
   int z; /* =0 if leftmost window of a line */
   int f; /* fill */
   int x, y;
   unsigned int w, h, bw; /* geometry */
   struct size_hint hints;
} clients[80];
int nr_clients;
int world_y, realm_x;
int cursor;
int screen_width, screen_height;
int monocle_mode;
const char * const bind_keys = "BFGHJKLMNPWX";
const int gap = 8, monocle_gap = 8;

void
get_geometry_xywh(struct client *c)
{
   Window r;
   unsigned int d;

   XGetGeometry(Dpy, c->id, &r,
         &c->x, &c->y, &c->w, &c->h, &c->bw, &d);
}

void
get_size_hints(struct client *c)
{
   XSizeHints hints;
   long supplied;

   XGetWMNormalHints(Dpy, c->id, &hints, &supplied);
   c->hints.base_width  = (supplied & PBaseSize) ? hints.base_width  :
                          (supplied & PMinSize)  ? hints.min_width   : 32;
   c->hints.base_height = (supplied & PBaseSize) ? hints.base_height :
                          (supplied & PMinSize)  ? hints.min_height  : 32;
   c->hints.width_inc   = (supplied & PResizeInc) ? hints.width_inc  : 0;
   c->hints.height_inc  = (supplied & PResizeInc) ? hints.height_inc : 0;
   c->hints.min_width   = (supplied & PMinSize)  ? hints.min_width   : 32;
   c->hints.min_height  = (supplied & PMinSize)  ? hints.min_height  : 32;
   if (c->hints.min_width  == 0) c->hints.min_width  = 32;
   if (c->hints.min_height == 0) c->hints.min_height = 32;
}

int
is_line_head(struct client *p)
{
   return !p->z;
}

void
make_it_head(struct client *p)
{
   p->z = 0;
}

void
make_it_rest(struct client *p)
{
   p->z = 1;
}

int
fill_line(int b)
{
   struct client *p;
   int i, a = 0, n = 0;

   for (i = b; i < nr_clients; i++) {
      p = &clients[i];
      if (is_line_head(p) && i > b) break;

      if (p->f) {
         a += p->hints.base_width;
         n++; }
      else
         a += p->w;

      a += 2 * p->bw + gap;

      if (i == cursor)
         a += 32;
   }
   if (!n) return 0;
   if (a > screen_width) return 0;
   return (screen_width - a) / n;
}

int
realm_here(int b)
{
   struct client *p;
   int i;

   for (i = b; i < nr_clients; i++) {
      p = &clients[i];
      if (is_line_head(p) && i > b) break;
      if (i == cursor) return 1;
   }
   return 0;
}

void
apply_hints(struct client *p)
{
   int bw, wi, xw;

   bw = p->hints.base_width;
   wi = p->hints.width_inc;
   xw = p->w - bw;
   if (wi)
      p->w = bw + xw / wi * wi;
}

void
arrange(void)
{
   int i, x = 0, y = 0, line_height = 0, window_height, f;
   struct client *p;

   for (i = 0; i < nr_clients; i++) {
      p = &clients[i];

      if (is_line_head(p)) {
         x = 0;
         y = y + line_height + gap;
         line_height = 0;
         f = fill_line(i);
      }
      window_height = p->h + 2 * p->bw;
      line_height = MAX(line_height, window_height);

      if (p->f) {
         p->w = p->hints.base_width + f;
         apply_hints(p);
      }
      p->x = x;
      p->y = y;

      if (i == cursor) {
         p->x += 32;
         x    += 32;
      }
      x += p->w + 2 * p->bw + gap;
   }
}

void
place_world(void)
{
   int i, realm;
   struct client *p, c;

   if (monocle_mode) {
      c = clients[cursor];
      c.x = c.y = monocle_gap - c.bw;
      c.w = screen_width  - 2 * monocle_gap;
      c.h = screen_height - 2 * monocle_gap;
      apply_hints(&c);
      XMoveResizeWindow(Dpy, c.id, c.x, c.y, c.w, c.h);
      XRaiseWindow(Dpy, c.id);
      return;
   }

   for (i = 0; i < nr_clients; i++) {
      p = &clients[i];
      if (is_line_head(p))
         realm = realm_here(i);
      c = *p;
      c.y += world_y;
      if (realm)
         c.x += realm_x;
      XMoveResizeWindow(Dpy, c.id, c.x, c.y, c.w, c.h);
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
      get_size_hints(&clients[nr_clients]);
      nr_clients++;
   }
}

void
paste_window(int n)
{
   int i, o;

   if (nr_clients < 2) return;

   if (n < 0 && is_line_head(&clients[cursor])) {
      make_it_rest(&clients[cursor]);
      make_it_head(&clients[nr_clients - 1]);
   } else {
      make_it_rest(&clients[nr_clients - 1]);
   }

   o = cursor + (n > 0);

   for (i = nr_clients - 1; i >= o; i--)
      clients[i + 1] = clients[i];

   clients[o] = clients[nr_clients];

   arrange();
}

void
cut_window(int n)
{
   int i, o;

   if (is_line_head(&clients[cursor]))
      make_it_head(&clients[cursor]);

   o = cursor;
   clients[nr_clients] = clients[o];
   make_it_head(&clients[nr_clients]);

   for (i = o; i < nr_clients; i++)
      clients[i] = clients[i + 1];

   arrange();
}

void
delete_window(Window w)
{
   int i;

   for (i = 0; i < nr_clients; i++)
      if (clients[i].id == w)
         break;
   make_it_head(&clients[i + 1]);
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
   struct client *p;
   int i;

   if (n > 0) {
      for (i = cursor + 1; i < nr_clients; i++)
         if (is_line_head(&clients[i])) break;
      if (i >= nr_clients) return;
      if (!is_line_head(&clients[i])) return;
   } else {
      for (i = cursor; i >= 0 ; i--) /* go to line head */
         if (is_line_head(&clients[i])) break;
      for (i--; i >= 0 ; i--)
         if (is_line_head(&clients[i])) break;
      if (i < 0) return;
   }
   cursor = i;

   p = &clients[cursor];

   if (p->y + world_y < 0)
      world_y = -p->y;
   else if (p->y + p->h + world_y > screen_height)
      world_y = screen_height - (p->y + p->h);
/*
   if (clients[cursor].y + world_y < 0)
      world_y = -clients[cursor].y;
   else
   if (clients[cursor].y + clients[cursor].h + world_y > screen_height)
      world_y = screen_height - (clients[cursor].y + clients[cursor].h);
*/
   realm_x = 0;

   arrange();
}

void
move_cursor_inline(int n)
{
   struct client *p = &clients[cursor];

   if (n > 0) {
      if (cursor + 1 >= nr_clients) return;
      if (is_line_head(&clients[cursor + 1])) return;
      cursor++;
   } else {
      if (cursor == 0) return;
      if (is_line_head(&clients[cursor])) return;
      cursor--;
   }

   if (p->x + realm_x < 0)
      realm_x = -p->x;
   else if (p->x + p->w + realm_x > screen_width)
      realm_x = screen_width - (p->x + p->w);
/*
   if (clients[cursor].x + realm_x < 0)
      realm_x = -clients[cursor].x;
   else
   if (clients[cursor].x + clients[cursor].w + realm_x > screen_width)
      realm_x = screen_width - (clients[cursor].x + clients[cursor].w);
*/

   arrange();
}

void
resize_window(int x, int y)
{
   struct client *p = &clients[cursor];

   p->w += x * (p->hints.width_inc  ?: 32);
   p->h += y * (p->hints.height_inc ?: 32);

   if (p->w < p->hints.min_width)  p->w = p->hints.min_width;
   if (p->h < p->hints.min_height) p->h = p->hints.min_height;

   XResizeWindow(Dpy, p->id, p->w, p->h);

   arrange();
}

void
join(void)
{
   int i;

   /* find next head */
   for (i = cursor + 1; i < nr_clients; i++)
      if (is_line_head(&clients[i]))
         break;
   make_it_rest(&clients[i]);
}

void
cut(void)
{
   make_it_head(&clients[cursor]);
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
      if (e.xkey.keycode == XKeysymToKeycode(Dpy, XK_F)) {
         if (e.xkey.state & ShiftMask) {
            clients[cursor].f = !clients[cursor].f;
            arrange(); }
         else
            world_y -= 100; }
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
      else
      if (e.xkey.keycode == XKeysymToKeycode(Dpy, XK_P)) {
         if (e.xkey.state & ShiftMask)
            paste_window(-1);
         else
            paste_window(1); }
      else
      if (e.xkey.keycode == XKeysymToKeycode(Dpy, XK_X)) {
         if (e.xkey.state & ShiftMask)
            ;
         else
            cut_window(1); }
      else
      if (e.xkey.keycode == XKeysymToKeycode(Dpy, XK_N)) {
         if (e.xkey.state & ShiftMask)
            resize_window(0, -1);
         else
            resize_window(-1, 0); }
      else
      if (e.xkey.keycode == XKeysymToKeycode(Dpy, XK_W)) {
         if (e.xkey.state & ShiftMask)
            resize_window(0, 1);
         else
            resize_window(1, 0); }
      else
      if (e.xkey.keycode == XKeysymToKeycode(Dpy, XK_M)) {
         if (e.xkey.state & ShiftMask)
            ;
         else
            monocle_mode = !monocle_mode; }
      place_world();
      break;
   }
   XSetInputFocus(Dpy, clients[cursor].id, RevertToPointerRoot, CurrentTime);
}

void
mainloop(void)
{
   for (;;) {
      mainloop_body();
   }
}

void
select_input(void)
{
   const char *c;
   char s[2] = "X";

   XSelectInput(Dpy, Root, SubstructureRedirectMask |
                           SubstructureNotifyMask);

   for (c = bind_keys; *c; c++) {
      s[0] = *c;
      XGrabKey(Dpy, XKeysymToKeycode(Dpy, XStringToKeysym(s)), Mod1Mask, Root,
            True, GrabModeAsync, GrabModeAsync);
      XGrabKey(Dpy, XKeysymToKeycode(Dpy, XStringToKeysym(s)),
            Mod1Mask | ShiftMask, Root,
            True, GrabModeAsync, GrabModeAsync);
   }
}

int
xerror(Display *dpy, XErrorEvent *e)
{
   return 0;
}

void
init_wm(void)
{
   Dpy = XOpenDisplay(NULL);
   if (!Dpy) exit(1);

   XSetErrorHandler(xerror);
   Root = DefaultRootWindow(Dpy);
   screen_width  = XDisplayWidth(Dpy,  DefaultScreen(Dpy));
   screen_height = XDisplayHeight(Dpy, DefaultScreen(Dpy));
}

int
main(void)
{
   init_wm();
   init_clients();
   arrange();
   place_world();
   select_input();
   mainloop();
   XCloseDisplay(Dpy);
   return 0;
}
