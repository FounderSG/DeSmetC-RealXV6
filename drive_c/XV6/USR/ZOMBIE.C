#include "UNIX.H"

int main()
{
  if(fork() > 0)
    sleep(5); /* Let child exit before parent. */
  exit();
}
