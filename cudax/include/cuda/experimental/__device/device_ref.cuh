//===----------------------------------------------------------------------===//
//
// Part of CUDA Experimental in CUDA C++ Core Libraries,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.
//
//===----------------------------------------------------------------------===//

#ifndef _CUDAX__DEVICE_DEVICE_REF
#define _CUDAX__DEVICE_DEVICE_REF

#include <cuda/__cccl_config>

#if defined(_CCCL_IMPLICIT_SYSTEM_HEADER_GCC)
#  pragma GCC system_header
#elif defined(_CCCL_IMPLICIT_SYSTEM_HEADER_CLANG)
#  pragma clang system_header
#elif defined(_CCCL_IMPLICIT_SYSTEM_HEADER_MSVC)
#  pragma system_header
#endif // no system header

#include <cuda/std/__cuda/api_wrapper.h>
#include <cuda/std/__type_traits/decay.h>

namespace cuda::experimental
{
class device;

//! @brief A non-owning representation of a CUDA device
class device_ref
{
  friend class device;

  int __id_ = 0;

  template <::cudaDeviceAttr _Attr>
  struct __attr
  {
    using type = int;

    _CCCL_NODISCARD constexpr operator ::cudaDeviceAttr() const noexcept
    {
      return _Attr;
    }

    _CCCL_NODISCARD type operator()(device_ref __dev) const
    {
      return __dev.attr<_Attr>();
    }
  };

public:
  struct attrs;

  //! @brief For a given attribute, returns the type of the attribute value.
  //!
  //! @par Example
  //! @code
  //! using threads_per_block_t = device_ref::attr_result_t<device_ref::attrs::max_threads_per_block>;
  //! static_assert(std::is_same_v<threads_per_block_t, int>);
  //! @endcode
  //!
  //! @sa device_ref::attrs
  template <::cudaDeviceAttr _Attr>
  using attr_result_t = typename __attr<_Attr>::type;

  //! @brief Create a `device_ref` object from a native device ordinal.
  /*implicit*/ constexpr device_ref(int __id) noexcept
      : __id_(__id)
  {}

  //! @brief Retrieve the native ordinal of the `device_ref`
  //!
  //! @return int The native device ordinal held by the `device_ref` object
  _CCCL_NODISCARD constexpr int get() const noexcept
  {
    return __id_;
  }

  //! @brief Retrieve the specified attribute for the `device_ref`
  //!
  //! @param __attr The attribute to query. See `device_ref::attrs` for the available
  //!        attributes.
  //!
  //! @throws cuda_error if the attribute query fails
  //!
  //! @sa device_ref::attrs
  template <::cudaDeviceAttr _Attr>
  _CCCL_NODISCARD auto attr([[maybe_unused]] device_ref::__attr<_Attr> __attr) const
  {
    int __value = 0;
    _CCCL_TRY_CUDA_API(::cudaDeviceGetAttribute, "failed to get device attribute", &__value, _Attr, get());
    return static_cast<typename device_ref::__attr<_Attr>::type>(__value);
  }

  //! @overload
  template <::cudaDeviceAttr _Attr>
  _CCCL_NODISCARD auto attr() const
  {
    return attr(__attr<_Attr>());
  }
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS // Do not document

//! @brief RAII helper which saves the current device and switches to the
//!        specified device on construction and switches to the saved device on
//!        destruction.
//!
struct __scoped_device
{
private:
  // The original device ordinal, or -1 if the device was not changed.
  int const __old_device;

  //! @brief Returns the current device ordinal.
  //!
  //! @throws cuda_error if the device query fails.
  static int __current_device()
  {
    int device = -1;
    _CCCL_TRY_CUDA_API(cudaGetDevice, "failed to get the current device", &device);
    return device;
  }

  explicit __scoped_device(int new_device, int old_device) noexcept
      : __old_device(new_device == old_device ? -1 : old_device)
  {}

public:
  //! @brief Construct a new `__scoped_device` object and switch to the specified
  //!        device.
  //!
  //! @param new_device The device to switch to
  //!
  //! @throws cuda_error if the device switch fails
  explicit __scoped_device(device_ref new_device)
      : __scoped_device(new_device.get(), __current_device())
  {
    if (__old_device != -1)
    {
      _CCCL_TRY_CUDA_API(cudaSetDevice, "failed to set the current device", new_device.get());
    }
  }

  __scoped_device(__scoped_device&&)                 = delete;
  __scoped_device(__scoped_device const&)            = delete;
  __scoped_device& operator=(__scoped_device&&)      = delete;
  __scoped_device& operator=(__scoped_device const&) = delete;

  //! @brief Destroy the `__scoped_device` object and switch back to the original
  //!        device.
  //!
  //! @throws cuda_error if the device switch fails. If the destructor is called
  //!         during stack unwinding, the program is automatically terminated.
  ~__scoped_device() noexcept(false)
  {
    if (__old_device != -1)
    {
      _CCCL_TRY_CUDA_API(cudaSetDevice, "failed to restore the current device", __old_device);
    }
  }
};

#endif // DOXYGEN_SHOULD_SKIP_THIS

} // namespace cuda::experimental

#endif // _CUDAX__DEVICE_DEVICE_REF
