#include <iostream>
#include <string>
#include <optional>

void terminate() {
    std::cout << "DOOM\n";
    std::abort();
}

template <typename T>
class borrowed;

template <typename T>
class new_owner;

template <typename T>
class borrowed_mut;

template <typename T>
class owned {
public:
    template <typename... A>
    owned(A... args) : thing(std::in_place, std::forward<A>(args)...), borrowed_mut_flag(false), borrowed_immut(0) {}
	const T& operator*() {
        if (borrowed_mut_flag) terminate();
        if (borrowed_immut) terminate();
        if (!thing.has_value()) terminate();
        return *thing;
    }
	borrowed<T> borrow() { return borrowed(*this); }
	borrowed_mut<T> borrow_mut() { return borrowed_mut(*this); }
    ~owned() {
        // Avoid UaFs.
        if (borrowed_immut) terminate();
        if (borrowed_mut_flag) terminate();
    }
private:
	std::optional<T> thing;
	bool borrowed_mut_flag;
	size_t borrowed_immut;

    friend class new_owner<T>;
    friend class borrowed<T>;
    friend class borrowed_mut<T>;
};


template <typename T>
class new_owner {
public:
	new_owner(owned<T>&& thing) : original(thing) {
        if (original.borrowed_mut_flag) terminate();
        if (original.borrowed_immut) terminate();
        if (!original.thing.has_value()) terminate();
    }
    ~new_owner() {
        original.thing.reset();
    }
    const T& operator*() {
        return *original;
    }
	borrowed<T> borrow() { return original.borrowed(*this); }
	borrowed_mut<T> borrow_mut() { return original.borrowed_mut(*this); }

private:
	owned<T>& original;

    friend class borrowed<T>;
    friend class borrowed_mut<T>;
}; 


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

void handle_owned(new_owner<Example> another) {
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
