#include "UNIX.H"

char buf[512];

void
wc(fd, name)
int fd;
char *name;
{
  int i, n;
  int l, w, c, inword;

  l = w = c = 0;
  inword = 0;
  while((n = read(fd, buf, sizeof(buf))) > 0){
    for(i=0; i<n; i++){
      c++;
      if(buf[i] == '\n')
        l++;
      /* \013 = VT: C88 has no \v escape (it compiles to the letter v) */
      if(strchr(" \r\t\n\013", buf[i]))
        inword = 0;
      else if(!inword){
        w++;
        inword = 1;
      }
    }
  }
  if(n < 0){
    printf("wc: read error\n");
    exit();
  }
  printf("%d %d %d %s\n", l, w, c, name);
}

int
main(argc, argv)
int argc;
char *argv[];
{
  int fd, i;

  if(argc <= 1){
    wc(0, "");
    exit();
  }

  for(i = 1; i < argc; i++){
    if((fd = open(argv[i], 0)) < 0){
      printf("wc: cannot open %s\n", argv[i]);
      exit();
    }
    wc(fd, argv[i]);
    close(fd);
  }
  exit();
}
