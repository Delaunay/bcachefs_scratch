/* Include Guard */
#ifndef INCLUDE_BENZINA_BCACHEFS_READER_H
#define INCLUDE_BENZINA_BCACHEFS_READER_H

#include "cbcachefs.h"
#include <string>
#include <vector>

template <typename V>
using Array  = std::vector<V>;
using String = std::string;

using Superblock = struct bch_sb;

using SuperBlockFieldType  = enum bch_sb_field_type;
using SuperBlockFieldBase  = struct bch_sb_field;
using SuperBlockFieldClean = struct bch_sb_field_clean;

using JournalSetEntryType = enum bch_jset_entry_type;
using JournalSetEntry     = struct jset_entry;

inline uint64_t extract_bitflag(const uint64_t bitfield, uint8_t first_bit, uint8_t last_bit) {
    return bitfield << (sizeof(bitfield) * 8 - last_bit) >> (sizeof(bitfield) * 8 - last_bit + first_bit);
}

struct BCacheFSReader {
    public:
    BCacheFSReader(String const &file);

    ~BCacheFSReader();

    private:
    // Load the superblock in 2 phases
    //  - first phase reads only enough to be able to extract the full size of the superblock
    //  - second phase reallocate the superblock using its full size
    Superblock *read_superblock();

    SuperBlockFieldBase const *find_superblock_field(SuperBlockFieldType type) const;

    JournalSetEntry const *find_journal_entry(SuperBlockFieldClean const *field, JournalSetEntryType type) const;

    Array<JournalSetEntry const *> find_btree_roots(SuperBlockFieldClean const *field) const;

    // extract the size of a btree node
    uint64_t btree_node_size() const {
        return (uint64_t)(uint16_t)extract_bitflag(_sblock->flags[0], 12, 28) * BCH_SECTOR_SIZE;
    }

    public:
    FILE *      _file   = nullptr;
    Superblock *_sblock = nullptr;

    friend struct BCacheFS_Iterator;
};

#endif