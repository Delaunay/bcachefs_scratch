#include "logger.h"
#include "version.h"

#include "bcachefs.h"

#include <cassert>
#include <cstdio>

using String = std::string;

namespace {
inline uint64_t extract_bitflag(const uint64_t bitfield, uint8_t first_bit, uint8_t last_bit) {
    return bitfield << (sizeof(bitfield) * 8 - last_bit) >> (sizeof(bitfield) * 8 - last_bit + first_bit);
}
} // namespace

// the roots of the various b-trees are stored in jset_entry entries
struct BCacheJournal {};

struct BCacheFSReader {
    using Superblock = struct bch_sb;

    using SuperBlockFieldType  = enum bch_sb_field_type;
    using SuperBlockFieldBase  = struct bch_sb_field;
    using SuperBlockFieldClean = struct bch_sb_field_clean;

    using JournalSetEntryType = enum bch_jset_entry_type;
    using JournalSetEntry     = struct jset_entry;

    BCacheFSReader(String const &file) {
        _file = fopen(file.c_str(), "rb");

        debug(">>> Reading superblock");
        // Read the superblock at 4096 | 0x1000
        auto size = sizeof(Superblock);

        _sblock = (Superblock *)malloc(size);
        fseek(_file, BCH_SB_SECTOR * BCH_SECTOR_SIZE, SEEK_SET);
        fread(_sblock, size, 1, _file);

        // Realloc the superblock now we have all the data to allocate the true size
        assert(memcmp(&_sblock->magic, &BCACHE_MAGIC, sizeof(BCACHE_MAGIC)) == 0);
        size = sizeof(struct bch_sb) + _sblock->u64s * BCH_U64S_SIZE;

        _sblock = (Superblock *)realloc(_sblock, size);
        fseek(_file, BCH_SB_SECTOR * BCH_SECTOR_SIZE, SEEK_SET);
        fread(_sblock, size, 1, _file);

        // -----
        debug("<<< Read superblock");

        // Get the location of the different BTrees
        // bch_sb_field_clean entry is written on clean shutdown
        // it contains the jset_entry that holds the root node of the BTrees

        debug("Look for superblock field clean");
        auto field = (const SuperBlockFieldClean *)find_superblock_field(BCH_SB_FIELD_clean);

        debug("Look for journal entry");
        auto entry = find_journal_entry(field, BCH_JSET_ENTRY_btree_root);
        assert(entry->btree_id == BTREE_ID_extents);
    }

    SuperBlockFieldBase const *find_superblock_field(SuperBlockFieldType type) const {
        auto start = (SuperBlockFieldBase const *)((const uint8_t *)_sblock + sizeof(Superblock));
        auto end   = (SuperBlockFieldBase const *)(_sblock + _sblock->u64s * BCH_U64S_SIZE);
        return find_field<SuperBlockFieldBase>(type, BCH_SB_FIELD_NR, start, end);
    }

    JournalSetEntry const *find_journal_entry(SuperBlockFieldClean const *field, JournalSetEntryType type) const {
        auto start = (JournalSetEntry const *)((const uint8_t *)field + sizeof(SuperBlockFieldClean));
        auto end   = (JournalSetEntry const *)(field + field->field.u64s * BCH_U64S_SIZE);
        return find_field<JournalSetEntry>(type, BCH_JSET_ENTRY_NR, start, end);
    }

    template <typename T>
    struct u64s_spec get_spec() const {
        struct u64s_spec spec;
        return spec;
    }

    template <typename T>
    uint64_t get_u64s(T *v) {
        return v->u64s;
    }

    template <typename T>
    const T *find_field(uint32_t type, uint32_t type_max, T const *start, T const *end) const {
        if (type == type_max) {
            return nullptr;
        }

        auto field = start;
        while (field && field->type != type_max && field->type != type) {
            uint64_t u64s = field->u64s + get_spec<T>().start;
            field         = (T const *)((const uint8_t *)field + u64s * BCH_U64S_SIZE);
            debug("(size: {}) (type: {}) looking for {}", field->u64s, field->type, type);

            if (field >= end) {
                break;
            }
        }

        return field;
    }

    ~BCacheFSReader() {
        fclose(_file);
        free(_sblock);
    }

    // extract the size of a btree node
    uint64_t btree_node_size() const {
        return (uint64_t)(uint16_t)extract_bitflag(_sblock->flags[0], 12, 28) * BCH_SECTOR_SIZE;
    }

    public:
    FILE *      _file   = nullptr;
    Superblock *_sblock = nullptr;
};

template <>
struct u64s_spec BCacheFSReader::get_spec<BCacheFSReader::JournalSetEntry>() const {
    return U64S_JSET_ENTRY;
}

template <>
struct u64s_spec BCacheFSReader::get_spec<BCacheFSReader::SuperBlockFieldBase>() const {
    return U64S_BCH_SB_FIELD;
}

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

    BCacheFSReader("dataset.img");

    return 0;
}
