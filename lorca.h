#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <pthread.h>

#ifndef SERV_H
#define SERV_H

#define MAX_CONNECTION 100
#define RESP_BUF_MAX 1000

int recieve(char url[]);

static int serv_init(int port, int *listenfd) {
	char port_s[16];
	snprintf(port_s, 16, "%u", port);

	printf("Starting on http://127.0.0.1:%s\n", port_s);

	struct addrinfo hints, *res;

	// Pre set to empty connections
	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, port_s, &hints, &res)) {
		perror("getaddrinfo() failed\n");
		return 1;
	}

	for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
		int option = 1;
	
		*listenfd = socket(p->ai_family, p->ai_socktype, 0);
		setsockopt(*listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
		if (*listenfd == -1) {
			continue;
		}

		if (!bind(*listenfd, p->ai_addr, p->ai_addrlen)) {
			break;
		}
	}

	freeaddrinfo(res);

	if (listen(*listenfd, MAX_CONNECTION)) {
		perror("listen() error");
		return 1;
	}
}

//client connection
static int respond(int n, int clients[MAX_CONNECTION]) {
	char *buf = malloc(RESP_BUF_MAX);
	int rcvd = recv(clients[n], buf, RESP_BUF_MAX, 0);

	if (rcvd < 0) {
		perror("recv() error\n");
		return 1;
	} else if (rcvd == 0) {
		perror("Client disconnected upexpectedly\n");
		return 1;
	}

	// Quickly filter parameters from response dump
	char *url = strtok(buf, "\n");
	while (url != NULL) {
		if (!strncmp(url, "GET ", 4)) {
			url = strtok(url + 4, " ");
			break;
		}

		url = strtok(NULL, "\n");
	}

	// Route file descriptor into stdout
	dup2(clients[n], STDOUT_FILENO);
	close(clients[n]);

	recieve(url);

	fflush(stdout);
	shutdown(STDOUT_FILENO, SHUT_WR);
	close(STDOUT_FILENO);

	clients[n] = -1;
	return 0;
}

static int serv_start(int port) {
	int clients[MAX_CONNECTION];
	memset(clients, -1, sizeof(int) * MAX_CONNECTION);

	struct sockaddr_in clientaddr;
	int listenfd;

	serv_init(port, &listenfd);

	// Ignore SIGCHLD to avoid zombie threads
	signal(SIGCHLD, SIG_IGN);

	while (1) {
		int slot = 0;

		socklen_t addrlen = sizeof(clientaddr);
		clients[slot] = accept(listenfd, (struct sockaddr *) &clientaddr, &addrlen);

		if (clients[slot] < 0) {
			perror("accept() error");
		} else {
			if (fork() == 0) {
				respond(slot, clients);
				break;
			}
		}

		while (clients[slot]!=-1) {
			slot = (slot + 1) % MAX_CONNECTION;
		}
	}

	return 0;
}
#endif

#ifndef LORCA_H
#define LORCA_H

#define MAX_CHROME_ARGS 1024
#define LORCA_TMP "/tmp/lorca"
#define LORCA_PORT 1234

static char *chrome_args =
	" --disable-background-networking"
	" --disable-background-timer-throttling"
	" --disable-backgrounding-occluded-windows"
	" --disable-breakpad"
	" --disable-client-side-phishing-detection"
	" --disable-default-apps"
	" --disable-dev-shm-usage"
	" --disable-infobars"
	" --disable-extensions"
	" --disable-features=site-per-process"
	" --disable-hang-monitor"
	" --disable-ipc-flooding-protection"
	" --disable-popup-blocking"
	" --disable-prompt-on-repost"
	" --disable-renderer-backgrounding"
	" --disable-sync"
	" --disable-translate"
	" --disable-windows10-custom-titlebar"
	" --metrics-recording-only"
	" --no-first-run"
	" --no-default-browser-check"
	" --safebrowsing-disable-auto-update"
	" --enable-automation"
	;

static void findChrome(char buf[], int limit) {
	strncpy(buf, "/usr/bin/google-chrome-stable", limit);
}

int recieve(char url[]) {
	printf("HTTP/1.1 200 OK\rContent-type: text/html\n\r\n");
	printf("<title>Basic WebView</title>You are on URL, [%s]\n", url);
}

void *server_setup() {
	serv_start(LORCA_PORT);
	pthread_exit(0);
}

int lorca_new() {
	pthread_t id;
	pthread_create(&id, NULL, server_setup, NULL);

	char cmd[64 + sizeof(chrome_args) + MAX_CHROME_ARGS];
	findChrome(cmd, sizeof(cmd));
	strcat(cmd, chrome_args);

	char buf[128];
	snprintf(buf, sizeof(buf), " --window-size=%u,%u", 400, 400);
	strcat(cmd, buf);

	mkdir(LORCA_TMP, 700);

	snprintf(buf, sizeof(buf), " --usr-data-dir=%s", LORCA_TMP);
	strcat(cmd, buf);

	snprintf(buf, sizeof(buf), " --app=http://127.0.0.1:%u", LORCA_PORT);
	strcat(cmd, buf);

	system(cmd);
	pthread_cancel(id);
}

#endif

int main() {
	lorca_new();
}
