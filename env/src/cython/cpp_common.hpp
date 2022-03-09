#pragma once
#include "Python.h"
#define RAPIDFUZZ_PYTHON
#include <rapidfuzz/fuzz.hpp>
#include <rapidfuzz/distance.hpp>
#include <exception>

#include "rapidfuzz_capi.h"

#define PYTHON_VERSION(major, minor, micro) ((major << 24) | (minor << 16) | (micro << 8))

namespace fuzz = rapidfuzz::fuzz;

class PythonTypeError: public std::bad_typeid {
public:

    PythonTypeError(char const* error)
      : m_error(error) {}

    virtual char const* what() const noexcept {
        return m_error;
    }
private:
    char const* m_error;
};

/* copy from cython */
static inline void CppExn2PyErr() {
  try {
    if (PyErr_Occurred())
      ; // let the latest Python exn pass through and ignore the current one
    else
      throw;
  } catch (const std::bad_alloc& exn) {
    PyErr_SetString(PyExc_MemoryError, exn.what());
  } catch (const std::bad_cast& exn) {
    PyErr_SetString(PyExc_TypeError, exn.what());
  } catch (const std::bad_typeid& exn) {
    PyErr_SetString(PyExc_TypeError, exn.what());
  } catch (const std::domain_error& exn) {
    PyErr_SetString(PyExc_ValueError, exn.what());
  } catch (const std::invalid_argument& exn) {
    PyErr_SetString(PyExc_ValueError, exn.what());
  } catch (const std::ios_base::failure& exn) {
    PyErr_SetString(PyExc_IOError, exn.what());
  } catch (const std::out_of_range& exn) {
    PyErr_SetString(PyExc_IndexError, exn.what());
  } catch (const std::overflow_error& exn) {
    PyErr_SetString(PyExc_OverflowError, exn.what());
  } catch (const std::range_error& exn) {
    PyErr_SetString(PyExc_ArithmeticError, exn.what());
  } catch (const std::underflow_error& exn) {
    PyErr_SetString(PyExc_ArithmeticError, exn.what());
  } catch (const std::exception& exn) {
    PyErr_SetString(PyExc_RuntimeError, exn.what());
  }
  catch (...)
  {
    PyErr_SetString(PyExc_RuntimeError, "Unknown exception");
  }
}

static inline void PyErr2RuntimeExn(bool success) {
    if (!success)
    {
        // Python exceptions should be already set and will be retrieved by Cython
        throw std::runtime_error("");
    }
}

#define LIST_OF_CASES()   \
    X_ENUM(RF_UINT8,  uint8_t ) \
    X_ENUM(RF_UINT16, uint16_t) \
    X_ENUM(RF_UINT32, uint32_t) \
    X_ENUM(RF_UINT64, uint64_t)

/* RAII Wrapper for RF_String */
struct RF_StringWrapper {
    RF_String string;
    PyObject* obj;

    RF_StringWrapper()
        : string({nullptr, (RF_StringType)0, nullptr, 0, nullptr}), obj(nullptr) {}

    RF_StringWrapper(RF_String string_)
        : string(string_), obj(nullptr) {}

    RF_StringWrapper(RF_String string_, PyObject* o)
        : string(string_), obj(o)
    {
        Py_XINCREF(obj);
    }

    RF_StringWrapper(const RF_StringWrapper&) = delete;
    RF_StringWrapper& operator=(const RF_StringWrapper&) = delete;

    RF_StringWrapper(RF_StringWrapper&& other)
        : RF_StringWrapper()
    {
        swap(*this, other);
    }

    RF_StringWrapper& operator=(RF_StringWrapper&& other) {
        if (&other != this) {
            if (string.dtor) {
                string.dtor(&string);
            }
            Py_XDECREF(obj);
            string = other.string;
            obj = other.obj;
            other.string = {nullptr, (RF_StringType)0, nullptr, 0, nullptr};
            other.obj = nullptr;
      }
      return *this;
    };

    ~RF_StringWrapper() {
        if (string.dtor) {
            string.dtor(&string);
        }
        Py_XDECREF(obj);
    }

    friend void swap(RF_StringWrapper& first, RF_StringWrapper& second) noexcept
    {
        using std::swap;
        swap(first.string, second.string);
        swap(first.obj, second.obj);
    }
};

/* RAII Wrapper for RF_Kwargs */
struct RF_KwargsWrapper {
    RF_Kwargs kwargs;

    RF_KwargsWrapper()
        : kwargs({NULL, NULL}) {}

    RF_KwargsWrapper(RF_Kwargs kwargs_)
        : kwargs(kwargs_) {}

    RF_KwargsWrapper(const RF_KwargsWrapper&) = delete;
    RF_KwargsWrapper& operator=(const RF_KwargsWrapper&) = delete;

    RF_KwargsWrapper(RF_KwargsWrapper&& other)
        : RF_KwargsWrapper()
    {
        swap(*this, other);
    }

    RF_KwargsWrapper& operator=(RF_KwargsWrapper&& other)
    {
        if (&other != this) {
            if (kwargs.dtor) {
                kwargs.dtor(&kwargs);
            }
            kwargs = other.kwargs;
            other.kwargs = {NULL, NULL};
        }
        return *this;
    };

    ~RF_KwargsWrapper() {
        if (kwargs.dtor) {
            kwargs.dtor(&kwargs);
        }
    }

    friend void swap(RF_KwargsWrapper& first, RF_KwargsWrapper& second) noexcept
    {
        using std::swap;
        swap(first.kwargs, second.kwargs);
    }
};

/* RAII Wrapper for PyObject* */
struct PyObjectWrapper {
    PyObject* obj;

    PyObjectWrapper() noexcept
        : obj(nullptr) {}

    PyObjectWrapper(PyObject* o) noexcept
        : obj(o)
    {
        Py_XINCREF(obj);
    }

    PyObjectWrapper(const PyObjectWrapper& other) noexcept
    {
        obj = other.obj;
        Py_XINCREF(obj);
    }
    PyObjectWrapper& operator=(PyObjectWrapper other) noexcept
    {
        swap(*this, other);
        return *this;
    }

    PyObjectWrapper(PyObjectWrapper&& other) noexcept
        : PyObjectWrapper()
    {
        swap(*this, other);
    }

    PyObjectWrapper& operator=(PyObjectWrapper&& other) noexcept
    {
        swap(*this, other);
        return *this;
    };

    ~PyObjectWrapper() { Py_XDECREF(obj); }

    friend void swap(PyObjectWrapper& first, PyObjectWrapper& second) noexcept
    {
        using std::swap;
        swap(first.obj, second.obj);
    }
};

void default_string_deinit(RF_String* string)
{
    free(string->data);
}

template <typename Func, typename... Args>
auto visit(const RF_String& str, Func&& f, Args&&... args)
{
    switch(str.kind) {
# define X_ENUM(kind, type) case kind: return f((type*)str.data, (type*)str.data + str.length, std::forward<Args>(args)...);
    LIST_OF_CASES()
# undef X_ENUM
    default:
        throw std::logic_error("Invalid string type");
    }
}

template <typename Func, typename... Args>
auto visitor(const RF_String& str1, const RF_String& str2, Func&& f, Args&&... args)
{
    return visit(str2,
        [&](auto first, auto last) {
            return visit(str1, std::forward<Func>(f), first, last, std::forward<Args>(args)...);
        }
    );
}

static inline bool is_valid_string(PyObject* py_str)
{
    bool is_string = false;

    if (PyBytes_Check(py_str)) {
        is_string = true;
    }
    else if (PyUnicode_Check(py_str)) {
        // PEP 623 deprecates legacy strings and therefor
        // deprecates e.g. PyUnicode_READY in Python 3.10
#if PY_VERSION_HEX < PYTHON_VERSION(3, 10, 0)
        if (PyUnicode_READY(py_str)) {
          // cython will use the exception set by PyUnicode_READY
          throw std::runtime_error("");
        }
#endif
        is_string = true;
    }

    return is_string;
}

static inline void validate_string(PyObject* py_str, const char* err)
{
    if (PyBytes_Check(py_str)) {
        return;
    }
    else if (PyUnicode_Check(py_str)) {
        // PEP 623 deprecates legacy strings and therefor
        // deprecates e.g. PyUnicode_READY in Python 3.10
#if PY_VERSION_HEX < PYTHON_VERSION(3, 10, 0)
        if (PyUnicode_READY(py_str)) {
          // cython will use the exception set by PyUnicode_READY
          throw std::runtime_error("");
        }
#endif
        return;
    }

    throw PythonTypeError(err);
}

static inline RF_String convert_string(PyObject* py_str)
{
    if (PyBytes_Check(py_str)) {
        return {
            nullptr,
            RF_UINT8,
            PyBytes_AS_STRING(py_str),
            static_cast<int64_t>(PyBytes_GET_SIZE(py_str)),
            nullptr
        };
    } else {
        RF_StringType kind;
        switch(PyUnicode_KIND(py_str)) {
        case PyUnicode_1BYTE_KIND:
           kind = RF_UINT8;
           break;
        case PyUnicode_2BYTE_KIND:
           kind = RF_UINT16;
           break;
        default:
           kind = RF_UINT32;
           break;
        }

        return {
            nullptr,
            kind,
            PyUnicode_DATA(py_str),
            static_cast<int64_t>(PyUnicode_GET_LENGTH(py_str)),
            nullptr
        };
    }
}

template <typename CachedScorer>
static void scorer_deinit(RF_ScorerFunc* self)
{
    delete (CachedScorer*)self->context;
}

template<typename CachedScorer, typename T>
static inline bool distance_func_wrapper(const RF_ScorerFunc* self, const RF_String* str, int64_t str_count, T score_cutoff, T* result)
{
    CachedScorer& scorer = *(CachedScorer*)self->context;
    try {
        if (str_count != 1)
        {
            throw std::logic_error("Only str_count == 1 supported");
        }
        *result = visit(*str, [&](auto first, auto last){
            return scorer.distance(first, last, score_cutoff);
        });
    } catch(...) {
      PyGILState_STATE gilstate_save = PyGILState_Ensure();
      CppExn2PyErr();
      PyGILState_Release(gilstate_save);
      return false;
    }
    return true;
}

template<typename CachedScorer, typename T>
static inline bool normalized_distance_func_wrapper(const RF_ScorerFunc* self, const RF_String* str, int64_t str_count, T score_cutoff, T* result)
{
    CachedScorer& scorer = *(CachedScorer*)self->context;
    try {
        if (str_count != 1)
        {
            throw std::logic_error("Only str_count == 1 supported");
        }
        *result = visit(*str, [&](auto first, auto last){
            return scorer.normalized_distance(first, last, score_cutoff);
        });
    } catch(...) {
      PyGILState_STATE gilstate_save = PyGILState_Ensure();
      CppExn2PyErr();
      PyGILState_Release(gilstate_save);
      return false;
    }
    return true;
}

template<typename CachedScorer, typename T>
static inline bool similarity_func_wrapper(const RF_ScorerFunc* self, const RF_String* str, int64_t str_count, T score_cutoff, T* result)
{
    CachedScorer& scorer = *(CachedScorer*)self->context;
    try {
        if (str_count != 1)
        {
            throw std::logic_error("Only str_count == 1 supported");
        }
        *result = visit(*str, [&](auto first, auto last){
            return scorer.similarity(first, last, score_cutoff);
        });
    } catch(...) {
      PyGILState_STATE gilstate_save = PyGILState_Ensure();
      CppExn2PyErr();
      PyGILState_Release(gilstate_save);
      return false;
    }
    return true;
}

template<typename CachedScorer, typename T>
static inline bool normalized_similarity_func_wrapper(const RF_ScorerFunc* self, const RF_String* str, int64_t str_count, T score_cutoff, T* result)
{
    CachedScorer& scorer = *(CachedScorer*)self->context;
    try {
        if (str_count != 1)
        {
            throw std::logic_error("Only str_count == 1 supported");
        }
        *result = visit(*str, [&](auto first, auto last){
            return scorer.normalized_similarity(first, last, score_cutoff);
        });
    } catch(...) {
      PyGILState_STATE gilstate_save = PyGILState_Ensure();
      CppExn2PyErr();
      PyGILState_Release(gilstate_save);
      return false;
    }
    return true;
}

// todo cleanup
typedef bool (*func_f64) (const struct _RF_ScorerFunc*, const RF_String*, int64_t, double, double*);
typedef bool (*func_i64) (const struct _RF_ScorerFunc*, const RF_String*, int64_t, int64_t, int64_t*);

void assign_callback(RF_ScorerFunc& context, func_f64 func)
{
    context.call.f64 = func;
}

void assign_callback(RF_ScorerFunc& context, func_i64 func)
{
    context.call.i64 = func;
}

template<template <typename> class CachedScorer, typename T, typename InputIt1, typename ...Args>
static inline RF_ScorerFunc get_ScorerContext_distance(InputIt1 first1, InputIt1 last1, Args... args)
{
    using CharT1 = typename std::iterator_traits<InputIt1>::value_type;
    RF_ScorerFunc context;
    context.context = (void*) new CachedScorer<CharT1>(first1, last1, args...);

    assign_callback(context, distance_func_wrapper<CachedScorer<CharT1>, T>);
    context.dtor = scorer_deinit<CachedScorer<CharT1>>;
    return context;
}

template<template <typename> class CachedScorer, typename T, typename InputIt1, typename ...Args>
static inline RF_ScorerFunc get_ScorerContext_normalized_distance(InputIt1 first1, InputIt1 last1, Args... args)
{
    using CharT1 = typename std::iterator_traits<InputIt1>::value_type;
    RF_ScorerFunc context;
    context.context = (void*) new CachedScorer<CharT1>(first1, last1, args...);

    assign_callback(context, normalized_distance_func_wrapper<CachedScorer<CharT1>, T>);
    context.dtor = scorer_deinit<CachedScorer<CharT1>>;
    return context;
}

template<template <typename> class CachedScorer, typename T, typename InputIt1, typename ...Args>
static inline RF_ScorerFunc get_ScorerContext_similarity(InputIt1 first1, InputIt1 last1, Args... args)
{
    using CharT1 = typename std::iterator_traits<InputIt1>::value_type;
    RF_ScorerFunc context;
    context.context = (void*) new CachedScorer<CharT1>(first1, last1, args...);

    assign_callback(context, similarity_func_wrapper<CachedScorer<CharT1>, T>);
    context.dtor = scorer_deinit<CachedScorer<CharT1>>;
    return context;
}

template<template <typename> class CachedScorer, typename T, typename InputIt1, typename ...Args>
static inline RF_ScorerFunc get_ScorerContext_normalized_similarity(InputIt1 first1, InputIt1 last1, Args... args)
{
    using CharT1 = typename std::iterator_traits<InputIt1>::value_type;
    RF_ScorerFunc context;
    context.context = (void*) new CachedScorer<CharT1>(first1, last1, args...);

    assign_callback(context, normalized_similarity_func_wrapper<CachedScorer<CharT1>, T>);
    context.dtor = scorer_deinit<CachedScorer<CharT1>>;
    return context;
}

template<template <typename> class CachedScorer, typename T, typename ...Args>
static inline bool distance_init(RF_ScorerFunc* self, int64_t str_count, const RF_String* strings, Args... args)
{
    try {
        if (str_count != 1)
        {
            throw std::logic_error("Only str_count == 1 supported");
        }
        *self = visit(*strings, [&](auto first1, auto last1){
            return get_ScorerContext_distance<CachedScorer, T>(first1, last1, args...);
        });
    } catch(...) {
      PyGILState_STATE gilstate_save = PyGILState_Ensure();
      CppExn2PyErr();
      PyGILState_Release(gilstate_save);
      return false;
    }
    return true;
}

template<template <typename> class CachedScorer, typename T, typename ...Args>
static inline bool normalized_distance_init(RF_ScorerFunc* self, int64_t str_count, const RF_String* strings, Args... args)
{
    try {
        if (str_count != 1)
        {
            throw std::logic_error("Only str_count == 1 supported");
        }
        *self = visit(*strings, [&](auto first1, auto last1){
            return get_ScorerContext_normalized_distance<CachedScorer, T>(first1, last1, args...);
        });
    } catch(...) {
      PyGILState_STATE gilstate_save = PyGILState_Ensure();
      CppExn2PyErr();
      PyGILState_Release(gilstate_save);
      return false;
    }
    return true;
}

template<template <typename> class CachedScorer, typename T, typename ...Args>
static inline bool similarity_init(RF_ScorerFunc* self, int64_t str_count, const RF_String* strings, Args... args)
{
    try {
        if (str_count != 1)
        {
            throw std::logic_error("Only str_count == 1 supported");
        }
        *self = visit(*strings, [&](auto first1, auto last1){
            return get_ScorerContext_similarity<CachedScorer, T>(first1, last1, args...);
        });
    } catch(...) {
      PyGILState_STATE gilstate_save = PyGILState_Ensure();
      CppExn2PyErr();
      PyGILState_Release(gilstate_save);
      return false;
    }
    return true;
}

template<template <typename> class CachedScorer, typename T, typename ...Args>
static inline bool normalized_similarity_init(RF_ScorerFunc* self, int64_t str_count, const RF_String* strings, Args... args)
{
    try {
        if (str_count != 1)
        {
            throw std::logic_error("Only str_count == 1 supported");
        }
        *self = visit(*strings, [&](auto first1, auto last1){
            return get_ScorerContext_normalized_similarity<CachedScorer, T>(first1, last1, args...);
        });
    } catch(...) {
      PyGILState_STATE gilstate_save = PyGILState_Ensure();
      CppExn2PyErr();
      PyGILState_Release(gilstate_save);
      return false;
    }
    return true;
}

template<typename T>
std::vector<T> vector_slice(const std::vector<T>& vec, int start, int stop, int step)
{
    std::vector<T> new_vec;
    if (step == 0)
    {
        throw std::invalid_argument("slice step cannot be zero");
    }

    if (start < 0)
    {
        start += vec.size();
    }
    else if (start > vec.size())
    {
        start = vec.size();
    }

    if (stop < 0)
    {
        stop += vec.size();
    }
    else if (stop > vec.size())
    {
        stop = vec.size();
    }

    // todo perhaps makes sence to add special implementation for
    // step == 1 since this should be the most common
    if (start > stop)
    {
        stop = std::max(stop, -1);
        if (start != stop && start >= 0 && step < 0)
        {
            int count = 1 + (stop - start + 1) / step;
            new_vec.reserve(count);
            for (int i = start - 1; i > stop; i += step)
            {
                new_vec.push_back(vec[i]);
            }
        }
    }
    else if (start < stop)
    {
        start = std::max(start, 0);
        if (start != stop && stop >= 0 && step > 0)
        {
            int count = 1 + (stop - start - 1) / step;
            new_vec.reserve(count);
            for (int i = start; i < stop; i += step)
            {
                new_vec.push_back(vec[i]);
            }
        }
    }

    return new_vec;
}
