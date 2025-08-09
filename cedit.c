#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

struct termios orig_termios; // Original termios structure, needed to restore once the user exits the program

void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); // Set attribute as initial termios once the program exits.
}

/**
    By Default, our terminal is in cooked or canonical mode. This means that the string we enter is received by the program only when we press 'Enter'
    In order to run the program on each key-press, we have to turn on the raw mode. This functions enables raw mode.
*/
void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios); // int tcgetattr(int fildes, struct termios *termios_p)
    atexit(disableRawMode); // Call disableRawMode function at program exit (from "stdlib.h")
    /**
        We can set a terminal’s attributes by
            1. using tcgetattr() to read the current attributes into a struct,
            2. modifying the struct by hand, and
            3. passing the modified struct to tcsetattr() to write the new terminal attributes back out.
        All these functions are given by termios.h
     */
    struct termios raw = orig_termios; // struct termios is given by termios.h and represents the terminal structure for C.
    /**
        Ctrl-S stops data from being transmitted to the terminal until you press Ctrl-Q.
        This originates in the days when you might want to pause the transmission of data to let a
        device like a printer catch up. Let’s just turn off that feature.
        IXON is an Input flag (I is for input, ICANON and ISIG are exception).
        XON comes from the names of the two control characters that Ctrl-S and Ctrl-Q produce.
        XOFF to pause transmission and XON to resume transmission.
    */
    raw.c_iflag &= ~(ICRNL | IXON);
    raw.c_oflag &= ~(OPOST);
    /**
        c_lflag is for local flag, similarly c_iflag is for input, c_oflag is for output, c_cflag is for control.
        We can update these flags to change the behavior of terminal.
        ECHO is a bitflag with value 00000000000000000000000000001000.
        Bit flag is basically an integer value that helps to manage multiple flag variables. (Refer - https://pressbooks.lib.jmu.edu/programmingpatterns/chapter/bitflags/)

        To enable raw mode, we need to stop echoing of the variable (just like when we add password in sudo command)
        So we will flip the ECHO bit and add it to c_flag and set the attributes of termios.
        ICANON is the flag for canonical form of terminal. | is bitwise OR and not logical OR.
        ISIG is for SIGINT and SIGSTP commands that are sent when we type Ctrl+C and Ctrl+Z
        IEXTEN to stop special handling of control characters like Ctrl-V (literal next) on Linux and Ctrl-O (discard) on macOS; ensures these keys are read as regular input
    */
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // TCSAFLUSH argument specifies when to apply the change: in this case, it waits for all pending output to be written to the terminal, and also discards any input that hasn’t been read.
}

int main() {
    enableRawMode();

    char c;
    /** Read from Standard input, and exit when q is pressed (this will exit if my string has q in it and the remaining string will be added to terminal buffer) */
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
        if (iscntrl(c)) { //iscntrl is from ctype.h -> Tells if the input is a control key, which should not be printed. ASCII codes 0-31 and 127 are control characters
            printf("%d\r\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }
        }
    return 0;
}