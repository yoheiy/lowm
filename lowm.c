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
   int f_kill; /* killed as line */
   int f_icon; /* this is icon */
   int x, y;
   unsigned int w, h, bw; /* geometry */
   struct size_hint hints;
} clients[80];
int nr_clients;
int world_y, realm_x;
int cursor;
int screen_width, screen_height;
int monocle_mode, fill_mode;
const char * const bind_keys = "4BDFGHJKLMNPWX";

/* config */
const int gap = 8, left_gap = 16, monocle_gap = 8;
const int icon_width = 60, icon_height = 60, cursor_gap = 32;

/* client */
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

void
apply_hints(struct client *p)
{
   int bw, wi, xw, mw;
   int bh, hi, xh, mh;

   bw = p->hints.base_width;
   wi = p->hints.width_inc;
   xw = p->w - bw;
   mw = p->hints.min_width;
   if (wi)
      p->w = bw + xw / wi * wi;
   if (p->w < mw)
      p->w = mw;

   bh = p->hints.base_height;
   hi = p->hints.height_inc;
   xh = p->h - bh;
   mh = p->hints.min_height;
   if (hi)
      p->h = bh + xh / hi * hi;
   if (p->h < mh)
      p->h = mh;
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
is_client_icon(struct client *p)
{
   return p->f_icon;
}

void
make_it_normal(struct client *p)
{
   p->f_icon = 0;
}

void
make_it_icon(struct client *p)
{
   p->f_icon = 1;
}

int
cli_w(struct client *p)
{
   return is_client_icon(p) ? icon_width - 2 * p->bw : p->w;
}

int
cli_h(struct client *p)
{
   return is_client_icon(p) ? icon_height - 2 * p->bw : p->h;
}

int
cli_w_b(struct client *p)
{
   return is_client_icon(p) ? icon_width : p->w + 2 * p->bw;
}

int
cli_h_b(struct client *p)
{
   return is_client_icon(p) ? icon_height : p->h + 2 * p->bw;
}

/* line */
int
line_head(int b)
{
   struct client *p;
   int i;

   for (i = b; i > 0; i--) {
      p = &clients[i];
      if (is_line_head(p)) break;
   }
   return i;
}

int
line_len(int b)
{
   struct client *p;
   int i;

   for (i = b; i < nr_clients; i++) {
      p = &clients[i];
      if (is_line_head(p) && i > b) break;
   }
   return i - b;
}

int
fill_line(int b)
{
   struct client *p;
   int i, a = left_gap;

   for (i = b; i < nr_clients; i++) {
      p = &clients[i];
      if (is_line_head(p) && i > b) break;

      a += p->f ? p->hints.base_width : p->w;
      a += 2 * p->bw + gap;
      a += cursor_gap * (i == cursor);
   }
   if (a > screen_width) return 0;
   return screen_width - a;
}

int
n_fill(int b)
{
   struct client *p;
   int i, n = 0;

   for (i = b; i < nr_clients; i++) {
      p = &clients[i];
      if (is_line_head(p) && i > b) break;
      if (p->f) n++;
   }
   return n;
}

int
realm_here(int b)
{
   int e;

   e = b + line_len(b);
   return cursor >= b && cursor < e;
}

/* world */
void
align(void)
{
   struct client *p = &clients[cursor];

if (nr_clients == 0) return;

   if (p->y + world_y < 0)
      world_y = -p->y;
   else if (p->y + cli_h_b(p) + world_y > screen_height)
      world_y = screen_height - (p->y + cli_h_b(p));

   if (p->x + cli_w_b(p) > screen_width)
      realm_x = screen_width - (p->x + cli_w_b(p));
   else
      realm_x = 0;
}

void
arrange(void)
{
   int i, x = 0, y = 0, line_height = 0, window_height, f;
   struct client *p;

   for (i = 0; i < nr_clients; i++) {
      p = &clients[i];

      if (is_line_head(p)) {
         x = left_gap;
         y = y + line_height + gap;
         line_height = 0;
         f = fill_line(i) / (n_fill(i) ?: 1);
      }
      window_height = cli_h_b(p);
      line_height = MAX(line_height, window_height);

      if (p->f) {
         p->w = p->hints.base_width + f;
         apply_hints(p);
      }
      p->x = x;
      p->y = y;

      if (i == cursor) {
         p->x += cursor_gap;
         x    += cursor_gap;
      }
      x += cli_w_b(p) + gap;
   }
}

void
place_monocle(void)
{
   struct client c;

if (nr_clients == 0) return;

   c = clients[cursor];
   c.x = c.y = monocle_gap - c.bw;
   c.w = screen_width  - 2 * monocle_gap;
   c.h = screen_height - 2 * monocle_gap;
   apply_hints(&c);
   XMoveResizeWindow(Dpy, c.id, c.x, c.y, c.w, c.h);
   XRaiseWindow(Dpy, c.id);
}

int
is_line_tail(int i)
{
   return i + 1 == nr_clients || is_line_head(&clients[i + 1]);
}

void
place_world(void)
{
   int i, realm;
   struct client *p, c;

   if (monocle_mode)
      return place_monocle();

   for (i = 0; i < nr_clients; i++) {
      p = &clients[i];
      if (is_line_head(p))
         realm = realm_here(i);
      c = *p;
      c.y += world_y;
      if (realm)
         c.x += realm_x;
      c.w = cli_w(p);
      c.h = cli_h(p);

      if (fill_mode && is_line_tail(i) && !is_client_icon(p))
         c.w = MAX(c.w, screen_width - c.x - gap);

      apply_hints(&c);
      XMoveResizeWindow(Dpy, c.id, c.x, c.y, c.w, c.h);
   }
}

/* client list */
void
list_append(const struct client *new)
{
   clients[nr_clients] = *new;
   nr_clients++;
}

void
rotate_left(int b, int l)
{
   int i;

   for (i = 0; i < l; i++)
      clients[nr_clients + i] = clients[b + i];

   for (i = b; i < nr_clients; i++)
      clients[i] = clients[i + l];
}

void
rotate_right(int b, int l)
{
   int i;

   for (i = nr_clients - 1; i >= b; i--)
      clients[i + l] = clients[i];

   for (i = 0; i < l; i++)
      clients[b + i] = clients[nr_clients + i];
}

void
paste_window(int n)
{
   int b, l, i, o;

   if (nr_clients < 2) return;

   b = line_head(nr_clients - 1);
   l = line_len(b);

   if (clients[b].f_kill) {
      o = line_head(cursor) + (n > 0) * line_len(cursor);
   } else {
      if (n < 0 && is_line_head(&clients[cursor])) {
         make_it_rest(&clients[cursor]);
         make_it_head(&clients[b]);
      } else {
         make_it_rest(&clients[b]);
      }
      o = cursor + (n > 0);
   }
   for (i = b; i < b + l; i++)
      make_it_normal(&clients[i]);

   rotate_right(o, l);
}

void
cut_line(int n)
{
   int b, l, i;

if (nr_clients == 0) return;

   b = line_head(cursor);
   l = line_len(b);

   clients[b].f_kill = 1;
   for (i = b; i < b + l; i++)
      make_it_icon(&clients[i]);

   rotate_left(b, l);
}

void
cut_window(int n)
{
   int b, l;

if (nr_clients == 0) return;

   b = cursor, l = 1;
   if (is_line_head(&clients[b]))
      make_it_head(&clients[b + 1]);
   else
      make_it_head(&clients[b]);
      //cursor--;

   clients[b].f_kill = 0;
   make_it_icon(&clients[b]);

   rotate_left(b, l);
}

void
delete_window(Window w)
{
   int i;

   for (i = 0; i < nr_clients; i++)
      if (clients[i].id == w)
         break;
   if (i == nr_clients) return; /* not found */

   if (is_line_head(&clients[i]))
      make_it_head(&clients[i + 1]);
   rotate_left(i, 1);
   nr_clients--;

   if (i < cursor) cursor--;
   if (cursor == nr_clients) cursor--;
}

int
newwindow(Window w)
{
   XWindowAttributes wattr;
   struct client new = { 0, };

   XGetWindowAttributes (Dpy, w, &wattr);
   if (wattr.override_redirect == True)
      return 0;

   if (nr_clients == 0) cursor = 0;

   new.id = w;
   get_geometry_xywh(&new);
   get_size_hints(&new);
   list_append(&new);
   return 1;
}

void
new_window(Window w)
{
   XWindowAttributes wattr;
   struct client new = { 0, };

   XGetWindowAttributes (Dpy, w, &wattr);
   if (wattr.override_redirect == True)
      return;
   if (wattr.map_state != IsViewable)
      return;

   new.id = w;
   get_geometry_xywh(&new);
   get_size_hints(&new);
   list_append(&new);
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
      new_window(r_ch[i]);

   XFree (r_ch);
}

int
find_head_next(int b)
{
   int i;

   for (i = b + 1; i < nr_clients; i++)
      if (is_line_head(&clients[i]))
         break;
   return i;
}

int
find_last_line(void)
{
   int i, l = 0;
   struct client *p;

   for (i = 0; i < nr_clients; i++) {
      p = &clients[i];
      if (is_client_icon(p))
         break;
      if (is_line_head(p))
         l = i;
   }
   return l;
}

void
move_cursor(int n)
{
   int i;

   if (n > 0) {
      i = find_head_next(cursor);
      if (i < nr_clients) cursor = i;
   } else {
      cursor = line_head(cursor);
      if (cursor) cursor--;
      cursor = line_head(cursor);
   }
}

void
move_cursor_inline(int n)
{
   if (n > 0) {
      if (cursor + 1 >= nr_clients) return;
      if (is_line_head(&clients[cursor + 1])) return;
      cursor++;
   } else {
      if (cursor == 0) return;
      if (is_line_head(&clients[cursor])) return;
      cursor--;
   }
}

void
resize_window(int x, int y)
{
   struct client *p = &clients[cursor];

if (nr_clients == 0) return;

   p->w += x * (p->hints.width_inc  ?: 32);
   p->h += y * (p->hints.height_inc ?: 32);

   if (p->w < p->hints.min_width)  p->w = p->hints.min_width;
   if (p->h < p->hints.min_height) p->h = p->hints.min_height;

   XResizeWindow(Dpy, p->id, p->w, p->h);
}

void
join(void)
{
   int i;

if (nr_clients == 0) return;

   i = find_head_next(cursor);
   make_it_rest(&clients[i]);
}

void
cut(void)
{
if (nr_clients == 0) return;

   make_it_head(&clients[cursor]);
}

void
key_event_handler(KeySym k)
{
   int no_arrange = 0;

   switch (k) {
   case XK_j:
      move_cursor(1);
      break;
   case XK_k:
      move_cursor(-1);
      break;
   case XK_l:
      move_cursor_inline(1);
      break;
   case XK_h:
      move_cursor_inline(-1);
      break;
   case XK_g:
      world_y = cursor = 0;
      break;
   case XK_G:
      cursor = find_last_line();
      break;
   case XK_dollar:
      cursor = find_head_next(cursor) - 1;
      break;

   case XK_J:
      join();
      break;
   case XK_K:
      cut();
      break;

   case XK_x:
      cut_window(1);
      break;
   case XK_d:
      cut_line(1);
      break;
   case XK_p:
      paste_window(1);
      break;
   case XK_P:
      paste_window(-1);
      break;

   case XK_n:
      resize_window(-1, 0);
      break;
   case XK_N:
      resize_window(0, -1);
      break;
   case XK_w:
      resize_window(1, 0);
      break;
   case XK_W:
      resize_window(0, 1);
      break;

   case XK_L:
      world_y = -clients[cursor].y;
      no_arrange = 1;
      break;
   case XK_f:
      world_y -= 100;
      no_arrange = 1;
      break;
   case XK_b:
      world_y += 100;
      no_arrange = 1;
      break;

   case XK_m:
      monocle_mode = !monocle_mode;
      no_arrange = 1;
      break;
   case XK_F:
      fill_mode = !fill_mode;
      no_arrange = 1;
      //clients[cursor].f = !clients[cursor].f;
      break;
   }
   if (!no_arrange) arrange();
   align();
   place_world();
}

KeySym
key_convert(XEvent e)
{
   KeySym k;
   int a;

   a = e.xkey.state & ShiftMask ? 1 : 0;
   k = XLookupKeysym(&e.xkey, a);

   return k;
}

void
map_req_event_handler(Window w)
{
   if (!newwindow(w)) return;
   rotate_right(find_head_next(cursor), 1);
   arrange();
   align();
   place_world();
   XMapRaised(Dpy, w);
}

void
unmap_event_handler(Window w)
{
   delete_window(w);
   arrange();
   align();
   place_world();
}

void
set_focus(void)
{
   XSetInputFocus(Dpy, clients[cursor].id, RevertToPointerRoot, CurrentTime);
}

void
mainloop_body(void)
{
   XEvent e;

   XNextEvent(Dpy, &e);
   switch (e.type) {
   case MapRequest:
      map_req_event_handler(e.xmaprequest.window);
      break;
   case UnmapNotify:
      unmap_event_handler(e.xunmap.window);
      break;
   case KeyPress:
      key_event_handler(key_convert(e));
      break;
   }
}

void
mainloop(void)
{
   for (;;) {
      set_focus();
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
