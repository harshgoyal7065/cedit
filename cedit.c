#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

#define CTRL_KEY(k) ((k) & 0x1f)

struct termios orig_termios; // Original termios structure, needed to restore once the user exits the program

/** Print error message and exit */
void die(const char *s) {
    perror(s);
    exit(1);
}

void disableRawMode() {
    // Set attribute as initial termios once the program exits.
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

/**
    By Default, our terminal is in cooked or canonical mode. This means that the string we enter is received by the program only when we press 'Enter'
    In order to run the program on each key-press, we have to turn on the raw mode. This functions enables raw mode.
*/
void enableRawMode() {
    // int tcgetattr(int fildes, struct termios *termios_p)
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
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
        When BRKINT is turned on, a break condition will cause a SIGINT signal to be sent to the program, like pressing Ctrl-C.
        INPCK enables parity checking, which doesn’t seem to apply to modern terminal emulators.
        ISTRIP causes the 8th bit of each input byte to be stripped, meaning it will set it to 0. This is probably already turned off.
    */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    /**
        CS8 is not a flag, it is a bit mask with multiple bits, which we set using the bitwise-OR (|) operator unlike all the flags we are turning off.
        It sets the character size (CS) to 8 bits per byte.
     */
    raw.c_cflag |= (CS8);

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
    /**
        VMIN and VTIME come from <termios.h>.
        They are indexes into the c_cc field, which stands for “control characters”, an array of bytes that control various terminal settings.
        The VMIN value sets the minimum number of bytes of input needed before read() can return. We set it to 0 so that read() returns
        as soon as there is any input to be read.
        The VTIME value sets the maximum amount of time to wait before read() returns. It is in tenths of a second, so we set it to 1/10 of a second, or 100 milliseconds.
        If read() times out, it will return 0, which makes sense because its usual return value is the number of bytes read.
    */
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr"); // TCSAFLUSH argument specifies when to apply the change: in this case, it waits for all pending output to be written to the terminal, and also discards any input that hasn’t been read.
}

char editorReadKey() {
    int nread;
    char c;
    /** Read from Standard input */
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

void editorProcessKeypress() {
    char c = editorReadKey();
    switch (c) {
        case CTRL_KEY('q'):
        exit(0);
        break;
    }
}

int main() {
    enableRawMode();

    while (1) {
        editorProcessKeypress();
    }

    // while (1) {
    //     char c = '\0';
    //     /** Read from Standard input, and exit when q is pressed (this will exit if my string has q in it and the remaining string will be added to terminal buffer) */
    //     if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
    //     if (iscntrl(c)) { //iscntrl is from ctype.h -> Tells if the input is a control key, which should not be printed. ASCII codes 0-31 and 127 are control characters
    //         printf("%d\r\n", c);
    //     } else {
    //         printf("%d ('%c')\r\n", c, c);
    //     }
    //     if (c == CTRL_KEY('q')) break;
    // }
    return 0;
}