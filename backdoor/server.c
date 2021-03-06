#include <pty.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define nil NULL

int ln;

int listentcp(char *);
void pty(int);

void
sighandler(int s)
{
  close(ln);
  exit(0);
}

void
help(char *arg0)
{
  fprintf(stderr, "%s [options]\n"
      "  -p: Port\n", arg0);
  exit(0);
}

void
panic(const char *err)
{
  perror(err);
  exit(1);
}

int
main(int argc, char *argv[])
{
  int n;
  char *port = nil;

  while((n = getopt(argc, argv, "p:")) != -1){
    switch(n){
      case 'p':
        port = optarg;
        break;
    }
  }
  if (port == nil)
    help(argv[0]);

  int conn;
  struct sockaddr_in addr;
  socklen_t addrlen;

  ln = listentcp(port);
  if(ln == -1)
    panic("listentcp()");

  signal(SIGINT, sighandler);

  for(;;){
    conn = accept(ln, (struct sockaddr *)&addr, &addrlen);
    if(conn == -1)
      panic("accept()");
    printf("Client %s connected\n", inet_ntoa(addr.sin_addr));
    pty(conn); // only handle one client at the same time
  }
  close(ln);
}

int
listentcp(char *port)
{
  int n, conn;
  struct addrinfo addr = {
    AI_PASSIVE, AF_INET, SOCK_STREAM, 0,
    0, nil, nil, nil
  };
  struct addrinfo *res, *p;

  n = getaddrinfo(nil, port, &addr, &res);
  if(n == -1)
    return -1;

  for(p = res; p != nil; p = p->ai_next){
    conn = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (conn != -1)
      if (bind(conn, p->ai_addr, p->ai_addrlen) != -1)
        break;
    conn = -1;
  }
  if(conn == -1)
    return -1;

  listen(conn, 1);

  freeaddrinfo(res);
  return conn;
}
  
void
pty(int conn)
{
  int fdm;
  if(forkpty(&fdm, nil, nil, nil) == 0){
    char *arg[] = {nil};
    execvp("/bin/bash", arg);
  } else {
    int n, i;
    int fdi, fdo;
    char buf;
    fd_set fds, readfds;

    FD_ZERO(&fds);
    FD_SET(conn, &fds);
    FD_SET(fdm, &fds);

    readfds = fds;
    for(;;){
      fds = readfds;
      n = select(FD_SETSIZE, &fds, nil, nil, nil);
      if(n == -1)
        break;
      if(n == 0)
        continue;

      if(FD_ISSET(conn, &fds))
        fdi = conn, fdo = fdm;
      else
        fdi = fdm, fdo = conn;

      n = read(fdi, &buf, 1);
      if(n < 1)
        break;
      n = write(fdo, &buf, 1);
      if(n < 1)
        break;
    }
  }
  close(fdm);
  close(conn);
}
