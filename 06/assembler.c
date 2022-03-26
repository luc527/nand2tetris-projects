#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "symbol-tables.h"

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

#define BUFSIZE 1024

int main(int argc, char** argv)
{
    terminate_if(argc < 3, "Arguments: input-file output-file");
    char* in_name  = argv[1];
    char* out_name = argv[2];

    char buf[BUFSIZE];
    // General-purpose string buffer

    terminate_if(!init_atab(),
                 "Could not initialize the symbol table with the built-in variables.");

    //
    // First pass: read the labels into atab and output a temporary file
    // without labels and without whitespace or comments to worry about,
    // only the instructions
    //

    FILE* in = fopen(in_name, "r");
    terminate_if(in == NULL, "Could not open input file.");

    char temp_name[64];  // Not in `buf' because it'd be overwritten and we'll still need the name later to delete the file 
    sprintf(temp_name, "temp%lu", time(0));

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

            int ok = ainsert(buf, nextaddr);
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
                int found = alookup(buf+1, &num);
                if (!found) {
                    // Not in the table; it's a variable declaration
                    num = nextreg;
                    int ok = ainsert(buf+1, num);
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
