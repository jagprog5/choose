// sends stdin to tty, and removes trailing newline

#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <stdio.h>

int main() {
  if (access("/dev/tty", W_OK) != 0) {
    perror(NULL);
    return 1;
  }
  int tty_fd = open("/dev/tty", O_WRONLY);
  if (tty_fd == -1) {
    perror(NULL);
    return 1;
  }

  int c;
  bool input_was_newline = false;

  while ((c = getchar()) != EOF) {
    if (input_was_newline) {
      input_was_newline = false;
      char newline = '\n';
      ioctl(tty_fd, TIOCSTI, &newline);
    }


    if (c == '\n') {
      input_was_newline = true;
    } else {
      if (ioctl(tty_fd, TIOCSTI, &c) < 0) {
        perror(NULL);
        return 1;
      }
    }
  }
  return 0;
}
