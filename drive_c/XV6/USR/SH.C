#include "UNIX.H"

/* Parsed command representation */
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define O_READ      1
#define O_WRITE     2
#define O_APPEND    3

#define MAXARGS 10

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *subcmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *subcmd;
};

int fork1();  /* Fork but panics on failure. */
void panic();
struct cmd *parsecmd();

/* Execute cmd.  Never returns. */
void
runcmd(cmd)
struct cmd *cmd;
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;
  static char bincmd[25] = "/bin/";

  if(cmd == 0)
    exit();

  switch(cmd->type){
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit();
    if(ecmd->argv[0][0] == '/')
        exec(ecmd->argv[0], ecmd->argv);
    else {
        safestrcpy(&bincmd[5], ecmd->argv[0], 20);
        exec(bincmd, ecmd->argv);
    }
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    switch(rcmd->mode){
      case O_READ: 
        p[0] = open(rcmd->file, 0); 
        break;
      case O_WRITE: 
        p[0] = creat(rcmd->file, 0666); 
        break;
      case O_APPEND: 
        p[0] = open(rcmd->file, 1); 
        if(p[0]>=0) seek(p[0], 0, 2);
        break;
    }
    if(p[0] < 0){
      fprintf(2, "open %s failed, error %d\n", rcmd->file, p[0]);
      exit();
    }
    runcmd(rcmd->subcmd);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait();
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0)
      panic("pipe");
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait();
    wait();
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->subcmd);
    break;
  }
  exit();
}

char *prompt = "$ ";

int
getcmd(buf, nbuf)
char *buf;
int nbuf;
{
  fprintf(2, prompt);
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) /* EOF */
    return -1;
  return 0;
}

char *argv[] = { "ls", 0 };

int main()
{
    static char buf[100];
    
    if(getuid() == 0) prompt[0] = '#';
    while(getcmd(buf, sizeof(buf)) >= 0){
        if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
            /* Chdir must be called by the parent, not the child. */
            buf[strlen(buf)-1] = 0;  /* chop \n */
            if(chdir(buf+3) < 0)
                fprintf(2, "cannot cd %s\n", buf+3);
            continue;
        }
        if(strcmp(buf, "exit\n")==0) {
            exit();
        }
        if(fork1() == 0)
            runcmd(parsecmd(buf));
        wait();
    }
    return 0;
}

void
panic(s)
char *s;
{
  fprintf(2, "%s\n", s);
  exit();
}

int
fork1()
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

struct cmd*
execcmd()
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(subcmd, file, efile, mode, fd)
struct cmd *subcmd;
char *file;
char *efile;
int mode;
int fd;
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->subcmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(left, right)
struct cmd *left;
struct cmd *right;
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
listcmd(left, right)
struct cmd *left;
struct cmd *right;
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
backcmd(subcmd)
struct cmd *subcmd;
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->subcmd = subcmd;
  return (struct cmd*)cmd;
}

/* \013 = VT: C88 has no \v escape -- it compiles to the letter v, which
 * would make the shell split every word containing a v (/dev/fd0). */
char whitespace[] = " \t\r\n\013";
char symbols[] = "<|>&;()";

int
gettoken(ps, es, q, eq)
char **ps;
char *es;
char **q;
char **eq;
{
  char *s;
  int ret;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(ps, es, toks)
char **ps;
char *es;
char *toks;
{
  char *s;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline();
struct cmd *parsepipe();
struct cmd *parseexec();
struct cmd *nulterminate();

struct cmd*
parsecmd(s)
char *s;
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd*
parseline(ps, es)
char **ps;
char *es;
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd*
parsepipe(ps, es)
char **ps;
char *es;
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(cmd, ps, es)
struct cmd *cmd;
char **ps;
char *es;
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_READ, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRITE, 1);
      break;
    case '+':  /* >> */
      cmd = redircmd(cmd, q, eq, O_APPEND, 1);
      break;
    }
  }
  return cmd;
}

struct cmd*
parseblock(ps, es)
char **ps;
char *es;
{
  struct cmd *cmd;

  if(!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd*
parseexec(ps, es)
char **ps;
char *es;
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if(peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

/* NUL-terminate all the counted strings. */
struct cmd*
nulterminate(cmd)
struct cmd *cmd;
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    return 0;

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->subcmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->subcmd);
    break;
  }
  return cmd;
}
