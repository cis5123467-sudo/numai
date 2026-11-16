#ifndef NUMAI_CAS_H
#define NUMAI_CAS_H
#include <stdbool.h>

#define EXPRESSION_SIZE 112
#define ANSWER_SIZE 512

typedef enum {
    Calculate, Factorise, Simplify, Expand, Solve, Differentiate,
    Integrate, Definite, Substitute, FindRoot, CompleteSquare, OperationCount
} Operation;

typedef struct {
    char steps[ANSWER_SIZE];
    char answer[ANSWER_SIZE];
    bool ok;
} Solution;

extern const char *const operation_names[OperationCount];
bool cas_read_number(const char *input, double *value);
void solve_expression(Operation operation, const char *expression, const char *second,
                      const char *third, bool degrees, bool decimal, Solution *solution);

#endif
