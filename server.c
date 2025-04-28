#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h> 

#define MAX_CLIENTS 128

#define WIDTH 50
#define HEIGHT 25


typedef struct client {
	int x;
	int y;
	int vx;
	int vy;
	struct sockaddr_in address;
	int active;
	time_t last_event;
	char name[2];
} Client;




Client clients[MAX_CLIENTS];
int nclients = 0;

uv_udp_t server;

const char* namechars = "abcdefghigklmnopqrstuvwxyzABCDEFGHIGKLMNOPQRSTUVWXYZ0123456789";


void client_move(Client* c, int dx, int dy) {
	c->vx += dx;
	c->vy += dy;
	c->vx < -3?-3:c->vx;
	c->vx > 3?3:c->vx;
	c->vy < -3?-3:c->vy;
	c->vy > 3?3:c->vy;
}

void client_fire(Client* c) {
	// TODO: make a bullet and run in moving direction
}

void send_cb(uv_udp_send_t* req, int status) {
	free(req->data);
}

void client_send_name(Client* c) {
	uv_udp_send_t* req = malloc(sizeof(uv_udp_send_t));
	uv_buf_t buf = uv_buf_init(malloc(1024), 1024);
	buf.len = snprintf(buf.base, 1024, "NAME %2s\n", c->name);
	uv_udp_send(req, &server, &buf, 1, &c->address, send_cb);
	req->data = buf.base;
}

void client_send_noroom(struct sockaddr_in* addr) {
	uv_udp_send_t* req = malloc(sizeof(uv_udp_send_t));
	uv_buf_t buf = uv_buf_init(malloc(1024), 1024);
	buf.len = snprintf(buf.base, 1024, "NOROOM");
	uv_udp_send(req, &server, &buf, 1, addr, send_cb);
	req->data = buf.base;
}

void process_input(Client* c, const uv_buf_t* buf) {
	fprintf(stderr, "Got command %3s from %2s\n", buf->base, c->name);
	if ( !strncmp(buf->base, "MVL", 3) ) client_move(c, -1, 0);
	else if ( !strncmp(buf->base, "MVR", 3)) client_move(c, 1, 0);
	else if ( !strncmp(buf->base, "MVU", 3)) client_move(c, 0, -1);
	else if ( !strncmp(buf->base, "MVD", 3)) client_move(c, 0, 1);
	else if ( !strncmp(buf->base, "FIR", 3)) client_fire(c);
}

Client* find_by_addr(const struct sockaddr_in* addr) {
	int i;
	for(i = 0; i < MAX_CLIENTS; i++ ) {
		if (clients[i].active 
				&& !memcmp(&clients[i].address, addr, sizeof(struct sockaddr_in))) {
			return &clients[i];
		}
	}
	return NULL;
}


static void on_socket_recv(uv_udp_t *handle, 
		ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr,
		unsigned flags) {
	if ( nread > 0 ) {
		Client* c = find_by_addr(addr);
		if ( c ) {
			process_input(c, buf);
		} else {
			for (int i = 0; i < MAX_CLIENTS; i++ ) {
				if ( !clients[i].active) {
					clients[i].address = *(const struct sockaddr_in*)addr;
					clients[i].name[0] = namechars[rand()%strlen(namechars)];
					clients[i].name[1] = namechars[rand()%strlen(namechars)];
					printf("Client connected %2s\n", clients[i].name);
					clients[i].active = 1;
					client_send_name(&clients[i]);
					free(buf->base);
					return;
				}
			}
			client_send_noroom(addr);
		}
	}
	free(buf->base);

}


static void on_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

void reflect(Client* c ) {
	if ( c->x < 0 && c->vx < 0 )
		c->vx = -c->vx, c->x += c->vx;
	if ( c->y < 0 && c->vy < 0 )
		c->vy = -c->vy, c->y += c->vy;
	if ( c->x >= WIDTH && c->vx > 0 )
		c->vx = -c->vx, c->x += c->vx;
	if ( c->y >= HEIGHT && c->vy > 0 )
		c->vy = -c->vy, c->y += c->vy;
}


void calc_coords() {
	for (int i = 0; i < MAX_CLIENTS; i++ ) {
		if (clients[i].active) {
			clients[i].x += clients[i].vx;
			clients[i].y += clients[i].vy;
			reflect(&clients[i]);
		}
	}

}

void send_current_state(struct sockaddr_in* addr) {
	uv_buf_t buf;
	FILE* f = open_memstream(&buf.base, &buf.len);
	for (Client* c = clients; c < clients+MAX_CLIENTS; c++ ) {
		if (c->active)
			fprintf(f, "POS %.2s %d %d\n", c->name, c->x, c->y);
	}
	uv_udp_send_t* req = malloc(sizeof(uv_udp_send_t));
	fclose(f);
	req->data = buf.base;
	uv_udp_send(req, &server, &buf, 1, addr, send_cb);
}

void broadcast_state() {
	fprintf(stderr, "Boradcasting state\n");
	for (int i =0; i < MAX_CLIENTS; i++ ) {
		if (clients[i].active) {
			fprintf(stderr, "Sending to %2s\n", clients[i].name);
			send_current_state(&clients[i].address);
		}
	}
}


void on_periodic(uv_timer_t* timer) {
	calc_coords();
	broadcast_state();
}

int main() {
	uv_loop_t* loop = uv_default_loop();

	uv_udp_init(loop, &server);
	uv_timer_t periodic;

	uv_timer_init(loop, &periodic);
	
	struct sockaddr_in addr;
	uv_ip4_addr("0.0.0.0", 6678, &addr);

	uv_udp_bind(&server, (const struct sockaddr*)&addr, 0);

	uv_udp_recv_start(&server, on_alloc, on_socket_recv);

	memset(clients, 0, sizeof(clients));

	uv_timer_start(&periodic, on_periodic, 1000, 100);

	uv_run(loop, UV_RUN_DEFAULT);

}
