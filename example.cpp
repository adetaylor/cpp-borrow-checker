#include <iostream>
#include <string>
#include "borrows.h"

// Example code below.

class Example {
public:
  Example(int _id) : id(_id) { std::cout << "Example constructor " << id << "\n"; };
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

// TODO make a ton of individual tests.

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
    std::cout << "Next line should crash because it would otherwise allow UaF\n";
  }

  return 0;
}
