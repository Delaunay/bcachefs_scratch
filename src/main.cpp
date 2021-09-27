#include "logger.h"
#include "0.01.h"

NEW_EXCEPTION(HugeError);

int fun1(){
    if (true){
        throw HugeError("exepected {} got {}", 1, 2);
    }

    return 0;
}


int main(){
    info("Testing throwing function");
    info("version hash  : {}", _HASH);
    info("version date  : {}", _DATE);
    info("version branch: {}", _BRANCH);

    try {
        fun1();
    } catch (const HugeError& ex) {
        ex.what();
    }

    return 0;
}
