#include <stdio.h>

#define MAX_LINE_LENGTH 1000


int main(int argc, char *argv[]) {

    if (argc < 2) {
        return 0;
    }

    char line[MAX_LINE_LENGTH];
    for (int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "r");
        if (!f) {
            printf("wcat: cannot open file\n");
            return 1;
        }

        while (fgets(line, MAX_LINE_LENGTH-1, f) != NULL) {
            line[MAX_LINE_LENGTH-1] = 0;
            printf("%s", line);
        }
        
        fclose(f);
    }

    return 0;

}
