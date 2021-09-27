#include "bcachefs.h"
#include "logger.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

// ========================================================================================
// Utils
// ----------------------------------------------------------------------------------------

// Lookup the size spec given a type
template <typename T>
struct u64s_spec get_spec() {
    struct u64s_spec spec;
    return spec;
}

template <typename T>
const T *find_field(uint32_t type, uint32_t type_max, T const *start, T const *end);

// ========================================================================================

BCacheFSReader::BCacheFSReader(String const &file) {
    _file   = fopen(file.c_str(), "rb");
    _sblock = read_superblock();

    // Get the location of the different BTrees
    // bch_sb_field_clean entry is written on clean shutdown
    // it contains the jset_entry that holds the root node of the BTrees
    //
    // The btrees are necessary for us to do everything in BcacheFS
    // so this code belongs here
    debug("Look for superblock field clean");
    auto field = (const SuperBlockFieldClean *)find_superblock_field(BCH_SB_FIELD_clean);

    assert(field != nullptr);

    debug("Look for journal entry");
    auto entry = find_journal_entry(field, BCH_JSET_ENTRY_btree_root);

    assert(entry != nullptr);
    assert(entry->btree_id == BTREE_ID_extents);

    // BCacheFS_iter_next_btree_ptr
}

BCacheFSReader::~BCacheFSReader() {
    fclose(_file);
    free(_sblock);
}

// Load the superblock in 2 phases
//  - first phase reads only enough to be able to extract the full size of the superblock
//  - second phase reallocate the superblock using its full size
Superblock *BCacheFSReader::read_superblock() {
    debug(">>> Reading superblock");
    // Read the superblock at 4096 | 0x1000
    auto size = sizeof(Superblock);

    Superblock *block = (Superblock *)malloc(size);
    fseek(_file, BCH_SB_SECTOR * BCH_SECTOR_SIZE, SEEK_SET);
    fread(block, size, 1, _file);

    // Realloc the superblock now we have all the data to allocate the true size
    assert(memcmp(&block->magic, &BCACHE_MAGIC, sizeof(BCACHE_MAGIC)) == 0);
    size = sizeof(struct bch_sb) + block->u64s * BCH_U64S_SIZE;

    block = (Superblock *)realloc(block, size);
    fseek(_file, BCH_SB_SECTOR * BCH_SECTOR_SIZE, SEEK_SET);
    fread(block, size, 1, _file);

    debug("<<< Read superblock");
    return block;
}

SuperBlockFieldBase const *BCacheFSReader::find_superblock_field(SuperBlockFieldType type) const {
    auto start = (SuperBlockFieldBase const *)((const uint8_t *)_sblock + sizeof(Superblock));
    auto end   = (SuperBlockFieldBase const *)(_sblock + _sblock->u64s * BCH_U64S_SIZE);
    return find_field<SuperBlockFieldBase>(type, BCH_SB_FIELD_NR, start, end);
}

JournalSetEntry const *BCacheFSReader::find_journal_entry(SuperBlockFieldClean const *field,
                                                          JournalSetEntryType         type) const {
    auto start = (JournalSetEntry const *)((const uint8_t *)field + sizeof(SuperBlockFieldClean));
    auto end   = (JournalSetEntry const *)(field + field->field.u64s * BCH_U64S_SIZE);
    return find_field<JournalSetEntry>(type, BCH_JSET_ENTRY_NR, start, end);
}

// Look for a specific field by iterating through some memory
// the structs do not have a fixed sized
// we need to resolve their size to get to the next struct
template <typename T>
const T *find_field(uint32_t type, uint32_t type_max, T const *start, T const *end) {
    if (type == type_max) {
        return nullptr;
    }

    auto field = start;
    while (true) {
        // have we found our field
        if (field->type == type) {
            return field;
        }

        // compute next
        uint64_t u64s = field->u64s + get_spec<T>().start;
        field         = (T const *)((const uint8_t *)field + u64s * BCH_U64S_SIZE);

        debug("(size: {}) (type: {}) looking for {}", field->u64s, field->type, type);

        // make sure we still are inside our allocated memory
        if (field >= end) {
            return nullptr;
        }
    }

    // unreachable
    return nullptr;
}

template <>
struct u64s_spec get_spec<JournalSetEntry>() {
    return U64S_JSET_ENTRY;
}

template <>
struct u64s_spec get_spec<SuperBlockFieldBase>() {
    return U64S_BCH_SB_FIELD;
}
