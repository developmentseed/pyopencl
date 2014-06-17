#include "error.h"
#include "clobj.h"

#ifndef __PYOPENCL_CLHELPER_H
#define __PYOPENCL_CLHELPER_H

namespace pyopencl {

// {{{ GetInfo helpers

template<typename T, typename... ArgTypes, typename... ArgTypes2>
PYOPENCL_USE_RESULT static PYOPENCL_INLINE pyopencl_buf<T>
get_vec_info(cl_int (*func)(ArgTypes...), const char *name,
             ArgTypes2&&... args)
{
    size_t size = 0;
    call_guarded(func, name, args..., 0, nullptr, &size);
    pyopencl_buf<T> buf(size / sizeof(T));
    call_guarded(func, name, args..., size, buf.get(), &size);
    return buf;
}
#define pyopencl_get_vec_info(type, what, args...)                      \
    pyopencl::get_vec_info<type>(clGet##what##Info, "clGet" #what "Info", args)

template<typename T>
PYOPENCL_USE_RESULT static PYOPENCL_INLINE generic_info
convert_array_info(const char *tname, pyopencl_buf<T> &buf)
{
    generic_info info;
    info.dontfree = 0;
    info.opaque_class = CLASS_NONE;
    info.type = _copy_str(std::string(tname) + "[" +
                          tostring(buf.len()) + "]");
    info.value = buf.release();
    return info;
}

template<typename T>
PYOPENCL_USE_RESULT static PYOPENCL_INLINE generic_info
convert_array_info(const char *tname, pyopencl_buf<T> &&_buf)
{
    pyopencl_buf<T> &buf = _buf;
    return convert_array_info<T>(tname, buf);
}

#define pyopencl_convert_array_info(type, buf)          \
    pyopencl::convert_array_info<type>(#type, buf)
#define pyopencl_get_array_info(type, what, args...)                    \
    pyopencl_convert_array_info(type, pyopencl_get_vec_info(type, what, args))

template<typename CLObj, typename T>
PYOPENCL_USE_RESULT static PYOPENCL_INLINE generic_info
convert_opaque_array_info(T &&buf)
{
    generic_info info;
    info.dontfree = 0;
    info.opaque_class = CLObj::get_class_t();
    info.type = _copy_str(std::string("void*[") + tostring(buf.len()) + "]");
    info.value = buf_to_base<CLObj>(std::forward<T>(buf)).release();
    return info;
}
#define pyopencl_get_opaque_array_info(cls, what, args...)  \
    pyopencl::convert_opaque_array_info<cls>(               \
        pyopencl_get_vec_info(cls::cl_type, what, args))

template<typename CLObj, typename... ArgTypes, typename... ArgTypes2>
PYOPENCL_USE_RESULT static PYOPENCL_INLINE generic_info
get_opaque_info(cl_int (*func)(ArgTypes...), const char *name,
                ArgTypes2&&... args)
{
    typename CLObj::cl_type param_value;
    call_guarded(func, name, args..., sizeof(param_value),
                 &param_value, nullptr);
    generic_info info;
    info.dontfree = 0;
    info.opaque_class = CLObj::get_class_t();
    info.type = "void *";
    if (param_value) {
        info.value = (void*)(new CLObj(param_value, /*retain*/ true));
    } else {
        info.value = nullptr;
    }
    return info;
}
#define pyopencl_get_opaque_info(clobj, what, args...)              \
    pyopencl::get_opaque_info<clobj>(clGet##what##Info,             \
                                     "clGet" #what "Info", args)

template<typename... ArgTypes, typename... ArgTypes2>
PYOPENCL_USE_RESULT static PYOPENCL_INLINE generic_info
get_str_info(cl_int (*func)(ArgTypes...), const char *name,
             ArgTypes2&&... args)
{
    size_t param_value_size;
    call_guarded(func, name, args..., 0, nullptr, &param_value_size);
    pyopencl_buf<char> param_value(param_value_size);
    call_guarded(func, name, args..., param_value_size,
                 param_value.get(), &param_value_size);
    generic_info info;
    info.dontfree = 0;
    info.opaque_class = CLASS_NONE;
    info.type = "char*";
    info.value = (void*)param_value.release();
    return info;
}
#define pyopencl_get_str_info(what, args...)                            \
    pyopencl::get_str_info(clGet##what##Info, "clGet" #what "Info", args)

template<typename T, typename... ArgTypes, typename... ArgTypes2>
PYOPENCL_USE_RESULT static PYOPENCL_INLINE generic_info
get_int_info(cl_int (*func)(ArgTypes...), const char *name,
             const char *tpname, ArgTypes2&&... args)
{
    pyopencl_buf<T> param_value;
    call_guarded(func, name, args..., sizeof(T), param_value.get(), nullptr);
    generic_info info;
    info.dontfree = 0;
    info.opaque_class = CLASS_NONE;
    info.type = tpname;
    info.value = (void*)param_value.release();
    return info;
}
#define pyopencl_get_int_info(type, what, args...)                      \
    pyopencl::get_int_info<type>(clGet##what##Info, "clGet" #what "Info", \
                                 #type "*", args)

// }}}

template<typename T, typename CLType, typename... ArgTypes>
PYOPENCL_USE_RESULT static PYOPENCL_INLINE T*
convert_obj(cl_int (*clRelease)(CLType), const char *name, CLType cl_obj,
            ArgTypes&&... args)
{
    try {
        return new T(cl_obj, false, std::forward<ArgTypes>(args)...);
    } catch (...) {
        call_guarded_cleanup(clRelease, name, cl_obj);
        throw;
    }
}
#define pyopencl_convert_obj(type, func, args...)       \
    pyopencl::convert_obj<type>(func, #func, args)

template<typename CLObj, typename... T>
class _CLObjOutArg : public OutArg {
    typedef typename CLObj::cl_type CLType;
    clobj_t *const m_ret;
    CLType m_clobj;
    cl_int (*m_release)(CLType);
    const char *m_name;
    std::tuple<T...> m_t1;
    template<int... S>
    PYOPENCL_INLINE CLObj*
    __new_obj(seq<S...>)
    {
        return new CLObj(m_clobj, false, std::get<S>(m_t1)...);
    }
public:
    PYOPENCL_INLINE
    _CLObjOutArg(clobj_t *ret, cl_int (*release)(CLType),
                 const char *name, T... t1)
        : m_ret(ret), m_clobj(nullptr), m_release(release),
          m_name(name), m_t1(t1...)
    {
    }
    PYOPENCL_INLINE
    _CLObjOutArg(_CLObjOutArg<CLObj, T...> &&other)
        : m_ret(other.m_ret), m_clobj(other.m_clobj),
          m_release(other.m_release), m_name(other.m_name)
    {
        std::swap(m_t1, other.m_t1);
    }
    PYOPENCL_INLINE typename CLObj::cl_type*
    get()
    {
        return &m_clobj;
    }
    PYOPENCL_INLINE void
    finish()
    {
        *m_ret = __new_obj(typename gens<sizeof...(T)>::type());
    }
    PYOPENCL_INLINE void
    cleanup(bool finished)
    {
        if (finished) {
            delete *m_ret;
            *m_ret = nullptr;
        } else {
            call_guarded_cleanup(m_release, m_name, m_clobj);
        }
    }
};

template<typename CLObj, typename... T>
static PYOPENCL_INLINE _CLObjOutArg<CLObj, T...>
make_cloutarg(clobj_t *ret, cl_int (*release)(typename CLObj::cl_type),
              const char *name, T... t1)
{
    return _CLObjOutArg<CLObj, T...>(ret, release, name, t1...);
}
#define pyopencl_outarg(type, ret, func, args...)               \
    pyopencl::make_cloutarg<type>(ret, func, #func, ##args)

// {{{ extension function pointers

#if PYOPENCL_CL_VERSION >= 0x1020
template<typename T>
PYOPENCL_USE_RESULT static PYOPENCL_INLINE T
get_ext_fun(cl_platform_id plat, const char *name, const char *err)
{
    T func = (T)clGetExtensionFunctionAddressForPlatform(plat, name);
    if (!func) {
        throw pyopencl::clerror(name, CL_INVALID_VALUE, err);
    }
    return func;
}
#define pyopencl_get_ext_fun(plat, name)                                \
    pyopencl::get_ext_fun<name##_fn>(plat, #name, #name " not available")
#else
template<typename T>
PYOPENCL_USE_RESULT static PYOPENCL_INLINE T
get_ext_fun(const char *name, const char *err)
{
    T func = (T)clGetExtensionFunctionAddress(name);
    if (!func) {
        throw pyopencl::clerror(name, CL_INVALID_VALUE, err);
    }
    return func;
}
#define pyopencl_get_ext_fun(plat, name)                                \
    pyopencl::get_ext_fun<name##_fn>(#name, #name " not available")
#endif

// }}}

}

#endif
