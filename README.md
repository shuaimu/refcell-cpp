## borrow-cpp: a Rust-style borrow checker for C++, with (partial) static check 

### How-to-use

```cpp
#include "borrow.h"
using namespace borrow;

int main() {
  own_ptr<int> owner;
  owner.reset(new int(5));
  {
    auto a = borrow_mut(owner); // mutable ptr
    // auto b = borrow_mut(owner); // wrong! only one mutable ptr at a time 
  } // a's lifetime ends
  {
    auto c = borrow_ref(owner); // non-mutable (read-only) ptr
    auto d = borrow_ref(owner); // okay, multiple read-only ptrs are allowed
    // auto e = borrow_mut(owner); // wrong! mutable ptr is disallowed when there is a read-only ptr
  }
  return 0;
}
```

### Runtime check
A nullptr dereference is triggered (hence a seg fault) when violations happen. You can change the behavior by rewrite the `borrow_verify` macro. 

### Compile-time check

Using Facebook's [Infer](https://fbinfer.com/) which has implemented a Rust-like memory lifetime check, the violations of borrow rules have a good chance to be caught at compile time (static analysis). For example, the header contains a few violations tests which can be found by Infer using this command: 

```bash
infer run --pulse-only -- clang++ -x c++ -std=c++11 -O0 borrow.h -D BORROW_TEST=1
```

Output:
```
borrow.h:175: error: Nullptr Dereference
  Pulse found a potential null pointer dereference  on line 105 indirectly during the call to `borrow::borrow_mut`in call to `borrow::borrow_mut` .
  173.   owner.reset(new int(5));
  174.   auto x = borrow_mut(owner);
  175.   auto y = borrow_mut(owner);
                  ^
  176. }
  177.

borrow.h:186: error: Nullptr Dereference
  Pulse found a potential null pointer dereference  on line 105 indirectly during the call to `borrow::borrow_mut`in call to `borrow::borrow_mut` .
  184.   auto y = borrow_mut(owner);
  185.   {
  186.     auto z = borrow_mut(owner);
                    ^
  187.   }
  188. }

borrow.h:195: error: Nullptr Dereference
  Pulse found a potential null pointer dereference  on line 105 indirectly during the call to `borrow::borrow_mut`in call to `borrow::borrow_mut` .
  193.   auto x = borrow_ref(owner);
  194.   auto y = borrow_ref(owner);
  195.   auto z = borrow_mut(owner);
                  ^
  196. }
  197.
```

### TODO
* thread-safety
* performance mode (reduce to raw pointers)
