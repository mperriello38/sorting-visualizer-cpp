#pragma once

// The literal item being sorted.
//
// value:
// The number used by sorting algorithms for comparisons.
//
// id:
// Stable identity for this item. Two items can have the same value, so value alone
// is not enough to know whether a move/swap preserved the identity of each item.
// This will matter for duplicate handling, stability checks, and animation.
struct SortItem {
    int value;
    int id;
};
