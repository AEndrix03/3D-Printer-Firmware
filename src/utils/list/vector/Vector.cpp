//
// Created by redeg on 24/06/2024.
//

#include "utils/std.h"

namespace std {

    template <typename T>
    Vector<T>::Vector() {
        cursor = nullptr;
        head = nullptr;
        tail = nullptr;
    }

    template <typename T>
    Vector<T>::~Vector() {
        delete (*cursor);
        delete (*head);
        delete (*tail);
    }

    template <typename T>
    void Vector<T>::push_back(const T &obj) {
        auto* newElement = new VectorElement<T>(obj);
        if (tail != nullptr) {
            tail->setNext(newElement);
            newElement->setPrevious(tail);
        } else {
            head = newElement;
        }

        tail = newElement;
        ++lenght;
    }

    template<typename T>
    void Vector<T>::push(const T *obj) {
        VectorElement<T>* newElement = new VectorElement<T>(*obj);
        if (head != nullptr) {
           head->setPrevious(newElement);
           newElement->setNext(head);
        } else {
           tail = newElement;
        }

        head = newElement;
        ++lenght;
    }

    template<typename T>
    T Vector<T>::pop() {
        T element;
        if (head != nullptr) {
            element = head->get();
            head = head->getNext();
            head->setPrevious(nullptr);
            --lenght;
        }

        return element;
    }

    template<typename T>
    int Vector<T>::size() {
        return lenght;
    }

    template<typename T>
    const T* Vector<T>::const_find(const T &search) {
        VectorElement<T> *tmp = head;
        while (tmp != nullptr) {
            if (search == *tmp)
                return &(tmp->get());
        }
        return nullptr;
    }

    template<typename T>
    bool Vector<T>::operator==(const Vector<T> &v) {
        VectorElement<T> *tmp = head;
        VectorElement<T> *tmpV = v.head;
        while (tmp != nullptr && tmpV != nullptr) {
            if (tmp->get() == tmpV->get()) {
                tmp = tmp->getNext();
                tmpV = tmpV->getNext();
            } else {
                return false;
            }
        }
        return tmp == nullptr && tmpV == nullptr;
    }

    template<typename T>
    void Vector<T>::clear() {
        VectorElement<T> *tmp = head;
        VectorElement<T> *next = nullptr;
        while (tmp != nullptr) {
            next = tmp->getNext();
            delete tmp;
            tmp = next;
        }
        head = nullptr;
        tail = nullptr;
    }

    template<typename T>
    Vector<T>& Vector<T>::operator=(const Vector<T> &v) {
        if (this != &v) {
            clear();
            head = v.head;
            tail = v.tail;
        }
    }

    template<typename T>
    VectorElement<T> *Vector<T>::operator[](const int position) {
        int i = 0;
        VectorElement<T> * tmp = head;
        while (tmp != nullptr && i != position) {
            i++;
            tmp = tmp->getNext();
        }

        return tmp != nullptr ? tmp->get() : nullptr;
    }

    template<typename T>
    void Vector<T>::remove(const T &obj) {
        VectorElement<T> * tmp = head;
        while(tmp != nullptr) {
            if (*tmp == obj) {
                if (tmp->getPrevious() != nullptr) {
                    tmp->getPrevious()->setNext(tmp->getNext());
                } else {
                    head = tmp->getNext();
                }

                if (tmp->getNext() == nullptr) {
                    tail = tmp->getPrevious();
                }

                delete tmp;
            } else {
                tmp = tmp->getNext();
            }
        }
    }

} // std