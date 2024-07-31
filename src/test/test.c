#include <stdio.h>

#include <sys/mman.h>

#include "../cnm.c"

uint8_t scratch_buf[2048];
void *code_area;
size_t code_size;

static void default_print(int line, const char *verbose, const char *simple) {
    printf("%s", verbose);
}

int main(int argc, char **argv) {
    printf("cnm tester\n");
   
    code_size = 2048;
    code_area = mmap(NULL, code_size, PROT_EXEC | PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANON, -1, 0);

    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_parse(cnm, "\"hello \"  \"world\" += \"foo\"", "test.cnm", NULL, 0);
    token_next(cnm);
    token_next(cnm);
    token_next(cnm);
    token_next(cnm);

    munmap(code_area, code_size);

    return 0;
}

