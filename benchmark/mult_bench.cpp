#ifndef VANAGANDR_BENCH_EIGENDATE_HEADER
#define VANAGANDR_BENCH_EIGENDATE_HEADER

#include <hayai.hpp>

#include <Symbolic.h>


class SymbolicBench: public ::hayai::Fixture
{
public:
    virtual void SetUp() {

        mult = sym::mult(
            sym::mult(
                sym::mult(sym::make_val(3475), sym::make_val(5743)),
                sym::mult(sym::make_val(3475), sym::make_val(5743))),
            sym::mult(
                sym::mult(sym::make_val(3475), sym::make_val(5743)),
                sym::mult(sym::make_val(3475), sym::make_val(5743))));
    }

    virtual void TearDown(){
        delete mult;
    }

    sym::Context c;
    sym::ABSExpr* mult;
};

BENCHMARK_F(SymbolicBench, Mult, 10, 100)
{
    mult->full_eval(c);
}

volatile int v1 = 5743;
volatile int v2 = 3475;
volatile int v3 = 5743;
volatile int v4 = 3475;
volatile int v5 = 5743;
volatile int v6 = 3475;
volatile int v7 = 5743;
volatile int v8 = 3475;

BENCHMARK(Math, MultNative, 10, 100)
{
    ((v1 * v2) * (v3 * v4)) * ((v5 * v6) * (v7 * v8));
}


#endif

