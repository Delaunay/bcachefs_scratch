#include "logger.h"
#include "version.h"

#include "bcachefs.h"

#include <cassert>
#include <cstdio>

int main() {
    info("version hash  : {}", _HASH);
    info("version date  : {}", _DATE);
    info("version branch: {}", _BRANCH);

    BCacheFSReader reader("dataset.img");

    {
        BTreeIterator iter = reader.iterator(BTREE_ID_extents);

        auto bkey = iter.next_key();

        while (bkey != nullptr) {
            printf("bkey: u:%u, f:%u, t:%u, s:%u, o:%lu\n", bkey->u64s, bkey->format, bkey->type, bkey->size,
                   bkey->p.offset);

            bkey = iter.next_key();
        }
    }

    {
        //
        BTreeIterator iter = reader.iterator(BTREE_ID_dirents);

        auto bkey = iter.next_key();

        while (bkey != nullptr) {
            printf("bkey: u:%u, f:%u, t:%u, s:%u, o:%lu\n", bkey->u64s, bkey->format, bkey->type, bkey->size,
                   bkey->p.offset);

            bkey = iter.next_key();
        }
    }

    return 0;
}
