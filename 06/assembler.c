#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Utility function; so we can abbreviate
// if (thing == NULL) {
//     printf("something happened :(");
//     return 1;
// }
// to
// terminate_if(thing == NULL, "something happened :(");
void terminate_if(int x, char* msg)
{
    if (x) {
        printf("%s\n", msg);
        exit(EXIT_FAILURE);
    }
    // No need to worry about freeing allocated resources
    // or closing files, since the OS will do that for us at the end (hopefully?)
}

// writebin(num, bits, buf)
// writes `num' in binary in `buf' using exactly `bits' bits,
// assuming `buf' is big enough to hold it, including the null terminator (requires size bits+1).
// e.g. writebin(11, 10, bin)  -->  bin = "0000001011"
void writebin(unsigned num, int bits, char* buf)
{
    for (int i = bits-1; i >= 0; i--) {
        buf[i] = (num & 1) + '0';
        num >>= 1;
    }
    buf[bits] = '\0';
}

//
// Symbol table data type for C-instructions;
// maps assembly instruction segments to their binary counterparts
//

typedef struct {
    size_t size;
    struct {
        char* key;
        char* val;
    } entries[];
} ctab;

char* clookup(ctab* tab, char* key) 
{
    for (int i = 0; i < tab->size; i++)
        if (strcmp(key, tab->entries[i].key) == 0)
            return tab->entries[i].val;
    return NULL;
}

ctab comptab = {
    .size = 28,
    .entries = {
        { "0",   "0101010" }, { "1",   "0111111" }, { "-1",  "0111010" }, { "D",   "0001100" },
        { "A",   "0110000" }, { "M",   "1110000" }, { "!D",  "0001101" }, { "!A",  "0110001" },
        { "!M",  "1110001" }, { "-D",  "0001111" }, { "-A",  "0110011" }, { "-M",  "1110011" },
        { "D+1", "0011111" }, { "A+1", "0110111" }, { "M+1", "1110111" }, { "D-1", "0001110" },
        { "A-1", "0110010" }, { "M-1", "1110010" }, { "D+A", "0000010" }, { "D+M", "1000010" },
        { "D-A", "0010011" }, { "D-M", "1010011" }, { "A-D", "0000111" }, { "M-D", "1000111" },
        { "D&A", "0000000" }, { "D&M", "1000000" }, { "D|A", "0010101" }, { "D|M", "1010101" },
    }
};

ctab desttab = {
    .size = 8,
    .entries = {
        { "",    "000" },
        { "M",   "001" },
        { "D",   "010" },
        { "MD",  "011" }, 
        { "A",   "100" },
        { "AM",  "101" },
        { "AD",  "110" }, 
        { "AMD", "111" }, 
    }
};

ctab jmptab = {
    .size = 8,
    .entries = {
        { "",    "000" },
        { "JGT", "001" },
        { "JEQ", "010" },
        { "JGE", "011" },
        { "JLT", "100" },
        { "JNE", "101" },
        { "JLE", "110" },
        { "JMP", "111" },
    }
};

//
// Dynamic symbol table data type for A-instructions;
// maps labels/variables to addresses
//

// (There's really only one, but it needs to be dynamically allocated so it can grow if needed)

typedef struct {
    int    capacity;
    int    len;
    char** keys;
    int*   vals;
} atab;

atab* new_atab(int capacity)
{
    char** keys = calloc(capacity, sizeof(*keys));
    int*   vals = calloc(capacity, sizeof(*vals));
    if (keys == NULL || vals == NULL) return NULL;

    atab* tab = malloc(sizeof(*tab));
    if (tab == NULL) return NULL;

    tab->capacity = capacity;
    tab->len  = 0;
    tab->keys = keys;
    tab->vals = vals;

    return tab;
}

int ainsert(atab* tab, char* key, int val)
{
    if (tab->len == tab->capacity) {
        // Table is full and needs to be resized to accomodate a new entry
        tab->capacity *= 2;
        tab->keys = realloc(tab->keys, tab->capacity * sizeof(*(tab->keys)));
        tab->vals = realloc(tab->vals, tab->capacity * sizeof(*(tab->vals)));
        if (tab->keys == NULL || tab->vals == NULL) return 0;
    }
    char* newkey = strdup(key);
    if (newkey == NULL) return 0;

    tab->keys[tab->len] = newkey;
    tab->vals[tab->len] = val;
    tab->len++;
    return 1;
}

int alookup(atab* tab, char* key)
{
    // Start from the bottom so an append 'overrides' previous entries with the same key
    for (int i = tab->len - 1; i >= 0; i--)
        if (strcmp(key, tab->keys[i]) == 0)
            return tab->vals[i];
    return -1;
}

//
// Symbol table for the built-in variables
//

struct {
    int size;
    struct {
        char* key;
        int   val;
    } entries[];
} builtintab = {
    .size = 23,
    .entries = {
        { "R0",   0 }, { "R1",   1 }, { "R2",   2 }, { "R3",   3 }, 
        { "R4",   4 }, { "R5",   5 }, { "R6",   6 }, { "R7",   7 },
        { "R8",   8 }, { "R9",   9 }, { "R10", 10 }, { "R11", 11 },
        { "R12", 12 }, { "R13", 13 }, { "R14", 14 }, { "R15", 15 },
        { "SP",         0 },
        { "LCL",        1 },
        { "ARG",        2 },
        { "THIS",       3 },
        { "THAT",       4 },
        { "SCREEN", 16384 },
        { "KBD",    24576 }
    }
};

//
// Finally, the assembler
//

#define BUFSIZE 1024

int main(int argc, char** argv)
{
    terminate_if(argc < 3, "Arguments: input-file output-file");
    char* in_name  = argv[1];
    char* out_name = argv[2];

    char buf[BUFSIZE];
    // General-purpose string buffer

    //
    // Initialize the dynamic symbol table (which also involves inserting the 
    // built-in variables into it, which simplifies lookup later)
    //

    atab* atab = new_atab(100);
    terminate_if(atab == NULL, "Could not allocate memory for the symbol table.");

    for (int i = 0; i < builtintab.size; i++) {
        int ok = ainsert(atab, builtintab.entries[i].key, builtintab.entries[i].val);
        terminate_if(!ok, "Could not initialize the symbol table with the built-in variables.");
    }

    //
    // First pass: read the labels into atab and output a temporary file
    // without labels and without whitespace or comments to worry about,
    // only the instructions
    //

    FILE* in = fopen(in_name, "r");
    terminate_if(in == NULL, "Could not open input file.");

    char temp_name[64];  // Not in `buf' because it'd be overwritten and we'll still need the name later to delete the file 
    sprintf(temp_name, "temp%llu", time(0));

    FILE* out = fopen(temp_name, "w");  
    terminate_if(out == NULL, "Could not open temporary file to store the assembler's first pass output.");

    // Address of the next instruction (program counter)
    int nextaddr = 0;  
    // Current character being read
    int c;
    while (1) {
        // Ignore whitespace
        while ((c = getc(in)) != EOF && isspace(c))
            continue;
        if (c == EOF)
            break;

        // Ignore comments
        if (c == '/') {
            c = getc(in);
            if (c == '/') {
                while ((c = getc(in)) != '\n' && c != EOF)
                    continue;
                if (c == EOF)
                    break;
                // Done ignoring comment line, now restart the loop
                // (we restart because there may still be whitespace in the next line)
                continue;
            }
            else {
                // If it's not a comment (single / instead of //), put the
                // characters back in the file stream to read later.  This is
                // never going to happen in a valid program, but in doing this,
                // the present code segment is really just reponsible for
                // ignoring a comment line
                ungetc(c, in);
                ungetc('/', in);
            }
        }

        // Found a label declaration
        if (c == '(') {
            // Read it into a buffer and make an entry in the atab for it
            int i;
            for (i = 0; (c = getc(in)) != ')' && i < BUFSIZE-1; i++)
                buf[i] = c;
            buf[i] = '\0';

            int ok = ainsert(atab, buf, nextaddr);
            terminate_if(!ok, "Could not insert new label into the symbol table.");
            // Restart the loop from after the label declaration
            continue;
        }

        // At this point we know we have an actual instruction to read,
        // so we output it to the temp file for the second pass to parse it later
        putc(c, out);
        // Instructions have no spaces between them; that's what delimits them
        while ((c = getc(in)) != EOF && !isspace(c))  
            putc(c, out);
        if (c == EOF) {
            break;
        } else {
            putc('\n', out);
        }
        ++nextaddr;
    }

    fclose(in),  in  = NULL;
    fclose(out), out = NULL;

    //
    // Second pass
    //

    in = fopen(temp_name, "r");
    terminate_if(in == NULL, "Could not reopen the temporary file for the second pass.");

    out = fopen(out_name, "w");
    terminate_if(out == NULL, "Could not open or create output file.");

    // Next available register in memory for user variables (R0-R15 occupy the first 15)
    int nextreg = 16;  
    while (1) {
        // Now we don't need to ignore whitespace and comments, the first pass
        // already did that for us

        //
        // Read instruction into buffer and determine format
        //

        // Whether it's an A- or C- instruction
        int ains=0;  
        //  index of '='   index of ';'      length (also index of '\0')
        int equals_ind=-1, semicolon_ind=-1, len=0;

        c = getc(in);
        ains = c == '@';

        // Read the instruction into the buffer
        for (len = 0; !isspace(c) && len < BUFSIZE-1 && c != EOF; len++)
        {
            if      (c == '=') equals_ind    = len;
            else if (c == ';') semicolon_ind = len;
            buf[len] = c;
            c = getc(in);
        }
        buf[len] = '\0';
        if (c == EOF)
            break;

        //
        // Parse the instruction and emit its binary representation
        //

        // Parsing A-instruction
        if (ains) {
            unsigned num;
            // buf+1 to skip the first character @
            if (isdigit(buf[1])) {
                num = atoi(buf+1);
            } else {
                num = alookup(atab, buf+1);
                if (num == -1) {
                    // Not in the table; it's a variable declaration
                    num = nextreg;
                    int ok = ainsert(atab, buf+1, num);
                    terminate_if(!ok, "Could not insert new variable into the symbol table.");
                    nextreg++;
                }
            }

            // Write it in binary
            putc('0', out);
            writebin(num, 15, buf);
            fputs(buf, out);
            putc('\n', out);
        }
        // Parsing C-instruction
        else {
            // Isolate each field, according to
            // dest=comp;jmp
            // where dest and jmp are optional

            // If the equals sign is absent, meaning there's no dest field and comp is the first field,
            // then we make `dest' be the empty string by pointing it to the '\0' at the end of `buf',
            // and we make `comp' point to the start of `buf'
            char* dest = buf + len;
            char* comp = buf;
            if (equals_ind != -1) {
                // But if the equals sign is present, meaning there IS a dest field,
                // then we make `dest' point to the start of the buf,
                // and `comp' point to just after the equals sign, where the computation is described
                dest = buf;
                comp = buf + equals_ind + 1;
                // We also replace '=' with '\0' in the instruction so `dest' can be treated as a string,
                // which we want for the lookup
                buf[equals_ind] = '\0';
            }

            // +- Same logic here
            char *jmp = buf + len;
            if (semicolon_ind != -1) {
                jmp = buf + semicolon_ind + 1;
                buf[semicolon_ind] = '\0';
            }

            // Now write it in binary
            // (we could store clookup's result in an intermediary char* and terminate if it's NULL,
            // but since we're operating under the assumption that the program is correct,
            // and since it looks a lot nicer this way, we'll just call fputs on it directly)
            fputs("111", out);
            fputs(clookup(&comptab, comp), out);
            fputs(clookup(&desttab, dest), out);
            fputs(clookup(&jmptab,  jmp),  out);
            putc('\n', out);
        }
    }

    // Close the temporary file and delete it
    fclose(in);
    remove(temp_name);
    // Any other files are closed as the program exits
    // and the allocated memory is freed to

    return 0;
}
