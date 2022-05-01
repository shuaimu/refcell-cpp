#include <cstdint>
#include <memory>
#include <utility>

namespace borrow {

#ifndef borrow_verify
#define borrow_verify(x) {if (!(x)) {volatile int* a = nullptr ; *a;}}
//#define borrow_verify(x) nullptr
#endif

template<class T>
class ref_ptr {
 public:
  volatile const T* raw_{nullptr};
  int32_t* p_cnt_{nullptr};
  ref_ptr() = default;
  ref_ptr(const ref_ptr&) = delete;
  ref_ptr(ref_ptr&& p) {
    raw_ = p.raw_; 
    p_cnt_ = p.p_cnt_;
    p.raw_ = nullptr;
    p.p_cnt_ = nullptr;
  };
  ref_ptr(ref_ptr& p) {
    raw_ = p.raw_;
    p_cnt_ = p.p_cnt_;
    auto i = (*p_cnt_)++;
    borrow_verify(i > 0);
  }
  const T* operator->() {
    return raw_;
  }
  void release() {
    auto i = (*p_cnt_)--;
    borrow_verify(i > 0);
    raw_ = nullptr;
    p_cnt_ = nullptr;
  }
  ~ref_ptr() {
    if (p_cnt_ != nullptr) {
      auto i = (*p_cnt_)--;
      borrow_verify(i > 0);
    }
  }
};

template <typename T>
class mut_ptr {
 public:
  T* raw_;
  int32_t* p_cnt_;
  mut_ptr() = default;
  mut_ptr(const mut_ptr&) = delete;
  mut_ptr(mut_ptr&& p) : raw_(p.raw_), p_cnt_(p.p_cnt_) {
    raw_ = p.raw_;
    p.p_cnt_ = nullptr;
    p.raw_ = nullptr;
  }
  T* operator->() {
    return raw_;
  }
  void release() {
    auto i = (*p_cnt_)++;
    borrow_verify(i == -1);
    p_cnt_ = nullptr;
    raw_ = nullptr;
  }
  ~mut_ptr() {
    if (p_cnt_) {
      auto i = (*p_cnt_)++;
      borrow_verify(i == -1);
    }
  }
};

template <class T>
class own_ptr {
 public:
  own_ptr(const own_ptr&) = delete;
  own_ptr(): raw_(nullptr), cnt_(0) {
  }
  explicit own_ptr(T* p) : raw_(p), cnt_(0) {
  };
  own_ptr(own_ptr&& p) {
    auto i = p.cnt_.exchange(-2);
    borrow_verify(i==0);
    borrow_verify(cnt_ == 0);
    cnt_ = i;
    raw_ = p.raw_;
    p.raw_ = nullptr;
    p.cnt_ = 0;
  };

  inline void reset(T* p) {
    borrow_verify(cnt_ == 0);
    raw_ = p;
    borrow_verify(cnt_ == 0); // is this enough to capture data race?
  }
  T* raw_{nullptr};
  int32_t cnt_{0};

  inline mut_ptr<T> borrow_mut() {
    mut_ptr<T> mut;
    borrow_verify(cnt_ == 0);
    cnt_--;
    mut.p_cnt_ = &cnt_;
    mut.raw_ = raw_;
    raw_ = nullptr;
    return mut;
  }

  inline ref_ptr<T> borrow_ref() {
    *raw_; // for refer static analysis
    auto i = cnt_++;
    borrow_verify(i >= 0);
    ref_ptr<T> ref;
    ref.raw_ = raw_;
    ref.p_cnt_ = &cnt_;
    return ref;
  }

  T* operator->() {
    borrow_verify(cnt_==0);
    return raw_;
  }

  void release() {
    borrow_verify(cnt_ == 0);
    delete raw_;
    raw_ = nullptr;
  }

  ~own_ptr() {
    if (raw_) {
      release();
    }
  }
};

template <typename T>
inline mut_ptr<T> borrow_mut(own_ptr<T>& own_ptr) {
  return std::forward<mut_ptr<T>>(own_ptr.borrow_mut());
}

template <typename T>
inline ref_ptr<T> borrow_ref(own_ptr<T>& own_ptr) {
  return std::forward<ref_ptr<T>>(own_ptr.borrow_ref());
}

template <typename T>
inline void release_ptr(own_ptr<T>& ptr) {
  return ptr.release();
}

template <typename T>
inline void release_ptr(mut_ptr<T>& ptr) {
  return ptr.release();
}

template <typename T>
inline void release_ptr(ref_ptr<T>& ptr) {
  return ptr.release();
}

} // namespace borrow;

// infer run --pulse-only -- clang++ -x c++ -std=c++11 -O0 borrow.h -D BORROW_TEST=1
#ifdef BORROW_TEST
using namespace borrow;
void test1() {
  own_ptr<int> owner; 
  owner.reset(new int(5));
  auto x = borrow_mut(owner);
  auto y = borrow_mut(owner);
}

void test2() {
  own_ptr<int> owner; 
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
  own_ptr<int> owner; 
  owner.reset(new int(5));
  auto x = borrow_ref(owner);
  auto y = borrow_ref(owner);
  auto z = borrow_mut(owner);
}

int main() {
  test1();
  test2();
  test3();
}


#endif
