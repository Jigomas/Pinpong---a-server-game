#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <curses.h>


uv_udp_t client;
char buf[1024];

static void alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

void on_send_cb(uv_udp_send_t* req, int status) {
	free(req);
}

void send_command(const char* cmd) {
	mvprintw(0, 1, "Sending command %s ", cmd);
	uv_udp_send_t* req = malloc(sizeof(uv_udp_send_t));
	uv_buf_t b = uv_buf_init(buf, 1024);
	b.len = sprintf(buf, "%s\n", cmd);
	uv_udp_send(req, &client, &b, 1, NULL, on_send_cb);
}

char myname[2];

void on_recv(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags) {
	if (nread > 0) {
		if ( scanf(buf->base, "NAME %2s ", myname)== 1) {
			mvprintw(0, 0, "My alias now %2s, bytes: %ld  ", myname, nread);
			refresh();
			//printf("My name is %2s, bytes got: %ld\n", myname, nread);
			return;
		}
		char name[2];
		int x;
		int y;
		int scanned = 0;
		int total = 0;
		//fprintf(stderr, "Clearing\n");
		clear();
		mvprintw(0, 0, "My alias is %2s, bytes got: %ld  ", myname, nread);
		while(total < nread && sscanf(buf->base+total, "POS %2s %d %d %n", name, &x, &y, &scanned) >= 3) {
			mvprintw(y+1, x*2, "%2s", name);
			//printf("POS %2s %d %d\n", name, x, y);
			total += scanned;
		}
		refresh();
	}
	int ch = getch();
	if (ch == KEY_UP || ch == 'w') 
		send_command("MVU");
	else if (ch == KEY_DOWN || ch=='s') 
		send_command("MVD");
	else if (ch == KEY_LEFT || ch == 'a') 
		send_command("MVL");
	else if (ch == KEY_RIGHT || ch =='d')
		send_command("MVR");
	else if (ch == ' ')
		send_command("FIR");
}



void send_hello() {
	uv_udp_send_t* req = malloc(sizeof(uv_udp_send_t));
	uv_buf_t b = uv_buf_init(buf, 1024);
	b.len = snprintf(buf, 1024, "Hello\n");
	uv_udp_send(req, &client, &b, 1, NULL, on_send_cb);
}

int main(int argc, char* argv[]) {
	if ( argc > 1 ) {
		uv_loop_t* loop = uv_default_loop();
		uv_udp_init(loop, &client);
		struct sockaddr_in addr;
		uv_ip4_addr(argv[1], 6678, &addr);

		uv_udp_connect(&client, (struct sockaddr*)&addr);
		uv_udp_recv_start(&client, alloc_cb, on_recv);
		
		initscr();
		keypad(stdscr, TRUE);
		noecho();
		nodelay(stdscr, TRUE);
		cbreak();
		
		send_hello();

		uv_run(loop, UV_RUN_DEFAULT);
		endwin();
	} else {
		fprintf(stderr, "usage: %s <serveraddr>\n", argv[0]);
		return -1;
	}
}
