#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

typedef struct s_client
{
	int id;
	int fd;
	char *str;
	struct s_client *next;
}t_client;

t_client	*g_clients = NULL;
int	sock_fd, g_id = 0;
fd_set curr_sock, cpy_read, cpy_write;
char msg[42];
char str[42*4096], tmp[42*4096], buf[42*4096 + 42];


int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void	fatal()
{
	write(2, "Fatal error\n", strlen("Fatal error\n"));
	close(sock_fd);
	exit(1);
}

int	get_id(int fd)
{
	t_client *temp = g_clients;

	while(temp)
	{
		if(temp->fd == fd)
			return (temp->id);
		temp = temp->next;
	}
	return (-1);
}

int	get_max_fd()
{
	int	max = sock_fd;
	t_client	*temp = g_clients;

	while(temp)
	{
		if(temp->fd > max)
			max = temp->fd;
		temp = temp->next;
	}
	return (max);
}

void	send_all(int fd, char *str_req)
{
	t_client	*temp = g_clients;

	while(temp)
	{
		if(temp->fd != fd && FD_ISSET(temp->fd, &cpy_write))
		{
			if(send(temp->fd, str_req, strlen(str_req), 0) < 0)
				fatal();
		}
		temp = temp->next;
	}
}

int	add_client_to_list(int fd)
{
	t_client	*temp = g_clients;
	t_client	*new;

	if (!(new = calloc(1, sizeof(t_client))))
		fatal();
	new->id = g_id++;
	new->fd = fd;
	new->next = NULL;
	if (!g_clients)
		g_clients = new;
	else
	{
		while(temp->next)
			temp = temp->next;
		temp->next = new;
	}
	return (new->id);
}

void	add_client()
{
	int	client_fd;

	if ((client_fd = accept(sock_fd, NULL, NULL)) < 0)
		fatal();
	sprintf(msg, "server: client %d just arrived\n", add_client_to_list(client_fd));
	send_all(client_fd, msg);
	FD_SET(client_fd, &curr_sock);
}

int	rm_client(int fd)
{
	t_client	*temp = g_clients;
	t_client	*del;
	int	id = get_id(fd);

	if (temp && temp->fd == fd)
	{
		g_clients = temp->next;
		free(temp->str);
		free(temp);
	}
	else
	{
		while(temp && temp->next && temp->next->fd != fd)
			temp = temp->next;
		del = temp->next;
		temp->next = temp->next->next;
		free(del->str);
		free(del);
	}
	return (id);
}

void	ex_msg(int fd, char *buf, int n)
{
	char *msg;

	t_client	*temp = g_clients;

	while (temp && temp->fd != fd)
		temp = temp->next;
	
	buf[n] = 0;
	temp->str = str_join(temp->str, buf);

	while (extract_message(&temp->str, &msg))
	{
		sprintf(buf, "client %d: %s", temp->id, msg);
		send_all(fd, buf);
	}
}

int	main(int ac, char **av)
{
	if (ac != 2)
	{
		write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		exit(1);
	}

	struct sockaddr_in servaddr;
	uint16_t port = atoi(av[1]);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
    servaddr.sin_port = htons(port);

    // Binding newly created socket to given IP and verification
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		fatal();
    if (bind(sock_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
		fatal();
    if (listen(sock_fd, 0) < 0)
		fatal();
	
	FD_ZERO(&curr_sock);
	FD_SET(sock_fd, &curr_sock);
	bzero(&tmp, sizeof(tmp));
	bzero(&buf, sizeof(buf));
	bzero(&str, sizeof(str));

	while(1)
	{
		cpy_write = cpy_read = curr_sock;
		if (select(get_max_fd() + 1, &cpy_read, &cpy_write, NULL, NULL) < 0)
			continue;
		for (int fd = 0; fd <= get_max_fd(); fd++)
		{
			if (FD_ISSET(fd, &cpy_read))
			{
				if (fd == sock_fd)
				{
					bzero(&msg, sizeof(msg));
					add_client();
					break ;
				}
				else
				{
					int ret_recv = recv(fd, str, sizeof(str) - 1, 0);
					if (ret_recv <= 0)
					{
						bzero(&msg, sizeof(msg));
						sprintf(msg, "server: client %d just left\n", rm_client(fd));
						send_all(fd, msg);
						FD_CLR(fd, &curr_sock);
						close(fd);
						break ;
					}
					else
						ex_msg(fd, str, ret_recv);
				}
			}
		}
	}
	return (0);
}
