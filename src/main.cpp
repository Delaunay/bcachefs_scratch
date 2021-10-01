#include "logger.h"
#include "version.h"

#include "bcachefs.h"

#include <cassert>
#include <cstdio>
#include <iostream>

int main() {
    info("version hash  : {}", _HASH);
    info("version date  : {}", _DATE);
    info("version branch: {}", _BRANCH);

    BCacheFSReader reader("dataset.img");

    {
        BTreeIterator iter = reader.iterator(BTREE_ID_extents);

        auto bkey = iter.next_key();

        while (bkey != nullptr) {
            std::cout << *bkey << std::endl;

            auto ext = iter.extend(bkey);
            std::cout << "    - ext " << ext << "\n";

            bkey = iter.next_key();
        }
    }

    {
        //
        BTreeIterator iter = reader.iterator(BTREE_ID_dirents);

        auto bkey = iter.next_key();

        while (bkey != nullptr) {
            std::cout << *bkey << std::endl;

            auto dir = iter.directory(bkey);
            std::cout << "    - dirent " << dir << "\n";

            bkey = iter.next_key();
        }
    }

    return 0;
}
