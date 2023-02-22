#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <printf.h>

typedef struct s_client {
	int	id;
	int	fd;
	char	*msg;
	struct s_client *next;
} t_client;

int	g_sockfd = -1;
int	g_id = 0;

t_client	*g_clients = NULL;
fd_set	all_fds, read_fds, write_fds;

int ft_strlen(char *str) {
	if (!str)
		return (0);
	int i = 0;
	while (str[i] != '\0')
		i++;
	return (i);
}

void fatal() {
	write(2, "Fatal error\n", 12);
	exit(1);
}

int max_fd() {
	int res = g_sockfd;
	for (t_client *x = g_clients; x; x = x->next) {
		if (res < x->fd)
			res = x->fd;
	}
	return (res);
}

void say_goodbye(int id) {
	char	buf[64] = {0};
	int	len = sprintf(buf, "server: client %d just left\n", id);
	for (t_client *x = g_clients; x; x = x->next) {
		if (x->id != id && FD_ISSET(x->fd, &write_fds)) {
			if (send(x->fd, buf, len, 0) == -1)
				fatal();
		}
	}
}

void say_hello(int id) {
	char	buf[64] = {0};
	int	len = sprintf(buf, "server: client %d just arrived\n", id);
	for (t_client *x = g_clients; x; x = x->next) {
		if (x->id != id && FD_ISSET(x->fd, &write_fds)) {
			if (send(x->fd, buf, len, 0) == -1)
				fatal();
		}
	}
}

int new_client(int fd) {
	t_client	*new_client = calloc(sizeof(t_client), 1);
	if (!new_client)
		fatal();
	t_client	*tmp = g_clients;

	new_client->id = g_id++;
	new_client->fd = fd;
	new_client->msg = NULL;

	if (!tmp) {
		g_clients = new_client;
		return (new_client->id);
	}
	while (tmp->next)
		tmp = tmp->next;
	tmp->next = new_client;
	return (new_client->id);
}

void accept_client() {
	int fd = accept(g_sockfd, NULL, NULL);
	if (fd == -1)
		fatal();
	FD_SET(fd, &all_fds);
	say_hello(new_client(fd));
}

void close_client(int fd) {
	t_client	*tmp = g_clients;
	for (t_client *x = g_clients; x; x = x->next) {
		if (x->fd != fd) {
			tmp = x;
			continue;
		}
		if (close(fd) == -1)
			fatal();
		FD_CLR(fd, &all_fds);
		say_goodbye(x->id);
		if (g_clients == x)
			g_clients = g_clients->next;
		else
			tmp->next = x->next;
		free(x->msg);
		free(x);
		break;
	}
}

void append_msg(t_client **client, char *buf, int len_buf) {
	char	*ret = NULL;
	int	new_len = len_buf + ft_strlen((*client)->msg);
	ret = malloc(sizeof(char) * (new_len + 1));
	if (!ret)
		fatal();
	ret[new_len] = '\0';
	int i = -1;
	while (++i < ft_strlen((*client)->msg))
		ret[i] = (*client)->msg[i];
	while (i < new_len) {
		ret[i] = buf[i];
		i++;
	}
	free((*client)->msg);
	(*client)->msg = ret;
}

char *format_msg(char *buf, int len, char *add) {
	char	*ret = NULL;
	int total_len = len + ft_strlen(add);
	ret = malloc(sizeof(char) * (total_len + 1));
	if (!ret)
		fatal();
	ret[total_len] = '\0';
	int i = -1;
	while (++i < len)
		ret[i] = buf[i];
	int j = 0;
	while (i < total_len) {
		ret[i] = add[j];
		i++;
		j++;
	}
	return (ret);
}

void send_to_other(t_client **client) {
	char	buf[64] = {0};
	char	*str_to_send = NULL;
	int	len = sprintf(buf, "client %d: ", (*client)->id);
	str_to_send = format_msg(buf, len, (*client)->msg);
	for (t_client *x = g_clients; x; x = x->next) {
		if (x->fd != (*client)->fd && FD_ISSET(x->fd, &write_fds)) {
			if (send(x->fd, str_to_send, ft_strlen(str_to_send), 0) == -1)
				fatal();
		}
	}
	free(str_to_send);
	free((*client)->msg);
	(*client)->msg = NULL;
}

int append_read(char *buf, int fd, int recv_res) {
	t_client	*tmp = g_clients;
	for (t_client *x = g_clients; x; x = x->next) {
		if (x->fd != fd)
			continue;
		tmp = x;
		append_msg(&tmp, buf, recv_res);
		printf("x->msg len = %d\n", ft_strlen(x->msg));
		if (ft_strlen(x->msg) % 4096 > 0)
			send_to_other(&tmp);
		break;
	}
}

void listen_to_clients() {
	int res;
	char	buf[4097] = {0};

	for (t_client *x = g_clients; x; x = x->next) {
		if (!FD_ISSET(x->fd, &read_fds))
			continue;
		printf("fd: %d, ready for reading, already contain in x->msg: %s\n", x->fd, x->msg);
		res = recv(x->fd, buf, 4096, 0);
		if (res < 0)
			fatal();
		buf[res] = '\0';
		printf("buf recv = %s\n", buf);
		if (res == 4096) {
			append_read(buf, x->fd, res);
			break;
		}
		if (!res) {
			if (x->msg) {
				t_client	*tmp = x;
				send_to_other(&tmp);
			}
			close_client(x->fd);
			break;
		}
		printf("recv = %s, len = %d\n", buf, res);
		append_read(buf, x->fd, res);
		break;
	}
}


int main(int argc, char **argv) {
	struct sockaddr_in servaddr;

	if (argc != 2) {
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}
	// socket create and verification 
	g_sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (g_sockfd == -1)
		fatal();
	else
		printf("Socket successfully created..\n"); 
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433);
	servaddr.sin_port = htons(atoi(argv[1])); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(g_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal();
	else
		printf("Socket successfully binded..\n");
	if (listen(g_sockfd, 10) != 0)
		fatal();
	FD_ZERO(&all_fds);
	FD_SET(g_sockfd, &all_fds);
	while (1) {
		read_fds = write_fds = all_fds;
		if (select(max_fd() + 1, &read_fds, &write_fds, NULL, NULL) == -1)
			fatal();
		if (FD_ISSET(g_sockfd, &read_fds)) {
			accept_client();
			continue;
		}
		listen_to_clients();
	}
	return (0);
}
