#include <cstdint>
#include <memory>
#include <utility>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <csignal>
#include <execinfo.h>
#include <iostream>
namespace borrow {

// Macros for custom error handling
#define PRINT_STACK_TRACE() \
    do { \
        void* buffer[30]; \
        int size = backtrace(buffer, 30); \
        char** symbols = backtrace_symbols(buffer, size); \
        if (symbols == nullptr) { \
            std::cerr << "Failed to obtain stack trace." << std::endl; \
            break; \
        } \
        std::cerr << "Stack trace:" << std::endl; \
        for (int i = 0; i < size; ++i) { \
            std::cerr << symbols[i] << std::endl; \
        } \
        free(symbols); \
    } while(0)

#ifndef borrow_verify
#ifdef BORROW_INFER_CHECK
#define borrow_verify(x) {if (!(x)) {volatile int* a = nullptr ; *a;}}
//#define borrow_verify(x) nullptr
#else
#define borrow_verify(x, errmsg) \
    do { \
        if (!(x)) { \
            fprintf(stderr, errmsg); \
            PRINT_STACK_TRACE(); \
            std::abort(); \
        } \
    } while(0)
  
#endif
#endif



template<class T>
class Ref {
 public:
  const T* raw_{nullptr};
  std::atomic<int32_t>* p_cnt_{nullptr};
  Ref() = default;
  Ref(const Ref&) = delete;
  Ref(Ref&& p) {
    raw_ = p.raw_; 
    p_cnt_ = p.p_cnt_;
    p.raw_ = nullptr;
    p.p_cnt_ = nullptr;
  };
  Ref(Ref& p) {
    raw_ = p.raw_;
    p_cnt_ = p.p_cnt_;
    auto i = (*p_cnt_)++;
    borrow_verify(i > 0, "error in Ref constructor");
  }
  const T* operator->() {
    return raw_;
  }
  void reset() {
    auto i = (*p_cnt_)--;
    borrow_verify(i > 0, "Trying to reset null pointer");
    raw_ = nullptr;
    p_cnt_ = nullptr;
  }
  ~Ref() {
    if (p_cnt_ != nullptr) {
      auto i = (*p_cnt_)--;
      borrow_verify(i > 0, "Trying to dereference null pointer"); // failure means - count became negative which is not possible
    }
  }
};

template <typename T>
class RefMut {
 public:
  T* raw_;
  std::atomic<int32_t>* p_cnt_;
  RefMut() = default;
  RefMut(const RefMut&) = delete;
  RefMut(RefMut&& p) : raw_(p.raw_), p_cnt_(p.p_cnt_) {
    raw_ = p.raw_;
    p.p_cnt_ = nullptr;
    p.raw_ = nullptr;
  }
  T* operator->() {
    return raw_;
  }
  void reset() {
    auto i = (*p_cnt_)++;
    borrow_verify(i == -1, "error in RefMut reset");
    p_cnt_ = nullptr;
    raw_ = nullptr;
  }
  ~RefMut() {
    if (p_cnt_) {
      auto i = (*p_cnt_)++;
      borrow_verify(i == -1, "error in checking just single reference of RefMut");
    }
  }
};

template <class T>
class RefCell {
 public:
  RefCell(const RefCell&) = delete;
  RefCell(): raw_(nullptr), cnt_(0) {
  }
  explicit RefCell(T* p) : raw_(p), cnt_(0) {
  };
  RefCell(RefCell&& p) {
    auto i = p.cnt_.exchange(-2);
    borrow_verify(i==0, "verify failed in RefCell move constructor");
    borrow_verify(cnt_ == 0, "verify failed in RefCell move constructor");
    cnt_ = i;
    raw_ = p.raw_;
    p.raw_ = nullptr;
    p.cnt_ = 0;
  };

  inline void reset(T* p) {
    borrow_verify(cnt_ == 0, "error in RefCell reset");
    raw_ = p;
    borrow_verify(cnt_ == 0, "error in RefCell reset"); // is this enough to capture data race?
  }
  T* raw_{nullptr};
  std::atomic<int32_t> cnt_{0};

  inline RefMut<T> borrow_mut() {
    RefMut<T> mut;
    borrow_verify(cnt_ == 0, "verify failed in borrow_mut");
    cnt_--;
    mut.p_cnt_ = &cnt_;
    mut.raw_ = raw_;
    raw_ = nullptr;
    return mut;
  }

  inline Ref<T> borrow_const() {
    // *raw_; // for refer static analysis
    auto i = cnt_++;
    borrow_verify(i >= 0, "verify failed in borrow_const");
    Ref<T> ref;
    ref.raw_ = raw_;
    ref.p_cnt_ = &cnt_;
    return ref;
  }

  T* operator->() {
    borrow_verify(cnt_==0, "verify failed in ->");
    return raw_;
  }

  void reset() {
    borrow_verify(cnt_ == 0, "verify failed in RefCell reset");
    delete raw_;
    raw_ = nullptr;
  }

  ~RefCell() {
    if (raw_) {
      reset();
    }
  }
};

template <typename T>
inline RefMut<T> borrow_mut(RefCell<T>& RefCell) {
  return std::forward<RefMut<T>>(RefCell.borrow_mut());
}

template <typename T>
inline Ref<T> borrow_const(RefCell<T>& RefCell) {
  return std::forward<Ref<T>>(RefCell.borrow_const());
}

template <typename T>
inline void reset_ptr(RefCell<T>& ptr) {
  return ptr.reset();
}

template <typename T>
inline void reset_ptr(RefMut<T>& ptr) {
  return ptr.reset();
}

template <typename T>
inline void reset_ptr(Ref<T>& ptr) {
  return ptr.reset();
}

} // namespace borrow;

// infer run --pulse-only -- clang++ -x c++ -std=c++11 -O0 borrow.h -D BORROW_TEST=1 -D BORROW_INFER_CHECK=1
#ifdef BORROW_TEST
using namespace borrow;
void test1() {
  RefCell<int> owner; 
  owner.reset(new int(5));
  auto x = borrow_mut(owner);
  auto y = borrow_mut(owner);
}

void test2() {
  RefCell<int> owner; 
  owner.reset(new int(5));
  {
    auto x = borrow_mut(owner);
  }
  auto y = borrow_mut(owner);
  {
    auto z = borrow_mut(owner);
  }
}

void test3() {
  RefCell<int> owner; 
  owner.reset(new int(5));
  auto x = borrow_const(owner);
  auto y = borrow_const(owner);
  auto z = borrow_mut(owner);
}

int main() {
  test1();
  test2();
  test3();
}


#endif