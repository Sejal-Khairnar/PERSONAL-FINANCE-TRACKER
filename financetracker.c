/*
  Personal Finance Tracker (C, single-file)
  Features:
    - Store transactions in an array of structs (income/expense)
    - Add/list/sort/search/filter
    - Save to and load from a file (plain text, |-separated)
    - ASCII bar chart of monthly EXPENSE spending for a chosen year

  Compile:  gcc -std=c11 -O2 finance_tracker.c -o finance_tracker
  Run:      ./finance_tracker
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TRANSACTIONS 2000
#define STR_LEN 64
#define NOTE_LEN 128
#define FILE_NAME "finance_data.txt"

typedef enum { INCOME = 0, EXPENSE = 1 } TxType;

typedef struct {
    int y, m, d;                  // date: year, month, day
    TxType type;                  // 0 income, 1 expense
    char category[STR_LEN];       // e.g., Food, Rent, Salary
    double amount;                // positive amount
    char note[NOTE_LEN];          // optional note
} Transaction;

static Transaction txs[MAX_TRANSACTIONS];
static int txCount = 0;

/* ----------------------- Utility I/O helpers ----------------------- */

static void trim_newline(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    if (n && (s[n-1] == '\n' || s[n-1] == '\r')) s[n-1] = '\0';
}

static void read_line(const char *prompt, char *buf, size_t n) {
    printf("%s", prompt);
    if (fgets(buf, (int)n, stdin) == NULL) { buf[0] = '\0'; return; }
    trim_newline(buf);
}

static int read_int(const char *prompt, int minV, int maxV) {
    char line[64];
    int x;
    for (;;) {
        printf("%s", prompt);
        if (!fgets(line, sizeof(line), stdin)) continue;
        if (sscanf(line, "%d", &x) == 1 && x >= minV && x <= maxV) return x;
        printf("Invalid input. Please enter an integer in [%d..%d].\n", minV, maxV);
    }
}

static double read_double(const char *prompt, double minV) {
    char line[64];
    double x;
    for (;;) {
        printf("%s", prompt);
        if (!fgets(line, sizeof(line), stdin)) continue;
        if (sscanf(line, "%lf", &x) == 1 && x >= minV) return x;
        printf("Invalid input. Please enter a number >= %.2f.\n", minV);
    }
}

static int valid_date(int y, int m, int d) {
    if (y < 1900 || y > 3000) return 0;
    if (m < 1 || m > 12) return 0;
    int mdays[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    int leap = ( (y%4==0 && y%100!=0) || (y%400==0) );
    if (leap) mdays[2] = 29;
    if (d < 1 || d > mdays[m]) return 0;
    return 1;
}

/* ----------------------- Core operations -------------------------- */

static void add_transaction(void) {
    if (txCount >= MAX_TRANSACTIONS) {
        printf("Storage full.\n");
        return;
    }

    int y = read_int("Year (e.g., 2025): ", 1900, 3000);
    int m = read_int("Month (1-12): ", 1, 12);
    int d = read_int("Day (1-31): ", 1, 31);
    if (!valid_date(y, m, d)) {
        printf("Invalid date.\n");
        return;
    }

    int t = read_int("Type (0 = Income, 1 = Expense): ", 0, 1);

    char category[STR_LEN];
    read_line("Category (e.g., Salary, Food, Rent): ", category, sizeof(category));
    if (category[0] == '\0') strcpy(category, (t==INCOME) ? "Salary" : "Misc");

    double amount = read_double("Amount: ", 0.0);
    if (amount <= 0.0) { printf("Amount must be positive.\n"); return; }

    char note[NOTE_LEN];
    read_line("Note (optional, no '|' please): ", note, sizeof(note));
    // Replace '|' if present to keep file format simple
    for (char *p = note; *p; ++p) if (*p == '|') *p = '/';
    for (char *p = category; *p; ++p) if (*p == '|') *p = '/';

    txs[txCount++] = (Transaction){ y, m, d, (TxType)t, {0}, amount, {0} };
    strncpy(txs[txCount-1].category, category, STR_LEN-1);
    strncpy(txs[txCount-1].note, note, NOTE_LEN-1);

    printf("Transaction added. Total = %d\n", txCount);
}

static void print_header(void) {
    printf("Idx  Date        Type     Category               Amount      Note\n");
    printf("---- ----------- -------- ---------------------- ----------- ------------------------------\n");
}

static void print_transaction(int i, const Transaction *t) {
    printf("%-4d %04d-%02d-%02d %-8s %-22s %11.2f %s\n",
           i, t->y, t->m, t->d, t->type==INCOME?"INCOME":"EXPENSE",
           t->category, t->amount, t->note);
}

static void list_all(void) {
    if (txCount == 0) { printf("No transactions.\n"); return; }
    print_header();
    for (int i = 0; i < txCount; ++i) print_transaction(i, &txs[i]);
}

/* ----------------------- Sorting ---------------------------------- */

static int cmp_date(const void *a, const void *b) {
    const Transaction *x = (const Transaction*)a, *y = (const Transaction*)b;
    if (x->y != y->y) return (x->y < y->y) ? -1 : 1;
    if (x->m != y->m) return (x->m < y->m) ? -1 : 1;
    if (x->d != y->d) return (x->d < y->d) ? -1 : 1;
    return 0;
}

static int cmp_amount_desc(const void *a, const void *b) {
    const Transaction *x = (const Transaction*)a, *y = (const Transaction*)b;
    if (x->amount < y->amount) return 1;
    if (x->amount > y->amount) return -1;
    return 0;
}

static void sort_menu(void) {
    if (txCount == 0) { printf("No transactions to sort.\n"); return; }
    printf("Sort by:\n  1) Date (ascending)\n  2) Amount (descending)\n");
    int c = read_int("Choose: ", 1, 2);
    if (c == 1) qsort(txs, txCount, sizeof(Transaction), cmp_date);
    else        qsort(txs, txCount, sizeof(Transaction), cmp_amount_desc);
    printf("Sorted.\n");
}

/* ----------------------- Searching/Filtering ---------------------- */

static void to_lower_str(char *s) {
    for (; *s; ++s) *s = (char)tolower((unsigned char)*s);
}

static void search_menu(void) {
    if (txCount == 0) { printf("No data.\n"); return; }
    printf("Search by:\n  1) Category contains text\n  2) Note contains text\n  3) Date equals (YYYY-MM-DD)\n");
    int c = read_int("Choose: ", 1, 3);

    if (c == 1 || c == 2) {
        char q[STR_LEN];
        read_line("Enter text: ", q, sizeof(q));
        char ql[STR_LEN]; strncpy(ql, q, sizeof(ql)); ql[STR_LEN-1] = 0; to_lower_str(ql);

        int found = 0;
        print_header();
        for (int i = 0; i < txCount; ++i) {
            char hay[NOTE_LEN];
            if (c == 1) {
                strncpy(hay, txs[i].category, sizeof(hay)); hay[NOTE_LEN-1] = 0;
            } else {
                strncpy(hay, txs[i].note, sizeof(hay)); hay[NOTE_LEN-1] = 0;
            }
            to_lower_str(hay);
            if (strstr(hay, ql)) { print_transaction(i, &txs[i]); found = 1; }
        }
        if (!found) printf("No matches.\n");
    } else {
        int y = read_int("Year: ", 1900, 3000);
        int m = read_int("Month: ", 1, 12);
        int d = read_int("Day: ", 1, 31);
        if (!valid_date(y,m,d)) { printf("Invalid date.\n"); return; }
        int found = 0;
        print_header();
        for (int i = 0; i < txCount; ++i) {
            if (txs[i].y==y && txs[i].m==m && txs[i].d==d) {
                print_transaction(i, &txs[i]);
                found = 1;
            }
        }
        if (!found) printf("No matches.\n");
    }
}

static void filter_expenses_over(void) {
    if (txCount == 0) { printf("No data.\n"); return; }
    double thr = read_double("Show EXPENSES over amount: ", 0.0);
    int found = 0;
    print_header();
    for (int i = 0; i < txCount; ++i) {
        if (txs[i].type == EXPENSE && txs[i].amount > thr) {
            print_transaction(i, &txs[i]);
            found = 1;
        }
    }
    if (!found) printf("No expenses above that amount.\n");
}

/* ----------------------- Save & Load ------------------------------ */
/* Format: y|m|d|type|category|amount|note\n
   type: 0 income, 1 expense
   '|' in text replaced with '/' on input
*/

static int save_to_file(const char *fname) {
    FILE *f = fopen(fname, "w");
    if (!f) { perror("fopen"); return 0; }
    for (int i = 0; i < txCount; ++i) {
        fprintf(f, "%d|%d|%d|%d|%s|%.2f|%s\n",
            txs[i].y, txs[i].m, txs[i].d, txs[i].type,
            txs[i].category, txs[i].amount, txs[i].note);
    }
    fclose(f);
    return 1;
}

static int load_from_file(const char *fname) {
    FILE *f = fopen(fname, "r");
    if (!f) { perror("fopen"); return 0; }
    int n = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        Transaction t = {0};
        char category[STR_LEN] = {0};
        char note[NOTE_LEN] = {0};
        int typeInt = 0;
        double amount = 0.0;

        // Parse | separated, note/category may have spaces
        // Using sscanf with scansets to stop at '|' ( %[^\|] )
        int matched = sscanf(line,
            "%d|%d|%d|%d|%63[^|]|%lf|%127[^\n]",
            &t.y, &t.m, &t.d, &typeInt, category, &amount, note);

        if (matched >= 6) {
            t.type = (typeInt==1)?EXPENSE:INCOME;
            strncpy(t.category, category, STR_LEN-1);
            t.amount = amount;
            if (matched == 7) strncpy(t.note, note, NOTE_LEN-1); else t.note[0] = '\0';
            if (valid_date(t.y,t.m,t.d) && t.amount >= 0.0) {
                if (n < MAX_TRANSACTIONS) txs[n++] = t;
            }
        }
    }
    fclose(f);
    txCount = n;
    return 1;
}

/* ----------------------- ASCII Monthly Chart ---------------------- */

static void monthly_spending_chart(void) {
    if (txCount == 0) { printf("No data.\n"); return; }
    int year = read_int("Enter year for EXPENSE chart: ", 1900, 3000);
    double sums[13] = {0.0}; // 1..12
    for (int i = 0; i < txCount; ++i) {
        if (txs[i].type == EXPENSE && txs[i].y == year) {
            if (txs[i].m >=1 && txs[i].m <=12) sums[txs[i].m] += txs[i].amount;
        }
    }
    // Find max to scale bars
    double maxv = 0.0;
    for (int m = 1; m <= 12; ++m) if (sums[m] > maxv) maxv = sums[m];

    if (maxv == 0.0) {
        printf("No expenses recorded for %d.\n", year);
        return;
    }

    const char *mon[13] = {"","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    int maxWidth = 50; // characters
    printf("\nMonthly Expense Chart for %d (each # ~ scaled)\n", year);
    for (int m = 1; m <= 12; ++m) {
        int bar = (int)((sums[m] / maxv) * maxWidth + 0.5);
        if (bar < 0) bar = 0;
        printf("%3s | ", mon[m]);
        for (int k = 0; k < bar; ++k) putchar('#');
        printf("  %.2f\n", sums[m]);
    }
    printf("\nTotal expenses in %d: ", year);
    double total = 0.0; for (int m = 1; m <= 12; ++m) total += sums[m];
    printf("%.2f\n\n", total);
}

/* ----------------------- Summary totals --------------------------- */

static void show_summary(void) {
    double income = 0.0, expense = 0.0;
    for (int i = 0; i < txCount; ++i) {
        if (txs[i].type == INCOME) income += txs[i].amount;
        else expense += txs[i].amount;
    }
    printf("Summary (all time): Income = %.2f | Expense = %.2f | Savings = %.2f\n",
           income, expense, income - expense);
}

/* ----------------------- Delete / Edit (optional helpers) -------- */

static void delete_by_index(void) {
    if (txCount == 0) { printf("No data.\n"); return; }
    int idx = read_int("Index to delete: ", 0, txCount-1);
    for (int i = idx; i < txCount-1; ++i) txs[i] = txs[i+1];
    txCount--;
    printf("Deleted. Remaining = %d\n", txCount);
}

/* ----------------------- Menu ------------------------------------ */

static void menu(void) {
    for (;;) {
        printf("\n==== Personal Finance Tracker ====\n");
        printf("1) Add transaction\n");
        printf("2) List all\n");
        printf("3) Sort (date/amount)\n");
        printf("4) Search (category/note/date)\n");
        printf("5) Filter: expenses over threshold\n");
        printf("6) Save to file\n");
        printf("7) Load from file\n");
        printf("8) Monthly expense ASCII chart\n");
        printf("9) Summary totals\n");
        printf("10) Delete by index\n");
        printf("0) Exit\n");
        int c = read_int("Choose: ", 0, 10);
        switch (c) {
            case 1: add_transaction(); break;
            case 2: list_all(); break;
            case 3: sort_menu(); break;
            case 4: search_menu(); break;
            case 5: filter_expenses_over(); break;
            case 6:
                if (save_to_file(FILE_NAME)) printf("Saved to '%s'.\n", FILE_NAME);
                else printf("Save failed.\n");
                break;
            case 7:
                if (load_from_file(FILE_NAME)) printf("Loaded from '%s'. %d records.\n", FILE_NAME, txCount);
                else printf("Load failed.\n");
                break;
            case 8: monthly_spending_chart(); break;
            case 9: show_summary(); break;
            case 10: delete_by_index(); break;
            case 0: printf("Goodbye!\n"); return;
            default: break;
        }
    }
}

int main(void) {
    // Try to load existing data on startup (optional)
    load_from_file(FILE_NAME); // ignore error if file doesn't exist
    printf("Welcome! %d existing record(s) loaded (if any) from %s.\n", txCount, FILE_NAME);
    menu();
    return 0;
}
