/* Include Guard */
#ifndef INCLUDE_BENZINA_BCACHEFS_READER_H
#define INCLUDE_BENZINA_BCACHEFS_READER_H

#include "cbcachefs.h"
#include "logger.h"

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#define INT(x) ((uint64_t)(x))

template <typename V>
using Array  = std::vector<V>;
using String = std::string;

using Superblock = struct bch_sb;

using SuperBlockFieldType  = enum bch_sb_field_type;
using SuperBlockFieldBase  = struct bch_sb_field;
using SuperBlockFieldClean = struct bch_sb_field_clean;

using JournalSetEntryType = enum bch_jset_entry_type;
using JournalSetEntry     = struct jset_entry;
using BTreeType           = enum btree_id;

using BTreePtr = struct bch_btree_ptr_v2;
using BKeyType = enum bch_bkey_type;
using BKey     = struct bkey;
using BValue   = struct bch_val;
using BDirEnt  = struct bch_dirent;

using BTreeNode = struct btree_node;
using BSet      = struct bset;

inline uint64_t extract_bitflag(const uint64_t bitfield, uint8_t first_bit, uint8_t last_bit) {
    return bitfield << (sizeof(bitfield) * 8 - last_bit) >> (sizeof(bitfield) * 8 - last_bit + first_bit);
}

struct BTreeIterator;

struct BCacheFSReader {
    public:
    BCacheFSReader(String const &file);

    ~BCacheFSReader();

    BTreeIterator iterator(BTreeType type) const;

    private:
    // Load the superblock in 2 phases
    //  - first phase reads only enough to be able to extract the full size of the superblock
    //  - second phase reallocate the superblock using its full size
    Superblock *read_superblock();

    SuperBlockFieldBase const *find_superblock_field(SuperBlockFieldType type) const;

    Array<JournalSetEntry const *> find_journal_entries(SuperBlockFieldClean const *field) const;

    BTreePtr const *find_btree_root(JournalSetEntry const *entry) const;

    public:
    // extract the size of a btree node
    uint64_t btree_node_size() const {
        return (uint64_t)(uint16_t)extract_bitflag(_sblock->flags[0], 12, 28) * BCH_SECTOR_SIZE;
    }

    uint64_t btree_block_size() const { return (uint64_t)_sblock->block_size * BCH_SECTOR_SIZE; }

    public:
    FILE *                         _file   = nullptr;
    Superblock *                   _sblock = nullptr;
    Array<JournalSetEntry const *> _btree_roots;

    friend struct BTreeIterator;
};

// Lookup the size spec given a type
template <typename T>
struct u64s_spec get_spec() {
    struct u64s_spec spec;
    return spec;
}

template <typename T>
uint64_t get_u64s(T *ptr) {
    using U = std::remove_const<T>::type;
    return ptr->u64s + get_spec<U>().start;
}

// Specialization
template <>
inline struct u64s_spec get_spec<JournalSetEntry>() {
    return U64S_JSET_ENTRY;
}

template <>
inline struct u64s_spec get_spec<SuperBlockFieldBase>() {
    return U64S_BCH_SB_FIELD;
}

template <>
inline struct u64s_spec get_spec<BKey>() {
    return U64S_BKEY;
}

template <>
inline uint64_t get_u64s(BTreePtr *ptr) {
    return sizeof(BTreePtr);
}
// -------------------------

template <typename T>
struct FieldIterator {
    FieldIterator(uint8_t const *c): current(c) {}

    FieldIterator() {}

    bool operator!=(FieldIterator const &b) const { return current != b.current; }

    bool operator==(FieldIterator const &b) const { return current == b.current; }

    bool operator<(FieldIterator const &b) const { return current < b.current; }

    bool operator>=(FieldIterator const &b) const { return current >= b.current; }

    T *operator->() { return (T *)current; }

    T *operator*() { return (T *)current; }

    FieldIterator &operator++() {
        next();
        return *this;
    }

    inline void next() {
        // The the size of the struct and move past it
        uint64_t u64s = get_u64s<T>((T *)current);
        current       = (current + u64s * BCH_U64S_SIZE);
    }

    uint8_t const *current = nullptr;
};

BValue const *get_value(BTreeNode const *node, const BKey *key);

// Iterates over all the BKeys inside a BSet
struct BKeyIterator {
    BKeyIterator() {}
    BKeyIterator(BSet const *bset);

    BKey const *next();

    FieldIterator<BKey const> iter;
    FieldIterator<BKey const> end;
};

// Iterates over all the BSet inside a BNode
struct BSetIterator {
    BSetIterator() {}
    explicit BSetIterator(BTreeNode const *node, uint64_t size);

    uint8_t const *offset(uint64_t block_size);

    BSet const *next(uint64_t block_size);

    FieldIterator<BSet const> iter;
    FieldIterator<BSet const> end;
    BTreeNode const *         node;
};

struct DirectoryEntry {
    uint64_t       parent_inode;
    uint64_t       inode;
    uint8_t        type;
    const uint8_t *name;
};

inline std::ostream &operator<<(std::ostream &out, DirectoryEntry const &dir) {
    out << dir.parent_inode << " ";
    out << dir.inode << " ";
    out << dir.type << " ";
    return out << (const char *)dir.name;
}

// Btree
//   Node - Chunk in the FS (fread - file) | BCacheFS_next_iter (read next node)
//      Set BKey + BValue   benz_bch_next_bkey + _BCacheFS_iter_next_bch_val
//      Set BKey + BValue
//      ...
//      Set BKey + BValue
//   Node - Chunk in the FS (fread - file)
//
struct BTreeIterator {
    public:
    BTreeIterator(BCacheFSReader const &reader, const BTreePtr *root_ptr, BTreeType type);

    ~BTreeIterator() {}

    BValue const *next() { return next_value(); }

    BKey const *next_key() { return _next_key(); }

    DirectoryEntry directory(BKey const *key);

    private:
    BValue const *next_value() {
        auto key = _next_key();
        return get_value(_iter.get(), key);
    }

    BKey const *_next_key();

    std::shared_ptr<BTreeNode> load_btree_node(BTreePtr const *ptr);

    bool has_children() const { return _children.size() > 0; }

    // get top level iterator
    BTreeIterator &iterator() {
        if (has_children()) {
            return _children[_children.size()];
        }
        return *this;
    }

    private:
    BCacheFSReader const &_reader;
    BTreeType const       _type;

    // Memory we need to read a node
    // we allocate one that we reuse for the different node we traverse
    std::shared_ptr<BTreeNode> _iter = nullptr;

    // Iterators
    BSetIterator         _bset_iterator;
    BKeyIterator         _key_iterator;
    Array<BTreeIterator> _children;
};

#endif