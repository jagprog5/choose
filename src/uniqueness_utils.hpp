#pragma once

#include <cstring> // memcpy
#include <deque>
#include <list>
#include <set>
#include <unordered_set>

namespace choose {

namespace {

// used in conjunction with ForgetfulSet or ForgetfulUnorderedSet.
// this manages the least recently used logic
// if a new element is inserted into a uniqueness set, it should also be inserted here.
// upon insertion here, the least recently used element will be removed from the uniqueness set if capacity is reached.
template <typename T>
struct ForgetfulManager {
  T* obj = 0;

  // least recently used queue. back is next to be removed. front is most recently used
  std::list<typename T::iterator> elems;
  const size_t n; // elems cap

  using refresh_handle = typename decltype(elems)::iterator;

  ForgetfulManager(size_t n) : n(n == 0 ? 1 : n) {
    // given the context when this is used, n arg is never 0.
    // enforced above for safety.
    // it is required since n==0 means upon insertion of an element into the uniqueness set,
    // the element is immediately removed by this instance (in ForgetfulManager::insert).
    // meaning the returned iterator (by ForgetfulSet::insert) would be invalid
  }

  // must be called before using
  void setup(T* obj) { this->obj = obj; }

  // if an insertion into the uniqueness set failed because it already existed,
  // call this function to update the recent-ness of that element
  void refresh(refresh_handle it) {
    if (it == elems.begin()) {
      // no action is needed. refreshed element is already most recent
    } else {
      // move refreshed element to the front
      elems.splice(elems.begin(), elems, it);
    }
  }

  // call when an insertion into the uniqueness set succeeds
  void insert(typename T::iterator t) {
    elems.push_front(t);
    if (likely(elems.size() > n)) {
      obj->erase(elems.back());
      elems.pop_back();
    }
  }

  void clear() {
    obj->clear();
    elems.clear();
  }
};

} // namespace

// only remembers last n elements. least recently used forgotten
template <typename Key, typename Compare>
class ForgetfulSet {
  struct KeyInternal {
    Key key;
    // ran into some issues with circular type declarations. refresh_handle's
    // type is `typename decltype(lru)::refresh_handle`. static assertion below
    // for safety.
    void* refresh_handle;

    operator Key() const { return key; }
  };

  std::set<KeyInternal, Compare> s;
  ForgetfulManager<decltype(s)> lru;

 public:
  ForgetfulSet(const Compare& comp, size_t n)
      : s(comp), //
        lru(n) {}

  // must be called before using and after all copies or moves of this instance
  void setup() { lru.setup(&s); }

  void clear() {
    lru.clear(); // clears both
  }

  auto insert(Key k) {
    KeyInternal ki;
    ki.key = k;
    auto ret = this->s.insert(ki);

    bool insertion_success = ret.second;

    static_assert(sizeof(typename decltype(lru)::refresh_handle) == sizeof(void*));

    if (insertion_success) {
      // possibly erases oldest from s
      this->lru.insert(ret.first);

      // set ki's refresh handle now that it exists in lru
      auto refresh_handle = this->lru.elems.begin();
      void* conversion_source = &refresh_handle;
      void* conversion_destination = &const_cast<KeyInternal&>(*ret.first).refresh_handle;
      std::memcpy(conversion_destination, conversion_source, sizeof(void*));
    } else {
      // element already existed
      typename decltype(lru)::refresh_handle refresh_handle;

      void* conversion_source = &const_cast<KeyInternal&>(*ret.first).refresh_handle;
      void* conversion_destination = &refresh_handle;
      std::memcpy(conversion_destination, conversion_source, sizeof(void*));

      this->lru.refresh(refresh_handle);
    }

    return ret;
  }
};

// largely copy paste from ForgetfulSet.
template <typename Key, typename Hash, typename KeyEqual>
class ForgetfulUnorderedSet {
  struct KeyInternal {
    Key key;
    // ran into some issues with circular type declarations. refresh_handle's
    // type is `typename decltype(lru)::refresh_handle`. static assertion below
    // for safety.
    void* refresh_handle;

    operator Key() const { return key; }
  };

  std::unordered_set<KeyInternal, Hash, KeyEqual> s;
  ForgetfulManager<decltype(s)> lru;

 public:
  ForgetfulUnorderedSet(const Hash& hash, const KeyEqual key_equal, float load_factor, size_t n) : s(0, hash, key_equal), lru(n) {
    s.max_load_factor(load_factor);
    // prevent rehashing by allocating a large enough bucket size. required to
    // prevent iters invalidation. +1 since element is inserted before one is erased to maintain cap
    s.reserve(this->lru.n + 1);
  }

  // must be called before using and after all copies or moves of this instance
  void setup() { lru.setup(&s); }

  void clear() {
    lru.clear(); // clears both
  }

  auto insert(Key k) {
    KeyInternal ki;
    ki.key = k;
    auto ret = this->s.insert(ki);

    bool insertion_success = ret.second;

    static_assert(sizeof(typename decltype(lru)::refresh_handle) == sizeof(void*));

    if (insertion_success) {
      // possibly erases oldest from s
      this->lru.insert(ret.first);

      // set ki's refresh handle now that it exists in lru
      auto refresh_handle = this->lru.elems.begin();
      void* conversion_source = &refresh_handle;
      void* conversion_destination = &const_cast<KeyInternal&>(*ret.first).refresh_handle;
      std::memcpy(conversion_destination, conversion_source, sizeof(void*));
    } else {
      // element already existed
      typename decltype(lru)::refresh_handle refresh_handle;

      void* conversion_source = &const_cast<KeyInternal&>(*ret.first).refresh_handle;
      void* conversion_destination = &refresh_handle;
      std::memcpy(conversion_destination, conversion_source, sizeof(void*));

      this->lru.refresh(refresh_handle);
    }

    return ret;
  }
};

} // namespace choose