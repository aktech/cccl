//===----------------------------------------------------------------------===//
//
// Part of the CUDA Toolkit, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.
//
//===----------------------------------------------------------------------===//

#ifndef __CUDAX__CONTAINERS_UNINITIALIZED_BUFFER_H
#define __CUDAX__CONTAINERS_UNINITIALIZED_BUFFER_H

#include <cuda/std/detail/__config>

#if defined(_CCCL_IMPLICIT_SYSTEM_HEADER_GCC)
#  pragma GCC system_header
#elif defined(_CCCL_IMPLICIT_SYSTEM_HEADER_CLANG)
#  pragma clang system_header
#elif defined(_CCCL_IMPLICIT_SYSTEM_HEADER_MSVC)
#  pragma system_header
#endif // no system header

#include <cuda/__memory_resource/properties.h>
#include <cuda/__memory_resource/resource_ref.h>
#include <cuda/std/__concepts/_One_of.h>
#include <cuda/std/__memory/align.h>
#include <cuda/std/__new/launder.h>
#include <cuda/std/__utility/swap.h>

#if _CCCL_STD_VER >= 2014 && !defined(_CCCL_COMPILER_MSVC_2017) \
  && defined(LIBCUDACXX_ENABLE_EXPERIMENTAL_MEMORY_RESOURCE)

//! @file The \c uninitialized_buffer class provides a typed buffer allocated from a given memory resource.
namespace cuda::experimental
{

//! @rst
//! .. _cudax-containers-uninitialized-buffer:
//!
//! Uninitialized type safe memory storage
//! ---------------------------------------
//!
//! ``uninitialized_buffer`` provides a typed buffer allocated from a given :ref:`memory resource
//! <libcudacxx-extended-api-memory-resources-resource>`. It handles alignment and release of the allocation.
//! The memory is uninitialized, so that a user needs to ensure elements are properly constructed.
//!
//! In addition to being type safe, ``uninitialized_buffer`` also takes a set of :ref:`properties
//! <libcudacxx-extended-api-memory-resources-properties>` to ensure that e.g. execution space constraints are checked
//! at compile time. However, we can only forward stateless properties. If a user wants to use a stateful one, then they
//! need to implement :ref:`get_property(const device_buffer&, Property)
//! <libcudacxx-extended-api-memory-resources-properties>`.
//!
//! .. warning::
//!
//!    ``uninitialized_buffer`` stores a reference to the provided memory :ref:`memory resource
//!    <libcudacxx-extended-api-memory-resources-resource>`. It is the user's resposibility to ensure the lifetime of
//!    the resource exceeds the lifetime of the buffer.
//!
//! @endrst
//! @tparam _T the type to be stored in the buffer
//! @tparam _Properties... The properties the allocated memory satisfies
template <class _Tp, class... _Properties>
class uninitialized_buffer
{
private:
  _CUDA_VMR::resource_ref<_Properties...> __mr_;
  size_t __count_ = 0;
  void* __buf_    = nullptr;

  //! @brief Determines the allocation size given the alignment and size of `T`
  _CCCL_NODISCARD _CCCL_HOST_DEVICE static constexpr size_t __get_allocation_size(const size_t __count) noexcept
  {
    constexpr size_t __alignment = alignof(_Tp);
    return (__count * sizeof(_Tp) + (__alignment - 1)) & ~(__alignment - 1);
  }

  //! @brief Determines the properly aligned start of the buffer given the alignment and size of  `T`
  _CCCL_NODISCARD _CCCL_HOST_DEVICE _Tp* __get_data() const noexcept
  {
    constexpr size_t __alignment = alignof(_Tp);
    size_t __space               = __get_allocation_size(__count_);
    void* __ptr                  = __buf_;
    return _CUDA_VSTD::launder(
      reinterpret_cast<_Tp*>(_CUDA_VSTD::align(__alignment, __count_ * sizeof(_Tp), __ptr, __space)));
  }

public:
  using value_type = _Tp;
  using reference  = _Tp&;
  using pointer    = _Tp*;
  using size_type  = size_t;

  //! @brief Constructs a \c uninitialized_buffer, allocating sufficient storage for \p __count elements through \p __mr
  //! @param __mr The memory resource to allocate the buffer with.
  //! @param __count The desired size of the buffer.
  //! @note Depending on the alignment requirements of `T` the size of the underlying allocation might be larger
  //! than `count * sizeof(T)`.
  //! @note Only allocates memory when \p __count > 0
  uninitialized_buffer(_CUDA_VMR::resource_ref<_Properties...> __mr, const size_t __count)
      : __mr_(__mr)
      , __count_(__count)
      , __buf_(__count_ == 0 ? nullptr : __mr_.allocate(__get_allocation_size(__count_)))
  {}

  uninitialized_buffer(const uninitialized_buffer&)            = delete;
  uninitialized_buffer& operator=(const uninitialized_buffer&) = delete;

  //! @brief Move construction
  //! @param __other Another \c uninitialized_buffer
  uninitialized_buffer(uninitialized_buffer&& __other) noexcept
      : __mr_(__other.__mr_)
      , __count_(__other.__count_)
      , __buf_(__other.__buf_)
  {
    __other.__count_ = 0;
    __other.__buf_   = nullptr;
  }

  //! @brief Move assignment
  //! @param __other Another \c uninitialized_buffer
  uninitialized_buffer& operator=(uninitialized_buffer&& __other) noexcept
  {
    if (__buf_)
    {
      __mr_.deallocate(__buf_, __get_allocation_size(__count_));
    }
    __mr_            = __other.__mr_;
    __count_         = __other.__count_;
    __buf_           = __other.__buf_;
    __other.__count_ = 0;
    __other.__buf_   = nullptr;
    return *this;
  }

  //! @brief Destroys an \c uninitialized_buffer deallocating the buffer
  //! @warning The destructor does not destroy any objects that may or may not reside within the buffer. It is the
  //! user's responsibility to ensure that all objects within the buffer have been properly destroyed.
  ~uninitialized_buffer()
  {
    if (__buf_)
    {
      __mr_.deallocate(__buf_, __get_allocation_size(__count_));
    }
  }

  //! @brief Returns an aligned pointer to the buffer
  _CCCL_NODISCARD _CCCL_HOST_DEVICE pointer begin() const noexcept
  {
    return __get_data();
  }

  //! @brief Returns an aligned pointer to end of the buffer
  _CCCL_NODISCARD _CCCL_HOST_DEVICE pointer end() const noexcept
  {
    return __get_data() + __count_;
  }

  //! @brief Returns an aligned pointer to the buffer
  _CCCL_NODISCARD _CCCL_HOST_DEVICE pointer data() const noexcept
  {
    return __get_data();
  }

  //! @brief Returns the size of the buffer
  _CCCL_NODISCARD _CCCL_HOST_DEVICE constexpr size_type size() const noexcept
  {
    return __count_;
  }

  //! @rst
  //! Returns the :ref:`resource_ref <libcudacxx-extended-api-memory-resources-resource-ref>` used to allocate
  //! the buffer
  //! @endrst
  _CCCL_NODISCARD _CCCL_HOST_DEVICE _CUDA_VMR::resource_ref<_Properties...> resource() const noexcept
  {
    return __mr_;
  }

  //! @brief Swaps the contents with those of another \c uninitialized_buffer
  //! @param __other The other \c uninitialized_buffer.
  _CCCL_HOST_DEVICE constexpr void swap(uninitialized_buffer& __other) noexcept
  {
    _CUDA_VSTD::swap(__mr_, __other.__mr_);
    _CUDA_VSTD::swap(__count_, __other.__count_);
    _CUDA_VSTD::swap(__buf_, __other.__buf_);
  }

#  ifndef DOXYGEN_SHOULD_SKIP_THIS // friend functions are currently brocken
  //! @brief Forwards the passed Properties
  _LIBCUDACXX_TEMPLATE(class _Property)
  _LIBCUDACXX_REQUIRES((!property_with_value<_Property>) _LIBCUDACXX_AND _CUDA_VSTD::_One_of<_Property, _Properties...>)
  friend constexpr void get_property(const uninitialized_buffer&, _Property) noexcept {}
#  endif // DOXYGEN_SHOULD_SKIP_THIS
};

template <class _Tp>
using uninitialized_device_buffer = uninitialized_buffer<_Tp, _CUDA_VMR::device_accessible>;

} // namespace cuda::experimental

#endif // _CCCL_STD_VER >= 2014 && !_CCCL_COMPILER_MSVC_2017 && LIBCUDACXX_ENABLE_EXPERIMENTAL_MEMORY_RESOURCE

#endif //__CUDAX__CONTAINERS_UNINITIALIZED_BUFFER_H
