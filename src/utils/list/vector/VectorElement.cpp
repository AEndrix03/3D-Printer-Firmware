//
// Created by redeg on 24/06/2024.
//

#include "utils/std.h"

namespace std {

    template<typename T>
    VectorElement<T>::~VectorElement() {
        delete element;
    }

    template <typename T>
    T VectorElement<T>::get() {
        return element;
    }

    template <typename T>
    void VectorElement<T>::set(T elem) {
        element = elem;
    }

    template <typename T>
    VectorElement<T>* VectorElement<T>::getNext() {
        return next;
    }

    template <typename T>
    VectorElement<T>* VectorElement<T>::getPrevious() {
        return previous;
    }

    template <typename T>
    bool VectorElement<T>::operator==(const VectorElement<T> &el) {
        return element == el.element;
    }

    template <typename T>
    void VectorElement<T>::operator=(const VectorElement<T> &el) {
        element = el.element;
    }

    template <typename T>
    void VectorElement<T>::operator=(T &el) {
        element = el;
    }

    template <typename T>
    void VectorElement<T>::setNext(const VectorElement<T> *element) {
        next = element;
    }

    template <typename T>
    void VectorElement<T>::setPrevious(const VectorElement<T> *element) {
        previous = element;
    }

}