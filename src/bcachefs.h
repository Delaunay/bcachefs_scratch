/* Include Guard */
#ifndef INCLUDE_BENZINA_BCACHEFS_READER_H
#define INCLUDE_BENZINA_BCACHEFS_READER_H

#include "cbcachefs.h"
#include "logger.h"

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#define INT(x) (*(uint64_t *)(x))

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
using BTreeValue          = struct bch_val;
using BTreePtr            = struct bch_btree_ptr_v2;

using BKeyType = enum bch_bkey_type;
using BKey     = struct bkey;

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

    template <typename U>
    FieldIterator(U c): current((T *)c) {}

    FieldIterator() {}

    bool operator!=(FieldIterator const &b) const { return current != b.current; }

    bool operator==(FieldIterator const &b) const { return current == b.current; }

    bool operator<(FieldIterator const &b) const { return current < b.current; }

    T *operator->() { return current; }

    T *operator*() { return current; }

    FieldIterator &operator++() {
        next();
        return *this;
    }

    inline void next() {
        // The the size of the struct and move past it
        uint64_t u64s = get_u64s<T>(current);
        current       = (T *)((const uint8_t *)current + u64s * BCH_U64S_SIZE);
    }

    T *current = nullptr;
};

BTreeValue const *get_value(BTreeNode const *node, const BKey *key);

struct BTreeIterator {

    BTreeIterator(BCacheFSReader const &reader, const BTreePtr *root_ptr, BTreeType type);

    ~BTreeIterator() {}

    BTreeValue const *next() { return next_value(); }

    BKey const *next_key() { return _next_key(); }

    private:
    // Btree
    //   Node - Chunk in the FS (fread - file) | BCacheFS_next_iter (read next node)
    //      Set BKey + BValue   benz_bch_next_bkey + _BCacheFS_iter_next_bch_val
    //      Set BKey + BValue
    //      ...
    //      Set BKey + BValue
    //   Node - Chunk in the FS (fread - file)
    //      ..
    BSet const *find_bset(BTreeNode const *ptr);

    BTreeValue const *next_value() {
        auto key = _next_key();
        return get_value(_iter.get(), key);
    }

    BKey const *_next_key();

    std::shared_ptr<BTreeNode> load_btree_node(BTreePtr const *ptr);

    // Iterates over all the BSet inside a BNode
    struct BSetIterator {
        BSetIterator() {}
        BSetIterator(BTreeNode const *node, uint64_t size):
            // node + sizeof(BTreeNode)
            // &node->keys
            iter((BSet const *)(&node->keys)), end((BSet const *)(node + size)), node(node) {}

        BSet const *offset(uint64_t block_size) {
            BSet const *v = *iter;

            const uint8_t *_cb = (const uint8_t *)v;

            _cb -= (uint64_t)node;

            // standard next
            _cb += sizeof(*v) + v->u64s * BCH_U64S_SIZE;

            _cb += block_size - (uint64_t)_cb % block_size +
                   // skip btree_node_entry csum
                   sizeof(struct bch_csum);

            _cb += (uint64_t)node;
            return (BSet const *)_cb;
        }

        BSet const *next(uint64_t block_size) {
            if (iter == end) {
                info("Bset is finished");
                return nullptr;
            }

            auto val     = *iter;
            iter.current = offset(block_size);
            return val;
        }

        FieldIterator<BSet const> iter;
        FieldIterator<BSet const> end;
        BTreeNode const *         node;
    };

    // Iterates over all the BKeys inside a BSet
    struct BKeyIterator {
        BKeyIterator() {}
        BKeyIterator(BTreeNode const *node, BSet const *bset):
            iter((BKey const *)(bset + sizeof(*bset))), end((BKey const *)(bset + bset->u64s * BCH_U64S_SIZE)) {}

        BKey const *next() {
            if (iter == nullptr || iter == end) {
                debug("Iterator is finished");
                return nullptr;
            }

            auto val = *iter;
            ++iter;

            debug("{} {} {}", INT(iter.current), INT(end.current), INT(val));
            return val;
        }

        FieldIterator<BKey const> iter;
        FieldIterator<BKey const> end;
    };

    bool has_children() const { return _children.size() > 0; }

    BCacheFSReader const &_reader;
    BTreeType const       _type;
    const BTreePtr *      _ptr = nullptr;

    // Memory we need to read a node
    // we allocate one that
    std::shared_ptr<BTreeNode> _iter = nullptr;

    // Iterators
    BSetIterator         _bset_iterator;
    BKeyIterator         _key_iterator;
    Array<BTreeIterator> _children;

    // Current values
    BSet const *      _bset  = nullptr;
    BKey const *      _key   = nullptr;
    BTreeValue const *_value = nullptr;
};

#endif