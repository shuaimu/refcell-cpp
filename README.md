## borrow-cpp: a Rust-style borrow checker for C++, with (partial) static check 

### How-to-use

```cpp
#include "borrow.h"
using namespace borrow;

int main() {
  RefCell<int> owner;
  owner.reset(new int(5));
  {
    RefMut<int> a = borrow_mut(owner); // mutable ptr
    // RefMut<int> b = borrow_mut(owner); // wrong! only one mutable ptr at a time 
  } // a's lifetime ends
  {
    Ref<int> c = borrow_const(owner); // const (non-mutable) ptr
    Ref<int> d = borrow_const(owner); // okay, multiple const ptrs are allowed
    // RefMut<int> e = borrow_mut(owner); // wrong! mutable ptr is disallowed when there is a const ptr
  }
  return 0;
}
```

### Runtime check
An abort is triggered when violations happen. You can change the behavior by rewrite the `borrow_verify` macro. 

### Compile-time check

Using Facebook's [Infer](https://fbinfer.com/) which has implemented a Rust-like memory lifetime check, the violations of borrow rules have a good chance to be caught at compile time (static analysis). For example, the header contains a few violations tests which can be found by Infer using this command: 

```bash
$ infer run --pulse-only -- clang++ -x c++ -std=c++11 -O0 borrow.h -D BORROW_TEST=1 -D BORROW_INFER_CHECK=1
```

Output:
```
borrow.h:179: error: Nullptr Dereference
  Pulse found a potential null pointer dereference  on line 109 indirectly during the call to `borrow::borrow_mut`in call to `borrow::borrow_mut` .
  177.   owner.reset(new int(5));
  178.   auto x = borrow_mut(owner);
  179.   auto y = borrow_mut(owner);
                  ^
  180. }
  181.

borrow.h:190: error: Nullptr Dereference
  Pulse found a potential null pointer dereference  on line 109 indirectly during the call to `borrow::borrow_mut`in call to `borrow::borrow_mut` .
  188.   auto y = borrow_mut(owner);
  189.   {
  190.     auto z = borrow_mut(owner);
                    ^
  191.   }
  192. }

borrow.h:199: error: Nullptr Dereference
  Pulse found a potential null pointer dereference  on line 109 indirectly during the call to `borrow::borrow_mut`in call to `borrow::borrow_mut` .
  197.   auto x = borrow_const(owner);
  198.   auto y = borrow_const(owner);
  199.   auto z = borrow_mut(owner);
                  ^
  200. }
  201.


Found 3 issues
                Issue Type(ISSUED_TYPE_ID): #
  Nullptr Dereference(NULLPTR_DEREFERENCE): 3
```

Note that when static analysis is enabled (BORROW_INFER_CHECK), a nullptr dereference is triggered in the `borrow_verify`, because we are relying on the nullptr dereference checking in Infer. However, a nullptr dereference is an undefined behavior and can cause unexpected results with compiler optimizations. Therefore, when compiling for release version, the static analysis flags should be turned off.   

### TODO
* thread-safety
* performance mode (reduce to raw pointers)
