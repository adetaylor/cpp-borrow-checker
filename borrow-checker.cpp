#include <iostream>
#include <string>
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
//   This is an object which then takes responsibility for the lifetime of the T but does not
//   copy the memory. The memory still resides in the original owner<T>.

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


class Example {
public:
    Example(int _id) : id(_id) { std::cout << "Example constructor << " << id << "\n"; };
    ~Example() { std::cout << "Example destructor " << id << "\n"; };
    void change() { std::cout << "Example mutating " << id << "\n"; }
    friend std::ostream& operator<<(std::ostream& os, const Example& example);
private:
    int id;
};

class Example2 {
public:
    ~Example2() { std::cout << "Example2 destructor\n"; };
    void change() { std::cout << "Example2 mutating\n"; }
    friend std::ostream& operator<<(std::ostream& os, const Example2& example);
    // TODO: work out how to make a class not constructible except
    // via owned<T>
    //friend class std::optional<Example2>;
    //friend class std::is_constructible<Example2>;
//private:
    Example2() { std::cout << "Example2 constructor\n"; };
};

std::ostream& operator<<(std::ostream& os, const Example& e)
{
    os << "Example " << e.id << "\n";
    return os;
}
std::ostream& operator<<(std::ostream& os, const Example2& e)
{
    os << "Example2\n";
    return os;
}

void handle_borrowed_mut(borrowed_mut<Example> another) {
    std::cout << "Borrowed_mut: " << *another;
    (*another).change();
}

void handle_borrowed(borrowed<Example> another) {
    std::cout << "Borrowed: " << *another;
}

void handle_borrowed_nested(borrowed<Example> another) {
    handle_borrowed(another);
}

void handle_owned(owned<Example> another) {
    std::cout << "Owned: " << *another;
    handle_borrowed(another);
    std::cout << "Still owned: " << *another;
}

struct MoreStuff {
    std::unique_ptr<borrowed<Example>> example_borrow;
};

int main(void) {
    owned<Example> original(1);
    std::cout << *original;
    handle_borrowed(original);
    handle_borrowed_nested(original);
    (*(original.borrow_mut())).change();
    handle_borrowed_mut(original);
    handle_owned(std::move(original));

    std::cout << "Next line should crash\n";
    //std::cout << *original;

    MoreStuff more;
    {
        owned<Example> foo(2);
        more.example_borrow = std::make_unique<borrowed<Example>>(foo.borrow());
        std::cout << "Next line should crash\n";
    }

    return 0;
}
