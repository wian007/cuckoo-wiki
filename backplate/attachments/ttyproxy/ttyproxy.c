// ttyproxy.c — PTY <-> /dev/ttyO2.real bridge with hex logging
// Build: ${CROSS_COMPILE}gcc -O2 -o ttyproxy ttyproxy.c
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

static int fd_master=-1, fd_real=-1;
static FILE *logf=NULL;

static const char *real_path="/dev/ttyO2.real";
static const char *log_path="/var/log/nl_intercept.log";
#ifndef B115200
#define B115200 B9600
#endif

// fallback cfmakeraw if not provided by libc
static void make_raw(struct termios *t) {
#ifdef __GLIBC__
  cfmakeraw(t);
#else
  // portable raw-ish settings
  t->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
  t->c_oflag &= ~(OPOST);
  t->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
  t->c_cflag &= ~(CSIZE|PARENB);
  t->c_cflag |= CS8;
#endif
}

static const char* ts(void){
  static char buf[32];
  time_t t=time(NULL);
  struct tm tm; localtime_r(&t,&tm);
  strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M:%S",&tm);
  return buf;
}

static void hexdump_dir(const char *dir, const uint8_t *p, ssize_t n){
  if(!logf) return;
  fprintf(logf, "%s %s ", ts(), dir);
  for(ssize_t i=0;i<n;i++) fprintf(logf, "%02x", p[i]);
  fputc('\n', logf); fflush(logf);
}

static void set_raw_speed(int fd, speed_t baud){
  struct termios tio;
  if(tcgetattr(fd,&tio)==-1) return;
  make_raw(&tio);
  cfsetispeed(&tio, baud);
  cfsetospeed(&tio, baud);
  tcsetattr(fd,TCSANOW,&tio);
}

static void cleanup(int signo){
  (void)signo;
  if(logf){ fprintf(logf,"%s STOP\n", ts()); fclose(logf); }
  if(fd_master!=-1) close(fd_master);
  if(fd_real!=-1) close(fd_real);
  _exit(0);
}

static speed_t parse_baud(long b){
  switch(b){
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: default: return B115200;
  }
}

int main(int argc, char **argv){
  const char *rpath = real_path;
  const char *lpath = log_path;
  speed_t baud = B115200;

  for(int i=1;i<argc;i++){
    if(!strcmp(argv[i],"-r") && i+1<argc) rpath=argv[++i];
    else if(!strcmp(argv[i],"-L") && i+1<argc) lpath=argv[++i];
    else if(!strcmp(argv[i],"-b") && i+1<argc) baud=parse_baud(strtol(argv[++i],NULL,10));
    else if(!strcmp(argv[i],"-h")){
      fprintf(stderr,"Usage: %s [-r /dev/ttyO2.real] [-b 115200] [-L logfile]\n", argv[0]);
      return 1;
    }
  }

  // allocate PTY
  fd_master = posix_openpt(O_RDWR|O_NOCTTY);
  if(fd_master==-1){ perror("posix_openpt"); return 1; }
  if(grantpt(fd_master)==-1 || unlockpt(fd_master)==-1){ perror("grant/unlock pt"); return 1; }
  char *slave_name = ptsname(fd_master);
  if(!slave_name){ perror("ptsname"); return 1; }

  // open real device
  fd_real = open(rpath, O_RDWR|O_NOCTTY);
  if(fd_real==-1){ perror(rpath); return 1; }

  // set raw + speed
  set_raw_speed(fd_master, baud);
  set_raw_speed(fd_real,   baud);

  // non-blocking
  int flags;
  flags=fcntl(fd_master,F_GETFL); fcntl(fd_master,F_SETFL, flags|O_NONBLOCK);
  flags=fcntl(fd_real,  F_GETFL); fcntl(fd_real,  F_SETFL, flags|O_NONBLOCK);

  // open log (best-effort)
  logf = fopen(lpath, "a");

  if(logf){ fprintf(logf,"%s PROXY START %s <-> %s\n", ts(), slave_name, rpath); fflush(logf); }

  // print the PTY SLAVE path on stdout
  printf("SLAVE=%s\n", slave_name);
  fprintf(stderr, "[ttyproxy] Bind-mount onto /dev/ttyO2:  mount --bind %s /dev/ttyO2\n", slave_name);

  signal(SIGINT, cleanup);
  signal(SIGTERM, cleanup);

  uint8_t buf[4096];
  while(1){
    fd_set rd; FD_ZERO(&rd);
    FD_SET(fd_master,&rd); FD_SET(fd_real,&rd);
    int nf = (fd_master>fd_real?fd_master:fd_real)+1;
    int rc = select(nf, &rd, NULL, NULL, NULL);
    if(rc<0){
      if(errno==EINTR) continue;
      perror("select"); break;
    }
    if(FD_ISSET(fd_master,&rd)){
      ssize_t n = read(fd_master, buf, sizeof(buf));
      if(n<=0) break;
      hexdump_dir("NLCLIENT->REAL", buf, n);
      (void)write(fd_real, buf, n);
    }
    if(FD_ISSET(fd_real,&rd)){
      ssize_t n = read(fd_real, buf, sizeof(buf));
      if(n<=0) break;
      hexdump_dir("REAL->NLCLIENT", buf, n);
      (void)write(fd_master, buf, n);
    }
  }
  cleanup(0);
  return 0;
}
