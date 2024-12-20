#include <iostream>
#include <utility> // For std::move

// Implementation of std::unique_ptr

template <typename T>
class UniquePtr {
private:
    T* ptr; // Raw pointer to manage

public:
    // Constructor: Takes ownership of a raw pointer
    explicit UniquePtr(T* p = nullptr) : ptr(p) {}

    // Destructor: Deletes the managed object
    ~UniquePtr() {
        delete ptr;
    }

    // Delete copy constructor to prevent copying
    UniquePtr(const UniquePtr&) = delete;

    // Delete copy assignment operator to prevent copying
    UniquePtr& operator=(const UniquePtr&) = delete;

    // Move constructor: Transfers ownership
    UniquePtr(UniquePtr&& other) noexcept : ptr(other.ptr) {
        other.ptr = nullptr;
    }

    // Move assignment operator: Transfers ownership
    UniquePtr& operator=(UniquePtr&& other) noexcept {
        if (this != &other) {
            delete ptr;        // Free the current resource
            ptr = other.ptr;   // Transfer ownership
            other.ptr = nullptr;
        }
        return *this;
    }

    // Access the underlying object
    T* operator->() const {
        return ptr;
    }

    // Dereference the underlying object
    T& operator*() const {
        return *ptr;
    }

    // Release ownership of the managed object
    T* release() {
        T* oldPtr = ptr;
        ptr = nullptr;
        return oldPtr;
    }

    // Reset the managed object
    void reset(T* newPtr = nullptr) {
        delete ptr;
        ptr = newPtr;
    }

    // Check if the pointer is not null
    bool isValid() const {
        return ptr != nullptr;
    }
};


int main() {
    //Examples
    // Using move constructor
    UniquePtr<int> ptr1(new int(42));
    UniquePtr<int> ptr2 = std::move(ptr1);  // Transfer ownership via move constructor
    if (!ptr1.isValid()) {
        std::cout << "ptr1 is now null after move construction." << std::endl;
    }
    if (ptr2.isValid()) {
        std::cout << "Value in ptr2: " << *ptr2 << std::endl;
    }

    // Using move assignment
    UniquePtr<int> ptr3(new int(100));
    ptr3 = std::move(ptr2);  // Transfer ownership via move assignment
    if (!ptr2.isValid()) {
        std::cout << "ptr2 is now null after move assignment." << std::endl;
    }
    if (ptr3.isValid()) {
        std::cout << "Value in ptr3: " << *ptr3 << std::endl;
    }

    UniquePtr<int> uptr1(new int(42));
    std::cout << "Value: " << *uptr1 << std::endl;

    UniquePtr<int> uptr2 = std::move(uptr1); // Transfer ownership
    if (!uptr1.isValid()) {
        std::cout << "uptr1 is now null." << std::endl;
    }

    uptr2.reset(new int(99));
    std::cout << "New value: " << *uptr2 << std::endl;

    int* rawPtr = uptr2.release(); // Release ownership
    std::cout << "Raw pointer value: " << *rawPtr << std::endl;
    delete rawPtr; // Manually delete the released pointer

    UniquePtr<int> uptr3;
    uptr3 = std::move(uptr2); // Move assignment
    if (!uptr2.isValid()) {
        std::cout << "uptr2 is now null after move assignment." << std::endl;
    }

    if (uptr3.isValid()) {
        std::cout << "Value in uptr3: " << *uptr3 << std::endl;
    }

    return 0;
}
