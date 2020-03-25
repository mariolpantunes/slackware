/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "runtime/helpers/debug_helpers.h"

#include <cinttypes>
#include <cstddef>
#include <vector>

template <typename DataType, size_t OnStackCapacity>
class StackVec {
  public:
    using iterator = DataType *;
    using const_iterator = const DataType *;

    static const size_t onStackCaps = OnStackCapacity;

    StackVec() = default;

    template <typename ItType>
    StackVec(ItType beginIt, ItType endIt)
        : dynamicMem(nullptr) {
        size_t count = (endIt - beginIt);
        if (count > OnStackCapacity) {
            dynamicMem = new std::vector<DataType>(beginIt, endIt);
            return;
        }

        while (beginIt != endIt) {
            push_back(*beginIt);
            ++beginIt;
        }
        onStackSize = count;
    }

    StackVec(const StackVec &rhs)
        : dynamicMem(nullptr) {
        if (onStackCaps < rhs.size()) {
            dynamicMem = new std::vector<DataType>(rhs.begin(), rhs.end());
            return;
        }

        for (const auto &v : rhs) {
            push_back(v);
        }
    }

    explicit StackVec(size_t initialSize)
        : StackVec() {
        resize(initialSize);
    }

    StackVec &operator=(const StackVec &rhs) {
        clear();

        if (this->dynamicMem != nullptr) {
            this->dynamicMem->insert(dynamicMem->end(), rhs.begin(), rhs.end());
            return *this;
        }

        if (onStackCaps < rhs.size()) {
            this->dynamicMem = new std::vector<DataType>(rhs.begin(), rhs.end());
            return *this;
        }

        for (const auto &v : rhs) {
            push_back(v);
        }

        return *this;
    }

    StackVec(StackVec &&rhs)
        : dynamicMem(nullptr) {
        if (rhs.dynamicMem != nullptr) {
            std::swap(this->dynamicMem, rhs.dynamicMem);
            return;
        }

        for (const auto &v : rhs) {
            push_back(v);
        }
    }

    StackVec &operator=(StackVec &&rhs) {
        clear();

        if (rhs.dynamicMem != nullptr) {
            std::swap(this->dynamicMem, rhs.dynamicMem);
            return *this;
        }

        if (this->dynamicMem != nullptr) {
            this->dynamicMem->insert(this->dynamicMem->end(), rhs.begin(), rhs.end());
            return *this;
        }

        for (const auto &v : rhs) {
            push_back(v);
        }

        return *this;
    }

    ~StackVec() {
        if (dynamicMem != nullptr) {
            delete dynamicMem;
            return;
        }
        clear();
    }

    size_t size() const {
        if (dynamicMem) {
            return dynamicMem->size();
        }
        return onStackSize;
    }

    size_t capacity() const {
        if (dynamicMem) {
            return dynamicMem->capacity();
        }
        return OnStackCapacity;
    }

    void reserve(size_t newCapacity) {
        if (newCapacity > onStackCaps) {
            ensureDynamicMem();
            dynamicMem->reserve(newCapacity);
        }
    }

    void clear() {
        if (dynamicMem) {
            dynamicMem->clear();
            return;
        }
        clearStackObjects();
    }

    void push_back(const DataType &v) { // NOLINT
        if (onStackSize == onStackCaps) {
            ensureDynamicMem();
        }

        if (dynamicMem) {
            dynamicMem->push_back(v);
            return;
        }

        new (onStackMem + onStackSize) DataType(v);
        ++onStackSize;
    }

    DataType &operator[](std::size_t idx) {
        if (dynamicMem) {
            return (*dynamicMem)[idx];
        }
        return *(onStackMem + idx);
    }

    const DataType &operator[](std::size_t idx) const {
        if (dynamicMem) {
            return (*dynamicMem)[idx];
        }
        return *(onStackMem + idx);
    }

    iterator begin() {
        if (dynamicMem) {
            return dynamicMem->data();
        }

        return onStackMem;
    }

    const_iterator begin() const {
        if (dynamicMem) {
            return dynamicMem->data();
        }

        return onStackMem;
    }

    iterator end() {
        if (dynamicMem) {
            return dynamicMem->data() + dynamicMem->size();
        }

        return onStackMem + onStackSize;
    }

    const_iterator end() const {
        if (dynamicMem) {
            return dynamicMem->data() + dynamicMem->size();
        }

        return onStackMem + onStackSize;
    }

    void resize(size_t newSize) {
        this->resizeImpl(newSize, nullptr);
    }

    void resize(size_t newSize, const DataType &value) {
        resizeImpl(newSize, &value);
    }

  private:
    void resizeImpl(size_t newSize, const DataType *value) {
        // new size does not fit into internal mem
        if (newSize > onStackCaps) {
            ensureDynamicMem();
        }

        // memory already backed by stl vector
        if (dynamicMem != nullptr) {
            if (value != nullptr) {
                dynamicMem->resize(newSize, *value);
            } else {
                dynamicMem->resize(newSize);
            }
            return;
        }

        if (newSize <= onStackSize) {
            // trim elements
            clearStackObjects(newSize, onStackSize - newSize);
            onStackSize = newSize;
            return;
        }

        // add new elements
        if (value != nullptr) {
            // copy-construct elements
            while (onStackSize < newSize) {
                new (onStackMem + onStackSize) DataType(*value);
                ++onStackSize;
            }
        } else {
            // default-construct elements
            while (onStackSize < newSize) {
                new (onStackMem + onStackSize) DataType();
                ++onStackSize;
            }
        }
    }

    void ensureDynamicMem() {
        if (dynamicMem == nullptr) {
            dynamicMem = new std::vector<DataType>();
            if (onStackSize > 0) {
                dynamicMem->reserve(onStackSize);
                for (auto it = onStackMem, end = onStackMem + onStackSize; it != end; ++it) {
                    dynamicMem->push_back(std::move(*it));
                }
                clearStackObjects();
            }
        }
    }

    void clearStackObjects() {
        clearStackObjects(0, onStackSize);
        onStackSize = 0;
    }

    void clearStackObjects(size_t offset, size_t count) {
        UNRECOVERABLE_IF(offset + count > onStackSize);
        for (auto it = onStackMem + offset, end = onStackMem + offset + count; it != end; ++it) {
            it->~DataType();
        }
    }

    alignas(alignof(DataType)) char onStackMemRawBytes[sizeof(DataType[onStackCaps])];
    std::vector<DataType> *dynamicMem = nullptr;
    DataType *const onStackMem = reinterpret_cast<DataType *const>(onStackMemRawBytes);
    size_t onStackSize = 0;
};

template <typename T, size_t LhsStackCaps, size_t RhsStackCaps>
bool operator==(const StackVec<T, LhsStackCaps> &lhs,
                const StackVec<T, RhsStackCaps> &rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    auto lhsIt = lhs.begin();
    auto lhsEnd = lhs.end();
    auto rhsIt = rhs.begin();

    for (; lhsIt != lhsEnd; ++lhsIt, ++rhsIt) {
        if (*lhsIt != *rhsIt) {
            return false;
        }
    }

    return true;
}
