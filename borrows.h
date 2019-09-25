#ifndef __BORROWS_H__
#define __BORROWS_H__

#include <optional>

// Placeholder termination function
void terminate() {
    std::cout << "DOOM\n";
    std::abort();
}

// Forward declarations

template <typename T>
class borrowed;

template <typename T>
class new_owner;

template <typename T>
class borrowed_mut;

// Using this series of classes:
//
// Every object T is owned by exactly one owned<T>.
// If you wish to pass ownership, you have a choice:
// * Move it into a new owned<T>. The memory will be moved and you can deallocate the old
//   owned<T>. However, this relies on you having a move constructor for T and can be expensive.
// * Create a new_owner<T>.
//   This is an object which then takes responsibility for the *lifetime* of the T but does not
//   move the memory. The memory still resides in the original owner<T>. As this does not move
//   the memory, this can be used with any T.

// An object owned at the current location.
// When this object goes out of scope, the object will be deallocated and no longer usable.
// If at that time the object has been borrowed, the program will terminate.
template <typename T>
class owned {
public:
  // Construct a new T in place.
  template <typename... A>
  owned(A... args) : thing(std::in_place, std::forward<A>(args)...), borrowed_mut_flag(false), borrowed_immut(0), new_owner_flag(false) {}
  // Move ownership of the T from a previous owner<T>.
  owned(owned<T>&& previous_owner) : thing(std::move(previous_owner.thing)), borrowed_mut_flag(false), borrowed_immut(0), new_owner_flag(false) {
    if (previous_owner.borrowed_mut_flag) terminate();
    if (previous_owner.borrowed_immut) terminate();
    if (previous_owner.new_owner_flag) terminate();
  }
  // Retrieve the T
  const T& operator*() {
    if (borrowed_mut_flag) terminate();
    if (borrowed_immut) terminate();
    if (!thing.has_value()) terminate();
    return *thing;
  }
  // Borrow the T immutably for temporary use elsewhere.
  borrowed<T> borrow() { return borrowed(*this); }
  // Borrow the T mutably for temporary use elsewhere.
  borrowed_mut<T> borrow_mut() { return borrowed_mut(*this); }
  ~owned() {
    // Avoid UaFs.
    if (borrowed_immut) terminate();
    if (borrowed_mut_flag) terminate();
    if (new_owner_flag) terminate();
  }
private:
  std::optional<T> thing;
  bool borrowed_mut_flag;
  bool new_owner_flag; // some new_owner object owns its lifetime
  size_t borrowed_immut;

  friend class new_owner<T>;
  friend class borrowed<T>;
  friend class borrowed_mut<T>;
};

// Represents a locus in the code which controls the lifetime of a T but
// does not actually store the memory for it (that's still within an owned<T>
// somewhere).
template <typename T>
class new_owner {
public:
  new_owner(owned<T>&& thing) : original(thing), has_ownership(true) {
    if (original.borrowed_mut_flag) terminate();
    if (original.borrowed_immut) terminate();
    if (!original.thing.has_value()) terminate();
    original.new_owner_flag = true;
  }
  new_owner(new_owner<T>&& thing) : original(thing.thing), has_ownership(true) {
    thing.has_ownership = false;
  }
  ~new_owner() {
    original.new_owner_flag = false;
    original.thing.reset();
  }
  const T& operator*() {
    if (!has_ownership) terminate();
    return *original;
  }
  borrowed<T> borrow() { return original.borrowed(*this); }
  borrowed_mut<T> borrow_mut() { return original.borrowed_mut(*this); }

private:
  owned<T>& original;
  bool has_ownership;

  friend class borrowed<T>;
  friend class borrowed_mut<T>;
}; 

// An immutable borrow of a T.
template <typename T>
class borrowed {
public:
  borrowed(owned<T>& thing) : original(thing) {
    if (original.borrowed_mut_flag) terminate();
    original.borrowed_immut++;
  }
  borrowed(new_owner<T>& thing) : original(thing.original) {
    if (original.borrowed_mut_flag) terminate();
    original.borrowed_immut++;
  }
  borrowed(borrowed<T>& thing) : original(thing.original) {
    if (original.borrowed_mut_flag) terminate();
    original.borrowed_immut++;
  }
  borrowed(borrowed<T>&& thing) : original(thing.original) {
    if (original.borrowed_mut_flag) terminate();
    original.borrowed_immut++;
  }
  const T& operator*() {
    if (original.borrowed_mut_flag) terminate();
    if (!original.thing.has_value()) terminate();
    return *original.thing;
  }
  ~borrowed() {
    original.borrowed_immut--;
  }
private:
  owned<T>& original;
};


// A mutable borrow of a T.
template <typename T>
class borrowed_mut {
public:
  borrowed_mut(owned<T>& thing) : original(thing), valid(true) {
    if (original.borrowed_mut_flag || original.borrowed_immut) terminate();
    original.borrowed_mut_flag = true;
  }
  borrowed_mut(new_owner<T>& thing) : original(thing.original), valid(true) {
    if (original.borrowed_mut_flag || original.borrowed_immut) terminate();
    original.borrowed_mut_flag = true;
  }
  borrowed_mut(borrowed_mut<T>&& thing) : original(std::move(thing.original)), valid(true) {
    thing.valid = false;
  }
  T& operator*() {
    if (!valid) terminate();
    if (!original.thing.has_value()) terminate();
    return *original.thing;
    }
  ~borrowed_mut() {
    original.borrowed_mut_flag = false;
    }
private:
  owned<T>& original;
  bool valid; // starts true
};

#endif
