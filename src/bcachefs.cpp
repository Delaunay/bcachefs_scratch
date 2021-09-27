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
struct FieldIterator {

    template <typename U>
    FieldIterator(U c): current((T *)c) {}

    bool operator!=(FieldIterator const &b) const { return current != b.current; }

    T *operator->() { return current; }

    T *operator*() { return current; }

    FieldIterator &operator++() {
        next();
        return *this;
    }

    inline void next() {
        uint64_t u64s = current->u64s + get_spec<T>().start;
        current       = (T *)((const uint8_t *)current + u64s * BCH_U64S_SIZE);
    }

    T *current;
};

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
    // on my example I have the fields below
    // - journal
    // - replicas_v0
    // - clean
    // - last one look like a dummy struct
    auto field = (const SuperBlockFieldClean *)find_superblock_field(BCH_SB_FIELD_clean);

    assert(field != nullptr);

    debug("Look for journal entry");
    // on my example I have the entries below
    // - usage
    // - usage
    // - usage
    // - usage
    // - usage
    // - data_usage
    // - data_usage
    // - deb_usage
    // - clock
    // - clock
    // - btree_root extents
    // - btree_root inodes
    // - btree_root dirents
    // - btree_root alloc
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
    auto iter = FieldIterator<SuperBlockFieldBase>((const uint8_t *)_sblock + sizeof(Superblock));
    auto end  = FieldIterator<SuperBlockFieldBase>(_sblock + _sblock->u64s * BCH_U64S_SIZE);

    for (; iter != end; ++iter) {
        debug("(size: {}) (type: {}) looking for {}", iter->u64s, iter->type, type);

        if (iter->type == type) {
            return *iter;
        }
    }

    return nullptr;
}

JournalSetEntry const *BCacheFSReader::find_journal_entry(SuperBlockFieldClean const *field,
                                                          JournalSetEntryType         type) const {

    auto iter = FieldIterator<JournalSetEntry>((const uint8_t *)field + sizeof(SuperBlockFieldClean));
    auto end  = FieldIterator<JournalSetEntry>(field + field->field.u64s * BCH_U64S_SIZE);

    for (; iter != end; ++iter) {
        debug("(size: {}) (type: {}) looking for {}", iter->u64s, iter->type, type);

        if (iter->type == type) {
            return *iter;
        }
    }

    return nullptr;
}

Array<JournalSetEntry const *> BCacheFSReader::find_btree_roots(SuperBlockFieldClean const *field) const {

    auto iter = FieldIterator<JournalSetEntry>((const uint8_t *)field + sizeof(SuperBlockFieldClean));
    auto end  = FieldIterator<JournalSetEntry>(field + field->field.u64s * BCH_U64S_SIZE);

    Array<JournalSetEntry const *> out;
    auto                           type = BCH_JSET_ENTRY_btree_root;

    for (; iter != end; ++iter) {
        debug("(size: {}) (type: {}) looking for {}", iter->u64s, iter->type, type);

        if (iter->type == type) {
            out.push_back(*iter);
        }
    }

    return out;
}

template <>
struct u64s_spec get_spec<JournalSetEntry>() {
    return U64S_JSET_ENTRY;
}

template <>
struct u64s_spec get_spec<SuperBlockFieldBase>() {
    return U64S_BCH_SB_FIELD;
}
