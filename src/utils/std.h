//
// Created by redeg on 24/06/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_STD_H
#define INC_3D_PRINTER_FIRMWARE_STD_H

namespace std {

    template <typename T>
    class VectorElement {
    public:
        explicit VectorElement(T element) : element(element) {};
        ~VectorElement();
        bool operator==(const VectorElement<T>& el);
        void operator=(const VectorElement<T>& el);
        void operator=(T& el);
        T get();
        void set(T elem);
        VectorElement* getNext();
        VectorElement* getPrevious();
        void setNext(const VectorElement<T>* element);
        void setPrevious(const VectorElement<T>* element);
    private:
        T element;
        VectorElement* previous;
        VectorElement* next;
    };

    template <typename T>
    class Vector {
    public:
        Vector();
        ~Vector();
        bool operator==(const Vector<T>& v);
        Vector<T>& operator=(const Vector<T>& v);
        VectorElement<T>* operator[](const int position);
        void push(const T* obj);
        void push_back(const T& obj);
        T pop();
        int size();
        void clear();
        void remove(const T& obj);

        const T* const_find(const T& search);

        class Iterator {
        public:
            explicit Iterator(VectorElement<T>* node) : current(node) {}

            bool operator!=(const Iterator& other) const { return current != other.current; }
            T& operator*() { return current->get(); }
            Iterator& operator++() {
                if (current) current = current->getNext();
                return *this;
            }

        private:
            VectorElement<T>* current;
        };

        Iterator begin() { return Iterator(head); }
        Iterator end() { return Iterator(nullptr); }
    private:
        VectorElement<T>* cursor;
        VectorElement<T>* head;
        VectorElement<T>* tail;
        int lenght = 0;
    };

}

#endif //INC_3D_PRINTER_FIRMWARE_STD_H
