// $Id$

/***********************************************************************
Moses - factored phrase-based language decoder
Copyright (C) 2006 University of Edinburgh

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
***********************************************************************/

#ifndef moses_WordsBitmap_h
#define moses_WordsBitmap_h

#include <algorithm>
#include <limits>
#include <vector>
#include <iostream>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include "TypeDef.h"
#include "WordsRange.h"

namespace Moses
{
typedef unsigned long WordsBitmapID;

/** Vector of boolean to represent whether a word has been translated or not.
 *
 * Implemented using a vector of char, which is usually the same representation
 * for the elements that a C array of bool would use.  A vector of bool, or a
 * Boost dynamic_bitset, could be much more efficient in theory.  Unfortunately
 * algorithms like std::find() are not optimized for vector<bool> on gcc or
 * clang, and dynamic_bitset lacks all the optimized search operations we want.
 * Only benchmarking will tell what works best.  Perhaps dynamic_bitset could
 * still be a dramatic improvement, if we flip the meaning of the bits around
 * so we can use its find_first() and find_next() for the most common searches.
 */
class WordsBitmap
{
  friend std::ostream& operator<<(std::ostream& out, const WordsBitmap& wordsBitmap);
private:
  std::vector<char> m_bitmap; //! Ticks of words in sentence that have been done.
  size_t m_firstGap; //! Cached position of first gap, or NOT_FOUND.

  WordsBitmap(); // not implemented
  WordsBitmap& operator= (const WordsBitmap& other);

  /** Update the first gap, when bits are flipped */
  void UpdateFirstGap(size_t startPos, size_t endPos, bool value) {
    if (value) {
      //may remove gap
      if (startPos <= m_firstGap && m_firstGap <= endPos) {
        m_firstGap = NOT_FOUND;
        for (size_t i = endPos + 1 ; i < m_bitmap.size(); ++i) {
          if (!m_bitmap[i]) {
            m_firstGap = i;
            break;
          }
        }
      }

    } else {
      //setting positions to false, may add new gap
      if (startPos < m_firstGap) {
        m_firstGap = startPos;
      }
    }
  }


public:
  //! Create WordsBitmap of length size, and initialise with vector.
  WordsBitmap(size_t size, const std::vector<bool>& initializer)
    :m_bitmap(initializer.begin(), initializer.end()), m_firstGap(0) {

    // The initializer may not be of the same length.  Change to the desired
    // length.  If we need to add any elements, initialize them to false.
    m_bitmap.resize(size, false);

    // Find the first gap, and cache it.
    std::vector<char>::const_iterator first_gap = std::find(
          m_bitmap.begin(), m_bitmap.end(), false);
    m_firstGap = (
                   (first_gap == m_bitmap.end()) ?
                   NOT_FOUND : first_gap - m_bitmap.begin());
  }

  //! Create WordsBitmap of length size and initialise.
  WordsBitmap(size_t size)
    :m_bitmap(size, false), m_firstGap(0) {
  }

  //! Deep copy.
  WordsBitmap(const WordsBitmap &copy)
    :m_bitmap(copy.m_bitmap), m_firstGap(copy.m_firstGap) {
  }

  //! Count of words translated.
  size_t GetNumWordsCovered() const {
    return std::count(m_bitmap.begin(), m_bitmap.end(), true);
  }

  //! position of 1st word not yet translated, or NOT_FOUND if everything already translated
  size_t GetFirstGapPos() const {
    return m_firstGap;
  }


  //! position of last word not yet translated, or NOT_FOUND if everything already translated
  size_t GetLastGapPos() const {
    for (int pos = int(m_bitmap.size()) - 1 ; pos >= 0 ; pos--) {
      if (!m_bitmap[pos]) {
        return pos;
      }
    }
    // no starting pos
    return NOT_FOUND;
  }


  //! position of last translated word
  size_t GetLastPos() const {
    for (int pos = int(m_bitmap.size()) - 1 ; pos >= 0 ; pos--) {
      if (m_bitmap[pos]) {
        return pos;
      }
    }
    // no starting pos
    return NOT_FOUND;
  }

  bool IsAdjacent(size_t startPos, size_t endPos) const;

  //! whether a word has been translated at a particular position
  bool GetValue(size_t pos) const {
    return bool(m_bitmap[pos]);
  }
  //! set value at a particular position
  void SetValue( size_t pos, bool value ) {
    m_bitmap[pos] = value;
    UpdateFirstGap(pos, pos, value);
  }
  //! set value between 2 positions, inclusive
  void
  SetValue( size_t startPos, size_t endPos, bool value ) {
    for(size_t pos = startPos ; pos <= endPos ; pos++) {
      m_bitmap[pos] = value;
    }
    UpdateFirstGap(startPos, endPos, value);
  }

  void
  SetValue(WordsRange const& range, bool val) {
    SetValue(range.GetStartPos(), range.GetEndPos(), val);
  }

  //! whether every word has been translated
  bool IsComplete() const {
    return GetSize() == GetNumWordsCovered();
  }
  //! whether the wordrange overlaps with any translated word in this bitmap
  bool Overlap(const WordsRange &compare) const {
    for (size_t pos = compare.GetStartPos() ; pos <= compare.GetEndPos() ; pos++) {
      if (m_bitmap[pos])
        return true;
    }
    return false;
  }
  //! number of elements
  size_t GetSize() const {
    return m_bitmap.size();
  }

  //! transitive comparison of WordsBitmap
  inline int Compare (const WordsBitmap &compare) const {
    // -1 = less than
    // +1 = more than
    // 0	= same

    size_t thisSize = GetSize()
                      ,compareSize = compare.GetSize();

    if (thisSize != compareSize) {
      return (thisSize < compareSize) ? -1 : 1;
    }
    return std::memcmp(
             &m_bitmap[0], &compare.m_bitmap[0], thisSize * sizeof(bool));
  }

  bool operator< (const WordsBitmap &compare) const {
    return Compare(compare) < 0;
  }

  inline size_t GetEdgeToTheLeftOf(size_t l) const {
    if (l == 0) return l;
    while (l && !m_bitmap[l-1]) {
      --l;
    }
    return l;
  }

  inline size_t GetEdgeToTheRightOf(size_t r) const {
    if (r+1 == m_bitmap.size()) return r;
    return (
             std::find(m_bitmap.begin() + r + 1, m_bitmap.end(), true) -
             m_bitmap.begin()
           ) - 1;
  }


  //! converts bitmap into an integer ID: it consists of two parts: the first 16 bit are the pattern between the first gap and the last word-1, the second 16 bit are the number of filled positions. enforces a sentence length limit of 65535 and a max distortion of 16
  WordsBitmapID GetID() const {
    assert(m_bitmap.size() < (1<<16));

    size_t start = GetFirstGapPos();
    if (start == NOT_FOUND) start = m_bitmap.size(); // nothing left

    size_t end = GetLastPos();
    if (end == NOT_FOUND) end = 0; // nothing translated yet

    assert(end < start || end-start <= 16);
    WordsBitmapID id = 0;
    for(size_t pos = end; pos > start; pos--) {
      id = id*2 + (int) GetValue(pos);
    }
    return id + (1<<16) * start;
  }

  //! converts bitmap into an integer ID, with an additional span covered
  WordsBitmapID GetIDPlus( size_t startPos, size_t endPos ) const {
    assert(m_bitmap.size() < (1<<16));

    size_t start = GetFirstGapPos();
    if (start == NOT_FOUND) start = m_bitmap.size(); // nothing left

    size_t end = GetLastPos();
    if (end == NOT_FOUND) end = 0; // nothing translated yet

    if (start == startPos) start = endPos+1;
    if (end < endPos) end = endPos;

    assert(end < start || end-start <= 16);
    WordsBitmapID id = 0;
    for(size_t pos = end; pos > start; pos--) {
      id = id*2;
      if (GetValue(pos) || (startPos<=pos && pos<=endPos))
        id++;
    }
    return id + (1<<16) * start;
  }

  TO_STRING();
};

// friend
inline std::ostream& operator<<(std::ostream& out, const WordsBitmap& wordsBitmap)
{
  for (size_t i = 0 ; i < wordsBitmap.m_bitmap.size() ; i++) {
    out << int(wordsBitmap.GetValue(i));
  }
  return out;
}

}
#endif
