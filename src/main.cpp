#include "logger.h"
#include "version.h"

#include "bcachefs.h"

#include <cassert>
#include <cstdio>

struct BCacheFS_Iterator {
    using BTreeNode = struct btree_node;

    BCacheFS_Iterator(BCacheFSReader *reader) {
        _node = (BTreeNode *)malloc(reader->btree_node_size());

        // jset business (journal?)
    }

    ~BCacheFS_Iterator() { free(_node); }

    public:
    BTreeNode *_node = nullptr;
};

int main() {
    info("version hash  : {}", _HASH);
    info("version date  : {}", _DATE);
    info("version branch: {}", _BRANCH);

    BCacheFSReader reader("dataset.img");

    BTreeIterator iter = reader.iterator(BTREE_ID_extents);

    auto bkey = iter.next_key();
    while (bkey != nullptr) {
        printf("bkey: u:%u, f:%u, t:%u, s:%u, o:%llu\n", bkey->u64s, bkey->format, bkey->type, bkey->size,
               bkey->p.offset);

        bkey = iter.next_key();
    }

    return 0;
}
