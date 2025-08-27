/*** includes ***/
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>

/** defines */
#define CEDIT_VERSION "0.0.0"
#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey
{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DEL_KEY
};

/** data */

/**
 * erow stands for “editor row”, and stores a line of text as a pointer to the dynamically-allocated character data and a length.
 * The typedef lets us refer to the type as erow instead of struct erow.
 */
typedef struct erow
{
    int size;
    char *chars;
} erow;

struct editorConfig
{
    int cx, cy;
    int screen_rows;
    int screen_cols;
    int num_rows;
    erow row;
    struct termios orig_termios;
};
struct editorConfig E;

struct termios orig_termios; // Original termios structure, needed to restore once the user exits the program

/** Print error message and exit */
void die(const char *s)
{
    /**
     * Clear the screen and reposition the cursor when our program exits.
     * If an error occurs in the middle of rendering the screen, we don’t want a bunch of garbage left over on the screen
     */
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disableRawMode()
{
    // Set attribute as initial termios once the program exits.
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

/**
    By Default, our terminal is in cooked or canonical mode. This means that the string we enter is received by the program only when we press 'Enter'
    In order to run the program on each key-press, we have to turn on the raw mode. This functions enables raw mode.
*/
void enableRawMode()
{
    // int tcgetattr(int fildes, struct termios *termios_p)
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");
    atexit(disableRawMode); // Call disableRawMode function at program exit (from "stdlib.h")
    /**
        We can set a terminal’s attributes by
            1. using tcgetattr() to read the current attributes into a struct,
            2. modifying the struct by hand, and
            3. passing the modified struct to tcsetattr() to write the new terminal attributes back out.
        All these functions are given by termios.h
     */
    struct termios raw = E.orig_termios; // struct termios is given by termios.h and represents the terminal structure for C.

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
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr"); // TCSAFLUSH argument specifies when to apply the change: in this case, it waits for all pending output to be written to the terminal, and also discards any input that hasn’t been read.
}

int editorReadKey()
{
    int nread;
    char c;
    /** Read from Standard input */
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }
    /**
     * Pressing an arrow key sends multiple bytes as input to our program.
     * These bytes are in the form of an escape sequence that starts with '\x1b', '[', followed by an 'A', 'B', 'C', or 'D'
     * depending on which of the four arrow keys was pressed.
     */
    if (c == '\x1b')
    {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';
        if (seq[0] == '[')
        {
            /**
             * Handling escape sequence of page up and page down
             * The Home key could be sent as <esc>[1~, <esc>[7~, <esc>[H, or <esc>OH. Similarly, the End key could be sent as <esc>[4~, <esc>[8~, <esc>[F, or <esc>OF
             */
            if (seq[1] >= '0' && seq[1] <= '9')
            {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return '\x1b';
                if (seq[2] == '~')
                {
                    switch (seq[1])
                    {
                    case '1':
                        return HOME_KEY;
                    case '3':
                        return DEL_KEY;
                    case '4':
                        return END_KEY;
                    case '5':
                        return PAGE_UP;
                    case '6':
                        return PAGE_DOWN;
                    case '7':
                        return HOME_KEY;
                    case '8':
                        return END_KEY;
                    }
                }
            }
            else
            {
                switch (seq[1])
                {
                case 'A':
                    return ARROW_UP;
                case 'B':
                    return ARROW_DOWN;
                case 'C':
                    return ARROW_RIGHT;
                case 'D':
                    return ARROW_LEFT;
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
                }
            }
        }
        else if (seq[0] == 'O')
        {
            switch (seq[1])
            {
            case 'H':
                return HOME_KEY;
            case 'F':
                return END_KEY;
            }
            return '\x1b';
        }
    }
    else
    {
        return c;
    }
}

/**
 * Get Current Cursor position
 */
int getCursorPosition(int *rows, int *cols)
{
    char buf[32];
    unsigned int i = 0;
    /**
     * The n command (Device Status Report) can be used to query the terminal for status information.
     * We want to give it an argument of 6 to ask for the cursor position.
     * The reply is an escape sequence! It’s an escape character (27),
     * followed by a [ character, and then the actual response: 24;80R, or similar. (This escape sequence is documented as Cursor Position Report.)
     * Cursor Position Report - https://vt100.net/docs/vt100-ug/chapter3.html#CPR
     */
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;

    /**
     * When we print out the buffer, we don’t want to print the '\x1b' character, because the terminal would interpret it as an escape sequence
     * and wouldn’t display it. So we skip the first character in buf by passing &buf[1] to printf().
     * printf() expects strings to end with a 0 byte, so we make sure to assign '\0' to the final byte of buf.
     */
    while (i < sizeof(buf) - 1)
    {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';
    // printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);
    // editorReadKey();
    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;
    return -1;
}

int getWindowSize(int *rows, int *cols)
{
    /**
     * ioctl(), TIOCGWINSZ, and struct winsize come from <sys/ioctl.h>.
     * On success, ioctl() will place the number of columns wide and the number of rows high the terminal
     * is into the given winsize struct. On failure, ioctl() returns -1.
     */
    struct winsize ws;
    /**
     * There is no simple “move the cursor to the bottom-right corner” command
     */
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        /**
         * We are sending two escape sequences one after the other.
         * The C command (Cursor Forward) moves the cursor to the right, and the B command (Cursor Down) moves the cursor down.
         * The argument says how much to move it right or down by. We use a very large value, 999, which should ensure that the
         * cursor reaches the right and bottom edges of the screen.
         * The C and B commands are specifically documented to stop the cursor from going past the edge of the screen.
         * The reason we don’t use the <esc>[999;999H command is that the documentation doesn’t specify what happens when
         *  you try to move the cursor off-screen.
         */
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;
        editorReadKey();
        return -1;
    }
    else
    {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/**
 * editorOpen() will eventually be for opening and reading a file from disk, so we put it in a new file i/o section.
 * To load our “Hello, world” message into the editor’s erow struct, we set the size field to the length of our message, malloc() the necessary memory,
 * and memcpy() the message to the chars field which points to the memory we allocated. Finally, we set the E.numrows variable to 1,
 * to indicate that the erow now contains a line that should be displayed.
 */

void editorOpen(char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
        die("fopen");
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    linelen = getline(&line, &linecap, fp);
    if (linelen != -1)
    {
        while (linelen > 0 && (line[linelen - 1] == '\n' ||
                               line[linelen - 1] == '\r'))
            linelen--;
        E.row.size = linelen;
        E.row.chars = malloc(linelen + 1);
        memcpy(E.row.chars, line, linelen);
        E.row.chars[linelen] = '\0';
        E.num_rows = 1;
    }
    free(line);
    fclose(fp);
}
/**
 * We want to replace all our write() calls with code that appends the string to a buffer,
 * and then write() this buffer out at the end. Unfortunately, C doesn’t have dynamic strings, so we’ll create our own dynamic string
 * type that supports one operation: appending.
 * An append buffer consists of a pointer to our buffer in memory,
 * and a length. We define an ABUF_INIT constant which represents an empty buffer. This acts as a constructor for our abuf type.
 */
struct abuf
{
    char *b;
    int len;
};
#define ABUF_INIT {NULL, 0}

/**
 * realloc() and free() come from <stdlib.h>. memcpy() comes from <string.h>
 * To append a string s to an abuf, the first thing we do is make sure we allocate enough memory to hold the new string.
 * We ask realloc() to give us a block of memory that is the size of the current string plus the size of the string we are appending.
 * realloc() will either extend the size of the block of memory we already have allocated, or it will take care of free()ing the
 * current block of memory and allocating a new block of memory somewhere else that is big enough for our new string.
 * Then we use memcpy() to copy the string s after the end of the current data in the buffer,
 * and we update the pointer and length of the abuf to the new values.
 */
void abAppend(struct abuf *ab, const char *s, int len)
{
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL)
        return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

/**
 * abFree() is a destructor that deallocates the dynamic memory used by an abuf.
 */
void abFree(struct abuf *ab)
{
    free(ab->b);
}

void editorMoveCursor(int key)
{
    switch (key)
    {
    case ARROW_LEFT:
        if (E.cx != 0)
        {
            E.cx--;
        }
        break;
    case ARROW_RIGHT:
        if (E.cx != E.screen_cols - 1)
        {
            E.cx++;
        }
        break;
    case ARROW_UP:
        if (E.cy != 0)
        {
            E.cy--;
        }
        break;
    case ARROW_DOWN:
        if (E.cy != E.screen_rows - 1)
        {
            E.cy++;
        }
        break;
    }
}
void editorProcessKeypress()
{
    int c = editorReadKey();
    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;

    case PAGE_UP:
    case PAGE_DOWN:
    { // We create a code block with that pair of braces so that we’re allowed to declare the times variable. (You can’t declare variables directly inside a switch statement.)
        int times = E.screen_rows;
        while (times--)
            editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    }
    break;

    case HOME_KEY:
        E.cx = 0;
        break;
    case END_KEY:
        E.cx = E.screen_cols - 1;
        break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
    }
}

/** Function to draw a screen of tilde */
void editorDrawRows(struct abuf *ab)
{
    int y;
    for (y = 0; y < E.screen_rows; y++)
    {
        if (y >= E.num_rows)
        {

            if (y == E.screen_rows / 3)
            {
                char welcome[80];
                int welcome_len = snprintf(welcome, sizeof(welcome),
                                           "Cedit editor -- version %s", CEDIT_VERSION);
                if (welcome_len > E.screen_cols)
                    welcome_len = E.screen_cols;
                /**
                 * To center a string, you divide the screen width by 2,
                 * and then subtract half of the string’s length from that. In other words:
                 * E.screen_cols/2 - welcome_len/2, which simplifies to (E.screen_cols - welcome_len) / 2.
                 * That tells you how far from the left edge of the screen you should start printing the string.
                 * So we fill that space with space characters, except for the first character, which should be a tilde.
                 */
                int padding = (E.screen_cols - welcome_len) / 2;
                if (padding)
                {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while (padding--)
                    abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcome_len);
            }
            else
            {
                abAppend(ab, "~", 1);
            }
        }
        else
        {
            int len = E.row.size;
            if (len > E.screen_cols)
                len = E.screen_cols;
            abAppend(ab, E.row.chars, len);
        }
        abAppend(ab, "\x1b[K", 3);
        if (y < E.screen_rows - 1)
        {
            abAppend(ab, "\r\n", 2);
        }
    }
}

/** Function to Refresh the screen */
void editorRefreshScreen()
{
    struct abuf ab = ABUF_INIT;
    /**
     * write() and STDOUT_FILENO come from <unistd.h>.
     * 4 in our write() call means we are writing 4 bytes out to the terminal. The first byte is \x1b, which is the escape character, or 27 in decimal.
     * The other three bytes are [2J
     * We are writing an escape sequence to the terminal. Escape sequences always start with an escape character (27) followed by a [ character.
     * We are using the J command (Erase In Display) to clear the screen
     * H command (Cursor Position) to position the cursor.
     * We are using VT100 escape sequence guide - https://vt100.net/docs/vt100-ug/chapter3.html
     */
    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    /**
     * We changed the old H command into an H command with arguments, specifying the exact position we want the cursor to move to.
     * We add 1 to E.cy and E.cx to convert from 0-indexed values to the 1-indexed values that the terminal uses.
     * Now, we’ll allow the user to move the cursor using the wasd keys. (If you’re unfamiliar with using these keys as arrow keys: w is your up arrow, s is your down arrow, a is left, d is right.)
     * **/
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    /**
     * We use escape sequences to tell the terminal to hide and show the cursor.
     * The h and l commands (Set Mode, Reset Mode) are used to turn on and turn off various terminal features or “modes”.
     * The VT100 User Guide just linked to doesn’t document argument ?25 which we use above.
     * It appears the cursor hiding/showing feature appeared in later VT models.
     */
    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/**
 * Function to initialize Editor
 */
void initEditor()
{
    E.cx = 0;
    E.cy = 0;
    E.num_rows = 0;

    if (getWindowSize(&E.screen_rows, &E.screen_cols) == -1)
        die("getWindowSize");
}

int main(int argc, char *argv[])
{
    enableRawMode();
    initEditor();
    if (argc >= 2)
    {
        editorOpen(argv[1]);
    }

    while (1)
    {
        editorRefreshScreen();
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