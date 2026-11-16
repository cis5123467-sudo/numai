#ifndef NUMAI_PHYSICS_H
#define NUMAI_PHYSICS_H

#include <stdbool.h>
#include <stddef.h>

#define PHYSICS_VARIABLES 4
#define PHYSICS_REFERENCE_SIZE 1800

enum {
    PhysicsNonnegative = 1, PhysicsPositive = 2, PhysicsEfficiency = 4,
    PhysicsTurns = 8, PhysicsBothSigns = 16
};

typedef struct {
    const char *symbol, *name, *unit, *formula;
    unsigned flags;
} PhysicsVariable;

typedef struct {
    const char *title, *topic, *word, *symbol, *uses, *example;
    bool higher;
    int count;
    PhysicsVariable variables[PHYSICS_VARIABLES];
} PhysicsEquation;

extern const PhysicsEquation physics_equations[];
extern const int physics_equation_count;
void physics_reference(const PhysicsEquation *equation, char output[PHYSICS_REFERENCE_SIZE]);
bool physics_solve(const PhysicsEquation *equation, int subject,
                   const char *const values[PHYSICS_VARIABLES], char *output, size_t size);
#endif
