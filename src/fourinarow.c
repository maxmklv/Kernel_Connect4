// Four in a Row kernel module

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h> 
#include <linux/uaccess.h> 
#include <linux/random.h> // for cpu moves
#include <linux/cdev.h> // not used as of now

// prototypes
static ssize_t fourinarow_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t fourinarow_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);

#define DEVICE_NAME "fourinarow"
#define MAX_CMD_LEN 8 // longest command length including null terminator 

#define ROWS 8
#define COLS 8

typedef struct {
    char player_color;      
    char cpu_color;         
    int player_turn;       // 0 - no active game, 1 - player, 2 - CPU
} game_state_t;

static game_state_t game_state = {.player_turn = 0}; // Global variable to hold game state
static int major; // major number assigned to device driver
static char board[ROWS][COLS + 1]; // board global variable so its accesible for read & write
static char user_cmd[MAX_CMD_LEN]; // string to store user command input
static struct class *cls;


static struct file_operations fops = {
    .read = fourinarow_read,
    .write = fourinarow_write,
    .open = NULL,
    .release = NULL
};

// flags used to communicate from write to read
bool display_board = false;
bool reset = false;
bool dropc = false;
bool cturn = false;
bool win = false;
bool loss = false;
bool tie = false;
bool ok = false;
bool nogame = false;
bool outofturn = false;

void init_board(void) { // separate function to be used for launch initialization or called when user asks for reset
    int i, j; // for loop vars
    for (i = 0; i < ROWS; i++) { // initialize the board 
        for (j = 0; j < COLS; j++) {
            board[i][j] = '0';
        }
        board[i][COLS] = '\0';  // Null terminator at the end
    }
}

int char_to_col(char c) {
    if (c >= 'A' && c <= 'H') {
        return c - 'A';  // 'A' -> 0, 'B' -> 1, ..., 'H' -> 7
    }
    return -1;
}

bool drop(char column, char piece) { 
    int col = char_to_col(column);
    int i;
    for (i = 0; i < ROWS; i++) {
        if (board[i][col] == '0') {
            board[i][col] = piece;
            return true;
        }
    }
    return false;
}

bool check_tie(void) {
    int i, j;
    for (i = 0; i < ROWS; i++) { // check if the board is completely filled (no empty spots)
        for (j = 0; j < COLS; j++) {
            if (board[i][j] == '0') {
                return false; // empty spot found, so it's not a tie
            }
        }
    }
    return true; // no empty spots, it's a tie
}

bool check_win(char piece) {
    // check for horizontal wins
    int i, j;
    for (i = 0; i < ROWS; i++) {
        for (j = 0; j < COLS - 3; j++) {
            if (board[i][j] == piece &&
                board[i][j+1] == piece &&
                board[i][j+2] == piece &&
                board[i][j+3] == piece) {
                return true;
            }
        }
    }

    // check for vertical wins
    for (i = 0; i < ROWS - 3; i++) {
        for (j = 0; j < COLS; j++) {
            if (board[i][j] == piece &&
                board[i+1][j] == piece &&
                board[i+2][j] == piece &&
                board[i+3][j] == piece) {
                return true;
            }
        }
    }

    // check diagonal (bottom-left to top-right)
    for (i = 3; i < ROWS; i++) {
        for (j = 0; j < COLS - 3; j++) {
            if (board[i][j] == piece &&
                board[i-1][j+1] == piece &&
                board[i-2][j+2] == piece &&
                board[i-3][j+3] == piece) {
                return true;
            }
        }
    }

    // check diagonal (top-left to bottom-right)
    for (i = 0; i < ROWS - 3; i++) {
        for (j = 0; j < COLS - 3; j++) {
            if (board[i][j] == piece &&
                board[i+1][j+1] == piece &&
                board[i+2][j+2] == piece &&
                board[i+3][j+3] == piece) {
                return true;
            }
        }
    }

    return false;
}


static char *fourinarow_devnode(struct device *dev, umode_t *mode) {
    if (mode) {
        *mode = 0666; // read + write for everyone
    }
    return NULL;
}

static int __init fourinarow_init(void) {
    init_board(); // initialize the board when module is loaded
    major = register_chrdev(0, DEVICE_NAME, &fops); // register device
    
    cls = class_create(THIS_MODULE, DEVICE_NAME); // create class
    cls->devnode = fourinarow_devnode;
    
    device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME); // create device

    pr_info("Hello, Kernel!\n"); // test message
    return 0;
}

static void __exit fourinarow_exit(void) {
    unregister_chrdev(major, DEVICE_NAME); // unregister device

    device_destroy(cls, MKDEV(major, 0)); // destroy device
    class_destroy(cls); // destory class

    pr_info("Goodbye, Kernel!\n"); // test message
}

static ssize_t fourinarow_read(struct file *file, char __user *buf, size_t len, loff_t *off) {
    if (*off > 0) {
        return 0;
    }    
    if (display_board) { // display the board
        int i;
        char board_str[512];
        int offset = 0;

         offset += snprintf(board_str + offset, sizeof(board_str) - offset, "  ABCDEFGH\n"); // Add column labels
        for (i = 7; i >= 0; i--) { // Print from top (row 8 / board[7]) to bottom (row 1 / board[0])
            offset += snprintf(board_str + offset, sizeof(board_str) - offset,
                               "%d %.8s\n", i + 1, board[i]);
                               //pr_info("Row %d: %.8s\n", i + 1, board[i]); // TESTING
        }

        if (copy_to_user(buf, board_str, offset)) { // return error if user input fails to copy into user_cmd
            pr_err("Command: failed to copy board data\n");
            return -EFAULT;
        }
        *off += offset;
        display_board = false;
        return offset;
    }
    if (reset || ok) {
        const char *ok_msg = "OK\n";
        size_t ok_len = strlen(ok_msg);

        if (copy_to_user(buf, ok_msg, ok_len)) {
            pr_err("Command: failed to copy OK response\n");
            return -EFAULT;
        }

        *off += ok_len;
        reset = false;
        return ok_len;
    }

    if (nogame) {
        const char *ng_msg = "NOGAME\n";
        size_t ng_len = strlen(ng_msg);
        
        if (copy_to_user(buf, ng_msg, ng_len)) {
            pr_err("Command: failed to copy NOGAME response\n");
            return -EFAULT;
        }

        *off += ng_len;
        nogame = false;
        return ng_len;
    }

    if (win) {
        const char *win_msg = "WIN\n";
        size_t win_len = strlen(win_msg);
        
        if (copy_to_user(buf, win_msg, win_len)) {
            pr_err("Command: failed to copy WIN response\n");
            return -EFAULT;
        }

        *off += win_len;
        win = false;
        return win_len;
    }

    if (tie) {
        const char *tie_msg = "TIE\n";
        size_t tie_len = strlen(tie_msg);
        
        if (copy_to_user(buf, tie_msg, tie_len)) {
            pr_err("Command: failed to copy TIE response\n");
            return -EFAULT;
        }

        *off += tie_len;
        tie = false;
        return tie_len;
    }

    if (loss) {
        const char *loss_msg = "LOSE\n";
        size_t loss_len = strlen(loss_msg);
        
        if (copy_to_user(buf, loss_msg, loss_len)) {
            pr_err("Command: failed to copy LOSE response\n");
            return -EFAULT;
        }

        *off += loss_len;
        loss = false;
        return loss_len;
    }
    
    if (outofturn) {
        const char *oot_msg = "OOT\n";
        size_t oot_len = strlen(oot_msg);
        
        if (copy_to_user(buf, oot_msg, oot_len)) {
            pr_err("Command: failed to copy OOT response\n");
            return -EFAULT;
        }

        *off += oot_len;
        outofturn = false;
        return oot_len;
    }

    return 0;
}

static ssize_t fourinarow_write(struct file *file, const char __user *buf, size_t len, loff_t *off) {
    if (len > MAX_CMD_LEN) { // make sure command not too long
        pr_err("Invalid command: too long (max %d chars allowed)\n", MAX_CMD_LEN);
        return -EINVAL;
    }


    if (copy_from_user(user_cmd, buf, len)) { // return error if user input fails to copy into user_cmd
        pr_err("Command: failed to copy data from user input\n");
        return -EFAULT;
    }

    user_cmd[len] = '\0'; // null terminate the string
    if (user_cmd[len - 1] == '\n') {
        user_cmd[len - 1] = '\0';
    }
    //pr_info("User command: '%s'\n", user_cmd); // TESTING
    if (strcmp(user_cmd, "BOARD") == 0) {
        display_board = true;
    }
    else if (strncmp(user_cmd, "RESET", 5) == 0) {
        char player;
        if (sscanf(user_cmd, "RESET %c", &player) == 1 && (player == 'R' || player == 'Y')){
            init_board();
            game_state.player_turn = 1; // set to player turn
            game_state.player_color = player;
            if (player == 'R') {
                game_state.cpu_color = 'Y';
            } else {
                game_state.cpu_color = 'R';
            }
            reset = true;
        }
        else {
            pr_err("Invalid color\n");
        }
    }
    else if (strncmp(user_cmd, "DROPC", 5) == 0) {
        char column; 
        if (sscanf(user_cmd, "DROPC %c", &column) == 1 && (column == 'A' || column == 'B' || column == 'C' || column == 'D' || column == 'E' || column == 'F' || column == 'G' || column == 'H')) {
            if (game_state.player_turn == 0) {
                nogame = true;
            }
            else if (game_state.player_turn == 1) {
                if (drop(column, game_state.player_color)) {
                    if (check_win(game_state.player_color)) {
                        game_state.player_turn = 0;
                        win = true;
                    }
                    else if (check_tie()) {
                        game_state.player_turn = 0;
                        tie = true;
                    }
                    else {
                        game_state.player_turn = 2; // set to CPU turn
                        ok = true;
                    }
                }
            
            }
            else if (game_state.player_turn == 2) {
                outofturn = true;
            }
        }
        else {
            pr_err("Invalid column\n");
        }
    }
    else if (strcmp(user_cmd, "CTURN") == 0) {
        if (game_state.player_turn == 0) {
            nogame = true;
        } 
        else if (game_state.player_turn == 1) {
            outofturn = true;
        }
        else if (game_state.player_turn == 2) { //CPU turn
            char col = 'A' + (get_random_int() % 8);
            if (drop(col, game_state.cpu_color)) {
                if (check_win(game_state.cpu_color)) {
                    game_state.player_turn = 0;
                    loss = true;
                }
                else if (check_tie()) {
                    game_state.player_turn = 0;
                    tie = true;
                }
                else {
                    game_state.player_turn = 1; // set to player turn
                    ok = true;
                }
            }
        }
    }
    else {
        pr_err("Invalid command\n");
    }

    return len;
}

module_init(fourinarow_init);
module_exit(fourinarow_exit);

MODULE_LICENSE("GPL");
