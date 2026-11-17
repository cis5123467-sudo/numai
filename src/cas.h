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

typedef enum { ExtraNone, ExtraRoot, ExtraSin, ExtraCos, ExtraTan } CalculationKind;
typedef struct {
    double value, argument;
    int numerator, denominator, radicand, argument_n, argument_d, argument_rad;
    CalculationKind kind;
    bool numeric, degrees;
    char exact[64];
} CalculationValue;

extern const char *const operation_names[OperationCount];
bool cas_read_number(const char *input, double *value);
bool cas_calculation(const char *input, bool degrees, CalculationValue *result);
void cas_trig_equation(const char *input, const char *lower, const char *upper,
                       bool degrees, Solution *solution);
void solve_expression(Operation operation, const char *expression, const char *second,
                      const char *third, bool degrees, bool decimal, Solution *solution);

#endif
