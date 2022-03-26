/**
 * Symbol table for C-instructions.
 * Maps assembly instruction segments to their binary counterparts.
 */
typedef struct {
    int size;
    struct {
        char* key;
        char* val;
    } entries[];
} ctab;

/**
 * Looks up the value associated with key in the given ctab, or NULL if not found.
 */
char* clookup(ctab* tab, char* key)
{
    // These symbol tables are small enough that sequential search is fine
    // (although since they are static we could pre-sort them and do a binary search)
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


#define ATAB_CAPACITY (1<<16)
/**
 * Symbol-table for A-instructions -- for variables and labels.
 * This is a hash table using open addressing with linear probing .
 * A program has at most 32768 instructions, so at most 32768
 * A-instructions. To ensure a load factor of 50% in the
 * absolutely worst case, this table is statically allocated
 * with 65536 (2^16) slots.
 */
struct {
    int   size;
    char *keys[ATAB_CAPACITY];
    int   vals[ATAB_CAPACITY];
} atab;

static unsigned ahash(char *s)
{
    unsigned hash = 5381;
    int c;
    while (c = *s++)
        hash = ((hash << 5) + hash) ^ c;
    return hash % ATAB_CAPACITY;
}

/**
 * Associates key with val in the atab.
 * Returns whether it did it.
 */
int ainsert(char* key, int val)
{
    unsigned i = ahash(key);

    while (atab.keys[i]) {
        if (strcmp(key, atab.keys[i]) == 0) {
            atab.vals[i] = val;
            return 1;
        }
        if (++i == ATAB_CAPACITY) i = 0;
    }

    char* newkey = strdup(key);
    if (newkey == NULL)
        return 0;

    atab.keys[i] = newkey;
    atab.vals[i] = val;
    return 1;
}

/**
 * Looks up the value associated with key in the atab,
 * putting it in *val (out parameter) and returning whether
 * the key was actually found.
 */
int alookup(char* key, int* val)
{
    // We need to do it like this because there's no special value like NULL
    // to denote that the val (int) was not found. For isntance, if we used -1
    // the user would not be able ever to associate -1 with a variable.

    unsigned i = ahash(key);

    while (atab.keys[i]) {
        if (strcmp(key, atab.keys[i]) == 0) {
            *val = atab.vals[i];
            return 1;
        }
        if (++i == ATAB_CAPACITY) i = 0;
    }
    return 0;
}

int init_atab()
{
    atab.size = 0;
    memset(atab.keys, (long) NULL, ATAB_CAPACITY);
    memset(atab.vals, 0, ATAB_CAPACITY);
    if (!ainsert("R0",   0)) return 0;
    if (!ainsert("R1",   1)) return 0;
    if (!ainsert("R2",   2)) return 0;
    if (!ainsert("R3",   3)) return 0;
    if (!ainsert("R4",   4)) return 0;
    if (!ainsert("R5",   5)) return 0;
    if (!ainsert("R6",   6)) return 0;
    if (!ainsert("R7",   7)) return 0;
    if (!ainsert("R8",   8)) return 0;
    if (!ainsert("R9",   9)) return 0;
    if (!ainsert("R10", 10)) return 0;
    if (!ainsert("R11", 11)) return 0;
    if (!ainsert("R12", 12)) return 0;
    if (!ainsert("R13", 13)) return 0;
    if (!ainsert("R14", 14)) return 0;
    if (!ainsert("R15", 15)) return 0;
    if (!ainsert("SP",         0)) return 0;
    if (!ainsert("LCL",        1)) return 0;
    if (!ainsert("ARG",        2)) return 0;
    if (!ainsert("THIS",       3)) return 0;
    if (!ainsert("THAT",       4)) return 0;
    if (!ainsert("SCREEN", 16384)) return 0;
    if (!ainsert("KBD",    24576)) return 0;
    return 1;
}
